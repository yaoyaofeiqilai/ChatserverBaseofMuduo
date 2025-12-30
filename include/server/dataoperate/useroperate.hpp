#ifndef USEROPERATE_HPP
#define USEROPERATE_HPP
//用户数据超控类，用于用户表数据的更改操作
//通过db提供的接口实现对数据的操作
//是业务层中的User和数据库底层存储的User进行交互的桥梁
#include "user.hpp"
#include <vector>
class UserOperate
{
  
    public:
    //插入新的用户信息，即注册新的用户
    bool insert(User& user);

    //查询用户
    User query(int id);

    //更改用户的状态
    bool update_state(User& user);

    bool resetUserState(int userid); //重置用户的状态信息

    //添加好友
    bool insertFriend(int userid,int friendid);

    //查询好友列表
    vector<User> queryFriend(int userid);
    private:
};

#endif