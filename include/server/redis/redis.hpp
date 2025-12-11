#ifndef REDIS_HPP
#define REDIS_HPP

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
using namespace std;

class Redis
{
public:
    Redis();
    ~Redis();

    // 连接redis服务器
    bool connect();

    // 向redis中发布消息
    bool publish(int channel, string message);

    // 向redis中订阅消息
    bool subscribe(int channel);

    // 取消对应通道的订阅
    bool unsubscribe(int channel);

    // 在独立线程中监听订阅通道中的消息
    void observer_channel_message();

    // 初始化监听的回调函数
    void init_notify_handler(function<void(int, string)> func);

    void beginlisten();

private:
    // rediscontext可以理解为相当于一个客户端窗口
    // 负责发布消息的context
    redisContext *publish_context_;

    // 负责监听对应的channel的context,监听时线程会进入
    // 等待状态（阻塞）,因此有单独一个线程负责监听
    redisContext *subscribe_context_;

    // 回调函数,收到订阅消息时回调
    function<void(int, string)> notify_message_handler_;
};

#endif