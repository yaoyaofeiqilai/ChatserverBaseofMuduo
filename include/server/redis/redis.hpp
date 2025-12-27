#ifndef REDIS_HPP
#define REDIS_HPP

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
#include <mutex>
#include <vector>
#include <chrono>
using namespace std;

class Redis
{
public:
    Redis();
    ~Redis();

    // 连接redis服务器
    bool connect(std::string ip, std::string user, std::string password, std::string dbname, int port);

    // 向redis中发布消息
    bool publish(string channel, string message);

    // 向redis中订阅消息
    bool subscribe(string channel);

    // 取消对应通道的订阅
    bool unsubscribe(string channel);

    // 在独立线程中监听订阅通道中的消息
    void observer_channel_message();

    // 初始化监听的回调函数
    void init_notify_handler(function<void(int, string)> func);

    void beginlisten();
    void updateTime(); // 更新使用时间
    int getIdleTime(); // 获取空闲时间

    // 1.群组表group  //用户群消息的转发用list
    // 2.在线用户onlineUser  //onlineUser userid  serverid
    // 设置redis数据库数据,包括set,del
    bool set(string key, string value);
    bool hset(string key, string field, string value);
    bool lpush(string key, string value);
    bool del(string key);
    bool hdel(string key, string field);
    // 获取数据,get
    vector<string> lrange(string key, int start, int end);
    string hget(string key, string field);

private:
    // rediscontext可以理解为相当于一个客户端窗口
    // 负责发布消息的context
    redisContext *context_;
    // 回调函数,收到订阅消息时回调
    function<void(int, string)> notify_message_handler_;
    // 上次使用时间
    chrono::steady_clock::time_point lastTime_;
};

#endif