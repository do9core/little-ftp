#pragma comment(lib,"ws2_32.lib")
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include "FTPCmd.h"

using namespace std;

SOCKET init(string);	//舒适化控制端socket
void breakMsg(string, int*, string*);	//分解信息，取得响应码和响应消息
string recieve(int*, string*);	//接收响应码和响应消息
void checkCmd(string, int*, string*);	//检查命令，执行命令流程

SOCKET control_socket;	//控制端socket
SOCKET data_socket;	//数据端socket
bool ispassive;	//是否处于被动模式
int passivePort = -1;	//被动模式下，服务器的端口号
ULONG passiveAddress = -1;	//被动模式下，服务器的地址

int main(int num, char* args[]) {
	WORD version = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(version, &wsaData) != 0) {
		cout << "[ERROR]socket error!" << endl;
		return INVALID_SOCKET;
	}

	if (num < 2) {
		cout << "usage: ftp <server>[:21]" << endl;
		system("pause");
		return 0;
	}

	control_socket = init(args[1]);
	//初始化控制端socket，连接到目标ip的主机上，默认控制端口21
	if (control_socket == INVALID_SOCKET) {
		cout << "[ERROR]init failed!" << endl;
		return 1;
	}

	int code;
	string text;
	recieve(&code, &text);

	cout << "ftp>";
	string msg;
	getline(cin, msg);
	//获取输入
	while (true) {
		checkCmd(msg, &code, &text); //执行命令，取得响应
		//如果命令为QUIT，且响应200，成功退出
		if (msg.compare(QUIT) == 0 && code == 200) {
			break;
		}

		cout << "ftp>";
		getline(cin, msg);
	}

	//关闭socket
	closesocket(control_socket);
	closesocket(data_socket);
	WSACleanup();
	system("pause");
	return 0;
}

string recieve(int* code, string* text) {
	//接收响应，最大1024字节
	char* recvArray = new char[1024];
	int ret = recv(control_socket, recvArray, 1024, 0);
	if (ret > 0) {
		recvArray[ret] = 0x00;
		string recvStr = recvArray;
		breakMsg(recvStr, code, text);

		cout << "code:" << *code << endl
			<< "message:" << *text << endl;
		return recvStr;
	}

	*code = -1;
	text->clear();
	text->append("[ERROR]cannot reach remote server.");

	cout << "code:" << *code << endl
		<< "message:" << *text << endl;
	return "";
}

void breakMsg(string msg, int* code, string* text) {
	if (msg.length() < 3) {
		*code = -1;
		text->clear();
		text->append("error");
		return;
	}

	int spaceIndex = msg.find(' ');

	if (spaceIndex < 0) {
		spaceIndex = msg.length();
	}

	*code = 0;
	for (int i = 0; i < spaceIndex; i++) {
		*code = *code * 10 + msg[i] - '0';
	}

	if (spaceIndex == msg.length()) {
		text->clear();
		return;
	}

	text->clear();
	*text = msg.substr(spaceIndex + 1, msg.length());
}

SOCKET init(string ipaddress) {
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		cout << "[ERROR]socket error!" << endl;
		return INVALID_SOCKET;
	}

	//初始化服务器地址和控制端口信息
	SOCKADDR_IN addr;
	ULONG ipv4addr;
	InetPton(AF_INET, ipaddress.c_str(), &ipv4addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(21);
	addr.sin_addr.S_un.S_addr = ipv4addr;

	if (connect(sock, (SOCKADDR*)&addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		//连接失败
		cout << "[ERROR]connect error!" << endl;
		return INVALID_SOCKET;
	}

	return sock;
}

void checkCmd(string cmd, int* code, string* text) {
	//裁剪命令，检查命令名和是否带参
	string subCmd;
	int spaceIndex = cmd.find(' ');
	if (spaceIndex < 0) {
		subCmd = string(cmd);
	}
	else {
		subCmd = cmd.substr(0, spaceIndex);
	}

	/* 需要对PORT PASV RETR STOR几个命令进行特殊处理
	   因为它们的流程与其他命令有所区别，并非一应一答 */
	if (subCmd.compare(PORT) == 0) {
		//主动模式，本地开放端口
		string param = cmd.substr(spaceIndex + 1, cmd.length());

		int port;
		{
			int i = 0;
			for (int j = 0; i < param.length(); i++) {
				if (param[i] == ',' && ++j == 4) {
					break;
				}
			}

			int port_base = 0;
			for (++i; param[i] != ',' && i < param.length(); i++) {
				port_base = port_base * 10 + param[i] - '0';
			}
			port_base <<= 8;
			int port_offset = 0;
			for (++i; param[i] != ',' && i < param.length(); i++) {
				port_offset = port_offset * 10 + param[i] - '0';
			}
			port = port_base + port_offset;
		}

		//开启监听socket
		SOCKET dataListener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		SOCKADDR_IN addr;
		int len = sizeof(SOCKADDR);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
		cout << "[INFO]PORT bind: " << port << endl;
		//绑定到端口
		if (bind(dataListener, (SOCKADDR*)&addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
			*code = -1;
			*text = "[ERROR]bind failed!";
			return;
		}
		//开始监听
		if (listen(dataListener, 1) == SOCKET_ERROR) {
			*code = -1;
			*text = "[ERROR]listen failed!";
			return;
		}
		//告知服务器本地开放的端口，默认15361
		send(control_socket, cmd.c_str(), strlen(cmd.c_str()), 0);
		//等待连接
		data_socket = accept(dataListener, (SOCKADDR*)&addr, &len);
		if (data_socket == INVALID_SOCKET) {
			*code = -1;
			*text = "[ERROR]data socket accept failed.";
			return;
		}

		recieve(code, text);
		if (*code == 200) {
			ispassive = false;	//切换到主动模式
		}
	}
	else if (subCmd.compare(PASV) == 0) {
		//被动模式，向服务器请求开放的端口
		send(control_socket, cmd.c_str(), strlen(cmd.c_str()), 0);
		//取得开放的端口
		recieve(code, text);

		string ip = "";
		int port;
		{
			int i = 0;
			for (int j = 0; i < text->length(); i++) {
				if ((*text)[i] == ',') {
					if (++j == 4) {
						break;
					}
					ip = ip + ".";
				}
				else {
					ip = ip + (*text)[i];
				}
			}

			int port_base = 0;
			for (++i; i < text->length() && (*text)[i] != ','; i++) {
				port_base = port_base * 10 + (*text)[i] - '0';
			}
			port_base <<= 8;
			int port_offset = 0;
			for (++i; i < (*text).length(); i++) {
				port_offset = port_offset * 10 + (*text)[i] - '0';
			}
			port = port_base + port_offset;
		}

		cout << "[INFO]passive port: " << port << endl;
		//记录端口和地址信息
		//用于传输数据时，建立socket连接
		passivePort = port;
		InetPton(AF_INET, ip.c_str(), &passiveAddress);

		ispassive = true;	//进入被动模式
	}
	else if (subCmd.compare(RETR) == 0) {
		send(control_socket, cmd.c_str(), strlen(cmd.c_str()), 0);
		recieve(code, text);
		//收到150响应，表示服务器已准备接收数据连接
		if (*code == 150) {
			if (ispassive) {
				//如果处于被动模式，则建立数据端socket连接
				data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

				SOCKADDR_IN addr;
				addr.sin_family = AF_INET;
				addr.sin_port = htons(passivePort);
				addr.sin_addr.S_un.S_addr = passiveAddress;

				connect(data_socket, (SOCKADDR*)&addr, sizeof(SOCKADDR));
				cout << "[INFO]passive data socket connected." << endl;
			}

			char* buffer = new char[1024];
			int ret = recv(data_socket, buffer, 1024, 0);
			//传输完毕时会收到服务器发来的226响应
			recieve(code, text);
			if (ret > 0) {
				buffer[ret] = 0x00;
				int spaceIndex = cmd.find(' ');
				string filename = cmd.substr(spaceIndex + 1, cmd.length());
				ofstream out(filename);
				if (!out) {
					cout << "[ERROR]out file stream created failed." << endl;
				}
				else {
					out << buffer;
					cout << "[INFO]out file write finished." << endl;
				}
				out.close();
			}
			delete buffer;

			if (ispassive) {
				//如果处于被动模式下，结束时关闭数据端socket
				closesocket(data_socket);
				cout << "[INFO]passive data socket closed." << endl;
			}
		}
		return;
	}
	else if (subCmd.compare(STOR) == 0) {
		//存储文件到服务器
		send(control_socket, cmd.c_str(), strlen(cmd.c_str()), 0);
		recieve(code, text);
		//如果收到150响应，表示服务器已准备好接收数据
		if (*code == 150) {
			if (ispassive) {
				//如果处于被动模式，建立数据端socket连接
				data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

				SOCKADDR_IN addr;
				addr.sin_family = AF_INET;
				addr.sin_port = htons(passivePort);
				addr.sin_addr.S_un.S_addr = passiveAddress;

				connect(data_socket, (SOCKADDR*)&addr, sizeof(SOCKADDR));
				cout << "[INFO]passive data socket connected." << endl;
			}

			string filename = cmd.substr(5, cmd.length());
			string fileText = "";
			ifstream in(filename);
			if (!in) {
				cout << "[ERROR]input file stream created failed." << endl;
			}
			else {
				in >> fileText;
			}
			in.close();
			send(data_socket, fileText.c_str(), fileText.length(), 0);
			//传输完毕时会收到服务端的226响应
			recieve(code, text);

			if (ispassive) {
				//被动模式下，关闭数据端socket
				closesocket(data_socket);
				cout << "[INFO]passive data socket closed." << endl;
			}
		}
		return;
	}
	else {
		//其他命令均为一应一答
		//发送消息后等待回应即可
		send(control_socket, cmd.c_str(), strlen(cmd.c_str()), 0);
		recieve(code, text);
	}
}