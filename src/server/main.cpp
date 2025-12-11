#include "chatserver.hpp"
#include "chatservice.hpp"
#include <string>
#include <iostream>
#include <signal.h>

static string server = "127.0.0.1";
static string user = "root";
static string password = "Lsg20041013.";
static string dbname = "chat";
using namespace std;
int main(int argc, char **argv)
{
     if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(-1);
    }
    string ip = argv[1];
    uint16_t port = atoi(argv[2]);
    signal(SIGINT, ChatService::ServerOffline);
    // 事件循环，作为参数传入server中
    EventLoop loop;
    InetAddress addr(ip, port);

    // 注册服务器类
    ChatServer server(&loop, addr, "ChatServer01");

    // 启动服务器
    server.start();

    // 开始监听
    loop.loop();
    /*
    {"msgid":1,"id":1,"password":"123344"}
    {"msgid":1,"id":2,"password":"123456"}
    {"msgid":7,"id":1,"friendid":2}
     {"msgid":7,"id":1,"friendid":6}
    */

    return 0;
}
