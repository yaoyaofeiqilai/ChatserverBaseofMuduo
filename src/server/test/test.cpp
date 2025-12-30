// #include <iostream>
// #include "redis.hpp"
// #include <thread>
// #include <string>
// #include <functional>
// #include "connectionpool.hpp"
// void test(int a, string s)
// {
//     cout << "test函数" << endl;
// }
// int main()
// {
//     // 连接池
//     auto pool = ConnectionPool<Redis>::getInstance();
//     pool->init_pool("127.0.0.1", 6379, "", "yaoyaofeiqilai1111", "", 16, 1024, 60, 5000);

//     auto conn = pool->getConnection();
//     conn->hset("onlineUser", to_string(123), "Server03");

//     if (conn->subscribe("ChatServer1111"))
//     {
//         cout << "订阅成功" << endl;
//         conn->init_notify_handler(function<void(int, string)>(test));
//         conn->beginlisten();
//     }
//     else
//     {
//         cout << "订阅失败" << endl;
//     }

//     auto conn2 = pool->getConnection();
//     conn2->publish("ChatServer1111", "Hello, World!");
//     conn2->publish("ChatServer1111", "Hello, World2!");
//     conn2->publish("ChatServer1111", "Hello, World3!");

//     int a;
//     cin >> a;
//     return 0;
// }