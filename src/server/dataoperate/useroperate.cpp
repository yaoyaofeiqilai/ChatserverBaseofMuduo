// 是对用户操作类UserOperate的实现
#include "useroperate.hpp"
#include <string>
#include "mysqlconnection.hpp"
#include "connectionpool.hpp"
#include <muduo/base/Logging.h>
#include <thread>
bool UserOperate::
    insert(User &user)
{
    //{"msgid":2,"name":"wangwu","password":"123456we"}
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into user(name,password,state) values('%s','%s','%s')", user.get_name().c_str(), user.get_password().c_str(), user.get_state().c_str());

    LOG_INFO << "sql:" << sql;
    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();

    if (conn->update(sql))
    {
        // 成功添加信息，获取插入成功的user的id,并返回
        user.set_id(mysql_insert_id(conn->get_connect()));
        // mysql_insert_id 函数用于获取上一次插入操作自动生成的主键ID
        return true;
    }
    return false;
}

User UserOperate::query(int id)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select * from user where id=%d", id);
    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        MYSQL_RES *res = conn->query(sql); // 获取查询的返回结果
        if (res != nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res); // 获取结果中对应的行
            if (row != nullptr)
            {
                User user;
                user.set_id(atoi(row[0]));
                user.set_name(row[1]);
                user.set_password(row[2]);
                user.set_state(row[3]);
                mysql_free_result(res); // 释放结果占用的内存资源，防止资源泄露
                return user;
            }
        }
    }
    return User();
}

bool UserOperate::update_state(User &user)
{
    // 组装sql语句
    char sql[1024];
    sprintf(sql, "update user set state='%s' where id=%d", user.get_state().c_str(), user.get_id());

    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        if (conn->update(sql))
        {
            return true;
        }
    }
    return false;
}

// 重置用户的状态信息
bool UserOperate::resetUserState()
{
    char sql[1024];
    sprintf(sql, "update user set state='offline'");

   ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        if (conn->update(sql))
        {
            return true;
        }
    }
    return false;
}

bool UserOperate::insertFriend(int userid, int friendid)
{

    char sql1[1024];
    char sql2[1024];
    sprintf(sql1, "insert into friend values(%d,%d)", userid, friendid);
    sprintf(sql2, "insert into friend values(%d,%d)", friendid, userid);

    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        if (conn->update(sql1))
        {
            conn->update(sql2);
        }
    }
    return false;
}

// 查询登录用户的好友列表
vector<User> UserOperate::queryFriend(int userid)
{
    char sql[1024];
    // 查询用户好友的sql语句
    sprintf(sql, "select user.id,user.name,user.state from friend inner join user on friend.friendid=user.id where friend.userid=%d;", userid);
    vector<User> vec; // 存储用户好友列表

    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        MYSQL_RES *res = nullptr;
        res = conn->query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                // 将查询结果传入vec
                User user;
                user.set_id(atoi(row[0]));
                user.set_name(row[1]);
                user.set_state(row[2]);
                vec.push_back(user);
            }
        }
    }
    return vec;
}