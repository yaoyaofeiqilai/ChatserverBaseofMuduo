#include <iostream>
#include "redis.hpp"
#include <thread>
#include <string>
#include "connectionpool.hpp"
int main()
{
    Redis redis;
    redis.connect("127.0.0.1", "", "yaoyaofeiqilai1111", "", 6379);
    // 插入一个数据
    redis.hset("onlineUser", to_string(1111), "Server01");
    redis.hset("onlineUser", to_string(1112), "Server01");
    redis.hset("onlineUser", to_string(333), "Server02");
    redis.hset("onlineUser", to_string(144), "Server01");
    redis.hset("onlineUser", to_string(1666), "Server03");
 
    //连接池
    auto pool=ConnectionPool<Redis>::getInstance();
    pool->init_pool("127.0.0.1", 6379, "", "yaoyaofeiqilai1111", "", 16, 1024, 60, 5000);

   auto conn= pool->getConnection();
   conn->hset("onlineUser", to_string(123), "Server03");
    return 0;
}