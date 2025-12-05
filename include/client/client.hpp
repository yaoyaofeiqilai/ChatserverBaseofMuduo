#ifndef CLINET_HPP
#define CLINET_HPP

#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>
#include<unordered_map>
#include "group.hpp"
#include "user.hpp"
#include "public.hpp"
#include <semaphore.h>
#include <atomic>
#include "keyguard.hpp"
// 记录当前系统登录的用户信息
extern User currentUser;
// 记录当前登录用户的好友列表信息
extern vector<User> currentUserFriendList;
// 记录当前登录用户的群组列表信息
extern vector<Group> currentUserGroupList;
//客户端支持的命令
extern unordered_map<string, string> commandMap;
//客户端命令处理函数
extern unordered_map<string, function<void(int, string)>> commandHandlerMap;

extern atomic_bool userIsLogin;

//读写线程通信的信号量
extern sem_t acksem;


// 显示当前登录成功用户的基本信息
void showCurrentUserData();

//接收线程，由于接收服务端发送的消息
//mian线程用作于写线程，即发送消息内容
//这样就可以同时接收和发送消息，不会出现发消息(阻塞)导致无法接收消息的情况
void readTaskHandler(int clientfd);

// 客户端登录业务
void login(int& clientfd);
//loginReation处理服务器发回来的登录响应
void loginReaction(json response);
//客户端注册业务
void reg(int& clientfd);

void regReation(json );
//获取系统时间
string getCurrentTime();

// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);
//保存RSA密钥并发送AES密钥给服务器
void keyHandler(int& clientfd ,json& js);

//封装加密方法
string msgEncryption(string & str);
//聊天菜单
void mainMenu(int);
#endif