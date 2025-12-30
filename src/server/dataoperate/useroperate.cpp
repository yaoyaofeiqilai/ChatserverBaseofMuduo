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
    string sql = "select * from user where id=?";
    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        QueryResult res = conn->query(sql,id); // 获取查询的返回结果
        if (res.size()==1)
        {
            User user;
            user.set_id(id);
            user.set_name(res[0]["name"]);
            user.set_password(res[0]["password"]);
            user.set_state(res[0]["state"]);
            return user;
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
bool UserOperate::resetUserState(int userid)
{
    string sql="update user set state='offline' where id=?";
    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        if (conn->update(sql,userid))
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
    string sql="select user.id,user.name,user.state from friend inner join user on friend.friendid=user.id where friend.userid=?";
    // 查询用户好友的sql语句


    vector<User> vec; // 存储用户好友列表

    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        QueryResult res = conn->query(sql,userid);
        User user;
        if (!res.empty())
        {
            for(auto& row:res)
            {
                user.set_id(stoi(row["id"]));
                user.set_name(row["name"]);
                user.set_state(row["state"]);
                vec.push_back(user);
            }
        }
    }
    return vec;
}