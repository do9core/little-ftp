#pragma once

#include <string>

using namespace std;

struct User {
	int log;		//ָʾ�û��ĵ�¼״̬��0-��û����USER���1-��û����PASS���>=2�ѵ�¼
	bool ispassive;	//ָʾ��������״̬��true-����״̬��false-����״̬
	string username;//�û���
	string password;//�û�����
	string workdir;	//�û��Ĺ���Ŀ¼
};