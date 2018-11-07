#pragma once

#include <string>

using namespace std;

struct User {
	int log;		//指示用户的登录状态，0-还没输入USER命令，1-还没输入PASS命令，>=2已登录
	bool ispassive;	//指示服务器的状态，true-被动状态，false-主动状态
	string username;//用户名
	string password;//用户密码
	string workdir;	//用户的工作目录
};