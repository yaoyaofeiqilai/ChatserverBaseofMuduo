//利用muduo组合的server类的头文件
#ifndef CHATSERVER_HPP
#define CHATSERVER_HPP
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include "muduo/base/Logging.h"
#include <atomic>
using namespace muduo;
using namespace muduo::net;

//聊天服务器的类型声明
class ChatServer 
{
    public:
    ChatServer(EventLoop* loop,const InetAddress& listenAddr,const string& nameArg);
    void start();
    private:
    //连接和读写回调函数，用于绑定事件发生的回调函数
    void ServerConnFunc(const TcpConnectionPtr&);
    void ServerMessFunc(const TcpConnectionPtr&,Buffer*,Timestamp);
    TcpServer server_;
    EventLoop* loop_;      //相当于epoll

    std::atomic<int> totalconnectedCount_;
};
#endif