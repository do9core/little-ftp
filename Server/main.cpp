#pragma comment(lib,"ws2_32.lib")
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include <io.h>
#include "User.h"
#include "FTPCmd.h"

#define BASE_DIR "D:\\ftpserver"
#define BUFFER_SIZE 1024

using namespace std;

char* buffer = new char[BUFFER_SIZE];	//传输文件用的buffer
SOCKET control_socket;	//控制端的socket
SOCKET data_socket_listener;	//被动模式的数据端socket的监听socket
SOCKET data_socket;		//数据端socket

SOCKET init();	//初始化控制端socket
void checkCmd(SOCKET*, User*, string);	//检查并执行命令
void sendResp(SOCKET*, string);	//向客户端发送回应
bool TraverseFiles(string, vector<string>&);	//遍历目录，取得的文件放在vector里
int renameFlag = 0;
string renameSource;
string* users;
string* passwords;

int main() {
	WORD version = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(version, &wsaData) != 0) {
		cout << "[ERROR]socket error!" << endl;
		return INVALID_SOCKET;
	}

	control_socket = init();	//初始化控制端socket
	if (control_socket == INVALID_SOCKET) {
		//初始化失败，返回
		cout << "[ERROR]init failed!" << endl;
		return 1;
	}

	users = new string[5];
	passwords = new string[5];

	users[0] = "admin\r\n";
	passwords[0] = "admin\r\n";

	users[1] = "testuser";
	passwords[1] = "testpass";

	while (true) {
		SOCKADDR_IN clientAddr;
		int len = sizeof(SOCKADDR);
		SOCKET client = accept(control_socket, (SOCKADDR*)&clientAddr, &len);
		//接收一个客户端socket，accept会阻塞进程，直到有连接到来
		if (client == INVALID_SOCKET) {
			//接收错误，继续等待接收
			cout << "[ERROR]accept error, passed." << endl;
			continue;
		}
		sendResp(&client, "220 Connection established. FTP Server response. Waiting USER or ACCT.");

		cout << "[INFO]accept client." << endl;
		//成功接收，初始化用户
		User user;
		user.log = 0;
		user.workdir = "\\";

		for (; true;) {	//不断处理用户发来的命令
			string recvCmd;
			char* recvArray = new char[1024];
			int ret = recv(client, recvArray, 1024, 0);
			if (ret > 0) {
				recvArray[ret] = 0x00;
				recvCmd = recvArray;
				cout << "[INFO]cmd recieved: " + recvCmd << endl;
				checkCmd(&client, &user, recvCmd);	//处理命令
			}
			delete recvArray;
			if (recvCmd.compare(QUIT) == 0) {
				//收到QUIT时，结束接收此用户的命令
				break;
			}	
		}

		//关闭使用到的socket，释放端口，以便后续使用
		closesocket(data_socket);
		closesocket(data_socket_listener);
		closesocket(client);
		cout << "[INFO]client quit." << endl;
		string check;
		cout << "continue?(y/n):";
		cin >> check;
		if (check[0] == 'N' || check[0] == 'n') {
			break;
		}
	}

	delete users;
	delete passwords;
	delete buffer;
	WSACleanup();
	system("pause");
	return 0;
}

SOCKET init() {
	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == INVALID_SOCKET) {
		cout << "[ERROR]invliad socket error!" << endl;
		return INVALID_SOCKET;
	}

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(21);
	addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (bind(listener, (SOCKADDR*)&addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "[ERROR]bind error!" << endl;
		return INVALID_SOCKET;
	}

	if (listen(listener, 5) == SOCKET_ERROR) {
		cout << "[ERROR]listen error!" << endl;
		return INVALID_SOCKET;
	}
	cout << "[INFO]listening..." << endl;

	return listener;
}

void sendResp(SOCKET* client, string msg) {
	msg = msg + "\r\n";
	send(*client, msg.c_str(), msg.length(), 0);
}

void checkCmd(SOCKET* client, User* user, string cmd) {

	if (cmd.compare(QUIT) == 0) {
		//发送200后结束服务
		sendResp(client, "200 Bye~");
		return;
	}

	if (cmd.compare(REIN) == 0) {
		user->log = 0;
		user->workdir = "\\";
		closesocket(data_socket_listener);
		closesocket(data_socket);
		sendResp(client, "220 Reinitialize finished.");
		return;
	}

	if (user->log == 0) {
		//要求用户输入用户名
		string c = cmd.substr(0, 4);
		if (c.compare(USER) == 0 || c.compare(ACCT) == 0) {
			user->username = cmd.substr(5, cmd.length());
			user->log = 1;
			sendResp(client, "331 USER command OK, password required.");
			return;
		}
		sendResp(client, "501 USER command syntax error.");
		return;
	}
	else if (user->log == 1) {
		//要求用户输入密码
		string c = cmd.substr(0, 4);
		if (c.compare(PASS) == 0) {
			user->password = cmd.substr(5, cmd.length());

			int userIndex = -1;
			for (int i = 0; i < users->length(); i++) {
				if (user->username.compare(users[i]) == 0) {
					userIndex = i;
					break;
				}
			}

			if (userIndex < 0) {
				sendResp(client, "530 User not exists.");
				return;
			}

			if (user->password.compare(passwords[userIndex]) == 0) {
				user->log = 2;
				sendResp(client, "230 User log in success.");
				return;
			}

			sendResp(client, "530 User password incorrect.");
			return;
		}
		sendResp(client, "530 User name or password incorrect.");
		return;
	}
	else
	{
		int spaceIndex = cmd.find(' ');
		//识别命令是否带参数
		string c;
		if (spaceIndex < 0) {
			c = string(cmd);
		}
		else {
			c = cmd.substr(0, spaceIndex);
		}

		if (renameFlag < 0) {
			if (c.compare(RNTO) != 0) {
				sendResp(client, "503 RNTO mast after than RNTR.");
				return;
			}

			if (spaceIndex < 0) {
				sendResp(client, "504 No new file name given.");
				return;
			}

			string fileLocal = BASE_DIR + user->workdir + cmd.substr(spaceIndex + 1, cmd.length());
			string newName = cmd.substr(spaceIndex + 1, cmd.length());
			ifstream in(fileLocal);
			if (in) {
				sendResp(client, "553 File already exists.");
				in.close();
				return;
			}

			in.close();
			rename(renameSource.c_str(), newName.c_str());
			sendResp(client, "250 File renamed.");
			renameFlag = 0;
			return;
		}

		if (c.compare(CWD) == 0) {
			if (spaceIndex < 0) {
				sendResp(client, "502 Parameter not allowed. Change dir failed.");
				return;
			}

			string workdir = cmd.substr(spaceIndex + 1, cmd.length());
			string localpath;
			//检查用户是否转到根目录
			if (workdir[0] == '\\' || workdir[0] == '/') {
				localpath = string(BASE_DIR) + workdir;
			}
			else {
				workdir = user->workdir + workdir;
				localpath = string(BASE_DIR) + workdir;
			}
			cout << "[INFO]CWD to " + localpath << endl;
			
			//检查目录是否存在
			struct _stat fileStat;
			if ((_stat(localpath.c_str(), &fileStat) == 0) && (fileStat.st_mode & _S_IFDIR)) {
				user->workdir = workdir;
				if (user->workdir[user->workdir.length() - 1] != '\\'
					&& user->workdir[user->workdir.length() - 1] != '/') {
					user->workdir = user->workdir + "\\";
				}
				sendResp(client, "250 Sucessfully changed dir.");
				return;
			}

			sendResp(client, "550 Directory not exist.");
			return;
		}
		else if (c.compare(PWD) == 0) {
			//返回当前工作目录
			sendResp(client, "257 " + user->workdir);
			return;
		}
		else if (c.compare(PASV) == 0) {
			//进入被动模式
			//生成一个数据端socket的监听socket
			data_socket_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (data_socket_listener == INVALID_SOCKET) {
				cout << "[ERROR]cannot create data socket listner." << endl;
				sendResp(client, "425 Cannot build data stream.");
				return;
			}

			int port = 60 * 256 + 1;
			SOCKADDR_IN addr_data;
			addr_data.sin_family = AF_INET;
			addr_data.sin_port = htons(port);
			addr_data.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
			//绑定到端口（应该是高位随机端口
			cout << "[INFO]bind port: " << port << endl;
			if (bind(data_socket_listener, (SOCKADDR*)&addr_data, sizeof(SOCKADDR)) == SOCKET_ERROR) {
				cout << "[ERROR]data socket listener bind error." << endl;
				sendResp(client, "425 Cannot build data stream.");
				return;
			}
			//监听端口
			if (listen(data_socket_listener, 1) == SOCKET_ERROR) {
				cout << "[ERROR]data socket listener listen error." << endl;
				sendResp(client, "425 Cannot build data stream.");
			}
			//向客户端发送端口信息
			sendResp(client, "227 127,0,0,1,60,01");
			user->ispassive = true;
			return;
		}
		else if (c.compare(PORT) == 0) {
			//进入主动模式
			//取得ip地址和端口参数信息
			string address = cmd.substr(spaceIndex + 1, cmd.length());
			string ip = "";
			int port;
			{
				int i = 0;
				for (int j = 0; i < address.length(); i++) {
					if (address[i] == ',') {
						if (++j == 4) {
							break;
						}
						ip = ip + ".";
					}
					else {
						ip = ip + address[i];
					}
				}

				int port_base = 0;
				for (++i; i < address.length() && address[i] != ','; i++) {
					port_base = port_base * 10 + address[i] - '0';
				}
				port_base <<= 8;
				int port_offset = 0;
				for (++i; i < address.length(); i++) {
					port_offset = port_offset * 10 + address[i] - '0';
				}
				port = port_base + port_offset;
			}

			cout << "[INFO]PORT target: " + ip + "::" << port << endl;
			//创建一个数据端socket
			data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (data_socket == INVALID_SOCKET) {
				cout << "[ERROR]data socket connector build error." << endl;
				sendResp(client, "425 Cannot build data stream.");
				return;
			}

			ULONG ipv4addr;
			InetPton(AF_INET, ip.c_str(), &ipv4addr);
			SOCKADDR_IN addr;
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			addr.sin_addr.S_un.S_addr = ipv4addr;
			//连接客户端的开放端口
			if (connect(data_socket, (SOCKADDR*)&addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
				cout << "[ERROR]data socket connector connect error." << endl;
				sendResp(client, "425 Cannot build data stream.");
				return;
			}

			sendResp(client, "200 PORT command successful.");
			user->ispassive = false;
			return;
		}
		else if (c.compare(LIST) == 0 || c.compare(NLST) == 0) {
			//读取服务器文件列表
			vector<string> files;
			TraverseFiles(BASE_DIR + user->workdir, files);
			//组合信息
			string rtnMsg = "226 ";
			for (int i = 0; i < files.size(); i++) {
				rtnMsg = rtnMsg + files[i] + " ";
			}
			sendResp(client, rtnMsg);
			return;
		}
		else if (c.compare(RETR) == 0) {
			if (spaceIndex < 0) {
				cout << "[ERROR]no file given." << endl;
				sendResp(client, "504 Invalid parameters.");
				return;
			}

			//获取本地文件名
			string filename = BASE_DIR + user->workdir + cmd.substr(spaceIndex + 1, cmd.length());
			cout << "[INFO]RETR file name: " + filename << endl;
			ifstream in(filename);
			if (in) {
				//文件存在，返回150准备发送
				sendResp(client, "150 Open connection for download.");
				if (user->ispassive) {
					SOCKADDR_IN addr;
					int len = sizeof(SOCKADDR);
					//使用被动模式时，取得数据端socket
					data_socket = accept(data_socket_listener, (SOCKADDR*)&addr, &len);
					if (data_socket == INVALID_SOCKET) {
						cout << "[ERROR]cannot accept passive data socket." << endl;
						sendResp(client, "435 cannot create data stream");
						return;
					}
					cout << "[INFO]data socket accepted." << endl;
				}

				string fileText;
				in >> fileText;
				send(data_socket, fileText.c_str(), fileText.length(), 0);
				sendResp(client, "226 Transform completed.");
			}
			else {
				//文件不存在，返回553结束传送
				sendResp(client, "553 File not exists.");
			}
			in.close();
			if (user->ispassive) {
				//使用被动模式时，关闭数据端socket
				closesocket(data_socket);
				cout << "[INFO]passive data socket closed." << endl;
			}
			return;
		}
		else if (c.compare(STOR) == 0) {
			if (spaceIndex < 0) {
				sendResp(client, "504 Invalid arguments.");
				return;
			}

			//获取要存储的文件名
			string filename = BASE_DIR + user->workdir + cmd.substr(spaceIndex + 1, cmd.length());
			cout << "[INFO]STOR file name: " + filename << endl;
			sendResp(client, "150 Open connection for upload.");

			if (user->ispassive) {
				SOCKADDR addr;
				int len = sizeof(SOCKADDR);
				//如果处在被动模式下，需要获取数据端socket
				data_socket = accept(data_socket_listener, (SOCKADDR*)&addr, &len);
				if (data_socket == INVALID_SOCKET) {
					cout << "[ERROR]passive data socket accept error." << endl;
					sendResp(client, "425 Cannot build data stream.");
					return;
				}
				cout << "[INFO]passive data socket accepted." << endl;
			}

			memset(buffer, 0, BUFFER_SIZE);
			int ret = recv(data_socket, buffer, BUFFER_SIZE, 0);
			if (ret > 0) {
				//成功获取数据，写入文件
				buffer[ret] = 0x00;
				ofstream out(filename);
				out << buffer;
				out.close();
				sendResp(client, "226 Transform completed.");
			}
			else {
				//没获取到数据，返回425
				sendResp(client, "425 No data recieved.");
			}

			if (user->ispassive) {
				//被动模式下，关闭数据端socket
				closesocket(data_socket);
				cout << "[INFO]passive data socket closed." << endl;
			}
			return;
		}
		else if (c.compare(DELE) == 0) {
			//删除服务器端文件
			if (spaceIndex < 0) {
				sendResp(client, "504 No file name given.");
				return;
			}

			string filename = BASE_DIR + user->workdir + cmd.substr(spaceIndex + 1, cmd.length());
			ifstream in(filename);
			if (!in) {
				in.close();
				sendResp(client, "553 File not exitsts, no need to delete.");
				return;
			}
			
			in.close();
			remove(filename.c_str());
			cout << "[INFO]DELE file path: " + filename << endl;
			sendResp(client, "250 File deleted.");
			return;
		}
		else if (c.compare(RNFR) == 0) {
			if (spaceIndex < 0) {
				sendResp(client, "504 No file name given.");
			}

			renameSource = BASE_DIR + user->workdir + cmd.substr(spaceIndex + 1, cmd.length());
			ifstream in(renameSource);
			if (!in) {
				sendResp(client, "553 File not exists.");
				in.close();
				return;
			}
			in.close();
			sendResp(client, "200 Target recieved.");
			renameFlag = -1;
			return;
		}
		else if (c.compare(MKD) == 0) {
			//创建目录
			if (spaceIndex < 0) {
				sendResp(client, "503 No dir name given.");
				return;
			}

			string dir = BASE_DIR + user->workdir + cmd.substr(spaceIndex + 1, cmd.length());
			bool success = CreateDirectory(dir.c_str(), NULL);
			if (success) {
				sendResp(client, "257 Directory created.");
				return;
			}
			
			sendResp(client, "553 Directory create failed.");
			return;
		}
		else if (c.compare(RMD)) {
			//删除目录
			if (spaceIndex < 0) {
				sendResp(client, "503 No dir name given.");
				return;
			}

			string dir = BASE_DIR + user->workdir + cmd.substr(spaceIndex + 1, cmd.length());
			bool success = RemoveDirectory(dir.c_str());
			if (success) {
				sendResp(client, "250 Directory removed.");
				return;
			}

			sendResp(client, "553 Directory delete failed.");
			return;
		}

		sendResp(client, "502 Command is not implement.");
		return;
	}
}

bool TraverseFiles(string path, vector<string> &files)
{
	_finddata_t file_info;
	string current_path = path + "*.*";

	int handle = _findfirst(current_path.c_str(), &file_info);
	//返回值为-1则查找失败  
	if (-1 == handle)
		return false;
	do
	{
		//判断是否子目录  
		string attribute;
		if (file_info.attrib == _A_SUBDIR) //是目录  
			attribute = "dir";
		else
			attribute = "file";
		string p;
		files.push_back(p.append(file_info.name).append('<' + attribute + '>'));
	} while (!_findnext(handle, &file_info));  //返回0则遍历完  
											   //关闭文件句柄 

	_findclose(handle);
	return true;
}