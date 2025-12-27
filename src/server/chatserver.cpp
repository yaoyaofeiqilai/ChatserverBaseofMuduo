// ChatServer类的定义
#include "chatserver.hpp"
#include <functional>
#include <iostream>
#include <string>
#include "json.hpp"
#include <string>
#include "chatservice.hpp"
#include "public.hpp"
using json = nlohmann::json;
using namespace std::placeholders;
// 初始化构造函数，同时设置事件的回调函数
ChatServer::ChatServer(EventLoop *loop, const InetAddress &listenAddr, const string &nameArg)
    : server_(loop, listenAddr, nameArg), loop_(loop), serverName_(nameArg), totalconnectedCount_(0)
{
   loop_->runEvery(5.0, [this]()
                   { std::cout << "当前连接数: " << this->totalconnectedCount_ << std::endl; });
   // 连接断开事件的回调函数，当事件发生时muduo网络库会调用该函，自动传入一个TcpConnectionPtr类型的参数
   // 这也意味着回调函数签名必须为--void(const TcpConnectionPtr&)--
   server_.setConnectionCallback(std::bind(&ChatServer::ServerConnFunc, this, _1));
   // 读写事件对调函数，muduo网络库会调用该函数，并传入TcpConnectionPtr，Buffer，Timestamp类型的参数
   // 这也意味着回调函数签名必须为--void(const TcpConnectionPtr&,Buffer*,Timestamp)--
   server_.setMessageCallback(std::bind(&ChatServer::ServerMessFunc, this, _1, _2, _3));
   // 设置服务器线程
   server_.setThreadNum(16);
}

void ChatServer::start()
{
   muduo::Logger::setLogLevel(muduo::Logger::FATAL);
   server_.start();

   //初始化service
   ChatService::setServerName(serverName_);
   ChatService::instance();

   // 生成公私钥
   KeyGuard *keyguard = KeyGuard::GetInstance();
   keyguard->generateRSAkey();
}

// 处理连接事件的函数定义
void ChatServer::ServerConnFunc(const TcpConnectionPtr &conn)
{
   if (!conn->connected()) // 关闭连接的代码
   {
      ChatService::instance()->closeException(conn); // 设置用户状态函数
      conn->shutdown();
      totalconnectedCount_--;
      return;
   }

   // 初始化信息，发送rsa公钥钥
   KeyGuard *keyguard = KeyGuard::GetInstance();
   string publickey;
   keyguard->getPublickey(publickey);
   json RSAjs;
   RSAjs["msgid"] = RAS_KEY_MSG;
   RSAjs["publickey"] = publickey;
   string rsaStr = RSAjs.dump();
   conn->send(rsaStr);
   totalconnectedCount_++;
}

// 处理信息读写事件的函数定义
void ChatServer::ServerMessFunc(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time)
{
   // 循环处理缓冲区，直到数据不足以组成一条完整消息
   while (buffer->readableBytes() >= 4)
   {
      // 1. 读取头部长度（peekInt32 读取网络字节序并转为主机字节序）
      int32_t len = buffer->peekInt32();

      // 2. 判断数据是否不够一条完整消息（长度头4字节 + 内容len字节）
      if (buffer->readableBytes() < 4 + len)
      {
         // 数据包不完整，等待下一次读取
         break;
      }

      // 3. 读取数据
      buffer->retrieve(4);                         // 既然数据够了，先弹出头部
      string text = buffer->retrieveAsString(len); // 再弹出内容

      // 4. 解密与业务处理 (逻辑不变，放入 try-catch 防止崩坏)
      try
      {
         KeyGuard *keyguard = KeyGuard::GetInstance();
         string key;
         if (keyguard->findAESkey(conn, key))
         {
            text = keyguard->MsgAESDecrypt(key, text);
         }
         else
         {
            string prikey;
            keyguard->getPrivatekey(prikey);
            text = keyguard->RsaDecrypt(prikey, text);
         }

         json js = json::parse(text);
         MsgHandler msghandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
         msghandler(conn, js, time);
      }
      catch (const std::exception &e)
      {
         cout << "Message parsing error: " << e.what();
         // 不要因为一条坏消息让服务器崩溃，继续处理下一条
      }
   }
}
