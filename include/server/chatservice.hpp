//编写一个唯一实例的服务类，实现对各种消息类型的处理
//静态函数+静态成员可以实现唯一实例，构造函数在private中
//使用map存储不同msgid的信息的处理函数

#ifndef CHATSERVICE_HPP
#define CHATSERVICE_HPP

#include<unordered_map>
#include <functional>
#include <muduo/net/TcpConnection.h>
#include "json.hpp"
#include "useroperate.hpp"
#include<thread>
#include<mutex>
#include <atomic>
#include"offlinemsgoperate.hpp"
#include "groupoperate.hpp"
#include "redis.hpp"
#include "keyguard.hpp"
#include "mysqlconnection.hpp"
#include "connectionpool.hpp"
using namespace muduo;
using namespace muduo::net;
using json=nlohmann::json;
using MsgHandler =std::function<void(const TcpConnectionPtr&,json&,Timestamp time)>;


class ChatService
{
    public:
    //获取唯一实例的方法
    static ChatService* instance();

     //处理服务器ctrl+c退出,重置用户状态
    static void ServerOffline(int);

    //登录信息的处理方法
    void login(const TcpConnectionPtr&,json&,Timestamp);

    //处理异常退出的方法
    void closeException(const TcpConnectionPtr& );

    //注册信息的处理方法
    void reg( const TcpConnectionPtr&,json&,Timestamp); 

    //获取消息类型对应的处理器
    MsgHandler getHandler(int msgid);

    //处理一对一聊天方法
    void oneChat(const TcpConnectionPtr&,json&,Timestamp);

    //添加好友的业务代码
    void addFriend(const TcpConnectionPtr&,json&,Timestamp);

    //创建群组的消息
    void createGroup(const TcpConnectionPtr&,json&,Timestamp);

    //加入群组的业务
    void addGroup(const TcpConnectionPtr&,json&,Timestamp);

    //群组聊天业务
    void groupChat(const TcpConnectionPtr&,json&,Timestamp);

    //用户退出业务
    void userLoginout(const TcpConnectionPtr&,json&,Timestamp);

    //处理从redis上监听的信息处理函数
    void redisMessageHandler(int userid,string message);

    //处理客户端发送来的密钥
    void clientAESkey(const TcpConnectionPtr &conn, json &js, Timestamp time);

    string serverMsgEncrtpt(const TcpConnectionPtr &conn, json &js);

    //启动性能检测函数
    void performanceTest();

    //设置服务器的名字
    static void setServerName(const string& name);

    private :
    //构造函数
    ChatService();
    //存储各个类型消息对应的方法
    std::unordered_map<int,MsgHandler> msgHandlerMap_;

    //用户表数据操作对象
    UserOperate useroperate_;

    //存储用户连接的map表
    std::unordered_map<int,TcpConnectionPtr> userConnMap_;
    //由于服务器时在多线程环境中的，要保证线程安全
    std::mutex ConnMapMutex_;

    //离线消息操作对象
    OfflineMsgOperate offlineMsgOperate_;

    //群组相关的业务处理方法
    GroupOperata groupOperata_;

    //连接池指针
    ConnectionPool<MysqlConnection>* mysqlPool_;

    //redis连接池指针
    ConnectionPool<Redis>* redisPool_;

    //redis操作对象
    Redis subscribeRedis_;

    //总共发送到信息数
    atomic<int> totalMsgCount_;

    //服务器名字
    static string serverName_;
};
#endif