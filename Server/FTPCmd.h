#pragma once

#define ACCT "ACCT" //同USER
#define CWD "CWD"   //<dir> 改变服务器上的工作目录
#define DELE "DELE" //<filename> 删除服务器上的指定文件
#define LIST "LIST" //<name> 列出文件信息或目录内文件列表
#define NLST "NLST" //<dir> 列出指定目录内容
#define MKD "MKD"   //<dir> 在服务器上创建目录
#define PASS "PASS" //<password> 登陆密码
#define PASV "PASV" //请求服务器等待连接
#define PORT "PORT" //<address> 4字节IP地址和2字节端口ID
#define PWD "PWD"   //显示工作目录
#define QUIT "QUIT" //退出登录
#define REIN "REIN" //重新初始化登录状态连接
#define RETR "RETR" //<filename> 从服务器上复制文件
#define RMD "RMD"   //<dir>	从服务器上删除目录
#define RNFR "RNFR" //<path> 重命名路径，后续紧跟RNTO
#define RNTO "RNTO" //<path> 路径命名为，紧跟RNFR
#define STOR "STOR" //<file> 复制文件到服务器上
#define TYPE "TYPE" //<data type> 数据类型 A=ASCII E=EBCDIC I=BINARY
#define USER "USER" //登录用户名
