#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis()
    : context_(nullptr)
{
}

Redis::~Redis()
{
    if (context_ != nullptr)
    {
        redisFree(context_);
    }
}

bool Redis::connect(std::string ip, std::string user, std::string password, std::string dbname, int port)
{
    // 负责publish发布消息的上下文连接
    bool res = false;
    context_ = redisConnect(ip.c_str(), port);
    if (nullptr == context_ || context_->err)
    {
        cerr << "connect redis failed!" << endl;
        return res;
    }

    redisReply *reply = (redisReply *)redisCommand(context_, "auth %s", password.c_str());
    if (reply == nullptr)
    {
        cerr << "auth failed!" << endl;
        return res;
    }

    if (reply->type == REDIS_REPLY_STATUS)
    {
        if (string(reply->str) == "OK")
        {
           // cout << "login success" << endl;
            res = true;
        }
        else
        {
            cout<<"password is wrong"<<endl;
        }
    }

    freeReplyObject(reply);
    return res;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(string channel, string message)
{
    redisReply *reply = (redisReply *)redisCommand(context_, "PUBLISH %s %s", channel.c_str(), message.c_str());
    if (nullptr == reply)
    {
        cerr << "publish command failed!" << endl;
        return false;
    }
    if(reply->type == REDIS_REPLY_INTEGER)
    {
        freeReplyObject(reply);
        return true;
    }
    freeReplyObject(reply);
    return false;
}

// 向redis指定的通道subscribe订阅消息
bool Redis::subscribe(string channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源
    if (REDIS_ERR == redisAppendCommand(this->context_, "SUBSCRIBE %s", channel.c_str()))
    {
        cerr << "subscribe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->context_, &done))
        {
            cerr << "subscribe command failed!" << endl;
            return false;
        }
    }
    // redisGetReply

    return true;
}

// 向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(string channel)
{
    if (REDIS_ERR == redisAppendCommand(this->context_, "UNSUBSCRIBE %s", channel.c_str()))
    {
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->context_, &done))
        {
            cerr << "unsubscribe command failed!" << endl;
            return false;
        }
    }
    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    cout<<"开始监听"<<endl;
    redisReply *reply = nullptr;
    while (REDIS_OK == redisGetReply(this->context_, (void **)&reply))
    {
        // 订阅收到的消息是一个带三元素的数组
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 给业务层上报通道上发生的消息
            notify_message_handler_(reply->element[2]->str);
        }

        freeReplyObject(reply);
    }

    cerr << ">>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<" << endl;
}

void Redis::init_notify_handler(function<void(string)> fn)
{
    this->notify_message_handler_ = fn;
}

void Redis::beginlisten()
{
    // 在单独的线程中，监听通道上的事件，有消息给业务层进行上报
    thread t([&]()
             { observer_channel_message(); });
    t.detach();

    // cout << "connect redis-server success!" << endl;
}

bool Redis::set(string key, string value)
{
    // 执行状态
    bool status = false;
    redisReply *reply = (redisReply *)redisCommand(context_, "SET %s %s", key.c_str(), value.c_str());

    if (nullptr == reply)
    {
        cerr << "set command failed!" << endl;
    }

    if (reply->type == REDIS_REPLY_STATUS && string(reply->str) == "OK")
    {
        status = true;
    }

    freeReplyObject(reply);
    return status;
}
bool Redis::hset(string key, string field, string value)
{
    redisReply *reply = (redisReply *)redisCommand(context_, "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    if (nullptr == reply)
    {
        cerr << "Hset command failed!" << endl;
        return false;
    }

    bool status = false;
    if (reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0)
    {
        status = true;
    }
    freeReplyObject(reply);
    return status;
}

bool Redis::sadd(string key,string value)
{
    redisReply *reply = (redisReply *)redisCommand(context_, "SADD %s %s", key.c_str(), value.c_str());
    if (nullptr == reply)
    {
        cerr << "Sadd command failed!" << endl;
        return false;
    }

    bool status = false;
    if (reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0)
    {
        status = true;
    }
    freeReplyObject(reply);
    return status;
}

vector<string> Redis::smembers(string key)
{
    vector<string> members;
    redisReply *reply = (redisReply *)redisCommand(context_, "SMEMBERS %s", key.c_str());
    if (nullptr == reply)
    {
        cerr << "Smembers command failed!" << endl;
        return members;
    }

    if (reply->type == REDIS_REPLY_ARRAY)
    {
        for (int i = 0; i < reply->elements; i++)
        {
            if (reply->element[i]->type == REDIS_REPLY_STRING)
            {
                members.push_back(reply->element[i]->str);
            }
        }
    }
    freeReplyObject(reply);
    return members;
}

// 添加群组成员
bool Redis::lpush(string key, string value)
{
    redisReply *reply = (redisReply *)redisCommand(context_, "LPUSH %s %s", key.c_str(), value.c_str());
    if (nullptr == reply)
    {
        cerr << "Lpush command failed!" << endl;
        return false;
    }

    bool status = false;
    if (reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0)
    {
        status = true;
        cout<<"添加成功"<<endl;
    }
    freeReplyObject(reply);
    return status;
}

bool Redis::del(string key)
{
    redisReply *reply = (redisReply *)redisCommand(context_, "DEL %s", key.c_str());
    if (nullptr == reply)
    {
        cerr << "del command failed!" << endl;
        return false;
    }

    bool status = false;
    if (reply->type == REDIS_REPLY_INTEGER)
    {
        status = true;
    }
    freeReplyObject(reply);
    return status;
}

bool Redis::hdel(string key, string field)
{
    redisReply *reply = (redisReply *)redisCommand(context_, "HDEL %s %s", key.c_str(), field.c_str());
    if (nullptr == reply)
    {
        cerr << "Hdel command failed!" << endl;
        return false;
    }

    bool status = false;
    if (reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0)
    {
        status = true;
    }
    freeReplyObject(reply);
    return status;
}

vector<string> Redis::lrange(string key, int start, int end)
{
    vector<string> userVec;
    redisReply *reply = (redisReply *)redisCommand(context_, "LRANGE %s %d %d", key.c_str(), start, end);
    if (reply == nullptr)
    {
        cerr << "command getGroupUser fail!" << endl;
        return userVec;
    }

    if (reply->type == REDIS_REPLY_ARRAY)
    {
        for (int i = 0; i < reply->elements; i++)
        {
            if (reply->element[i]->type == REDIS_REPLY_STRING)
            {
                userVec.push_back(reply->element[i]->str);
            }
        }
    }
    freeReplyObject(reply);
    return userVec;
}

string Redis::hget(string key, string field)
{
    string res="";
    // 查看用户是否在线表中,没在则不在线
    redisReply *reply = (redisReply *)redisCommand(context_, "HGET %s %s", key.c_str(), field.c_str());
    if (reply == nullptr)
    {
        cerr << "command getStatus fail!" << endl;
        return res;
    }

    if (reply->type == REDIS_REPLY_STRING)  //没有查到即不在线返回值类型为REDIS_REPLY_NIL
    {
        res = reply->str;
    }

    freeReplyObject(reply);
    return res;
}

void Redis::updateTime()
{
    lastTime_ = chrono::steady_clock::now();
} // 更新使用时间
int Redis::getIdleTime()
{
    return chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - lastTime_).count();
} // 获取空闲时间