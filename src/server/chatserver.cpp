//ChatServer类的定义
#include "chatserver.hpp"
#include <functional>
#include <iostream>
#include <string>
#include "json.hpp"
#include <string>
#include "chatservice.hpp"
#include "public.hpp"
using json =nlohmann::json;
using namespace std::placeholders;
//初始化构造函数，同时设置事件的回调函数
ChatServer::ChatServer(EventLoop* loop,const InetAddress& listenAddr,const string& nameArg)
:server_(loop,listenAddr,nameArg)
,loop_(loop)
{
    //连接断开事件的回调函数，当事件发生时muduo网络库会调用该函，自动传入一个TcpConnectionPtr类型的参数
    //这也意味着回调函数签名必须为--void(const TcpConnectionPtr&)--
    server_.setConnectionCallback(std::bind(&ChatServer::ServerConnFunc,this,_1));
    //读写事件对调函数，muduo网络库会调用该函数，并传入TcpConnectionPtr，Buffer，Timestamp类型的参数
    //这也意味着回调函数签名必须为--void(const TcpConnectionPtr&,Buffer*,Timestamp)--
    server_.setMessageCallback(std::bind(&ChatServer::ServerMessFunc,this,_1,_2,_3));
    //设置服务器线程
    server_.setThreadNum(4);
}

void ChatServer::start()
{
    server_.start();
    //生成公私钥
      KeyGuard* keyguard=KeyGuard::GetInstance();
      keyguard->generateRSAkey();

}

//处理连接事件的函数定义
void ChatServer::ServerConnFunc(const TcpConnectionPtr&conn)
{
   if(!conn->connected())  //关闭连接的代码
   {
      ChatService::instance()->closeException(conn);  //设置用户状态函数
      conn->shutdown();
      return ;
   }

    //初始化信息，发送rsa公钥钥
   KeyGuard* keyguard=KeyGuard::GetInstance();
   string publickey;
   keyguard->getPublickey(publickey);
   json RSAjs;
   RSAjs["msgid"]=RAS_KEY_MSG;
   RSAjs["publickey"]=publickey;
   string rsaStr=RSAjs.dump();
   conn->send(rsaStr);
}

//处理信息读写事件的函数定义
void ChatServer::ServerMessFunc(const TcpConnectionPtr&conn,Buffer*buffer,Timestamp time)
{
   //将缓冲区的信息转为字符串
   string text=buffer->retrieveAllAsString();
   //解密字符串
   KeyGuard* keyguard=KeyGuard::GetInstance();
   string key;
   if(keyguard->findAESkey(conn,key))
   {
      text=keyguard->MsgAESDecrypt(key,text);
   }
   else
   {
      //第一次连接，使用RSA解密AES密钥
      string prikey;
      keyguard->getPrivatekey(prikey);
      text=keyguard->RsaDecrypt(prikey,text);
   }
   //将字符串内容反序列化为json
   json js=json::parse(text);
   //目的：将业务模块代码与网络模块的代码解耦
   //放回信息类型的处理函数
   MsgHandler msghandler=ChatService::instance()->getHandler(js["msgid"].get<int>());
   //调用消息处理函数，进行业务处理
   msghandler(conn,js,time);
}
