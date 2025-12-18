#include "mysqlconnection.hpp"
#include "muduo/base/Logging.h"
#include <iostream>
MysqlConnection:: MysqlConnection()
 {
    // 初始化MYSQL连接句柄
    conn_ = mysql_init(nullptr);
    if (conn_ == nullptr) {
        //LOG_INFO << "mysql_init failed!";
        return ;
    } 
    lastTime_=chrono::steady_clock::now();
 }

 //析构函数
MysqlConnection::~MysqlConnection()
 {
    if(conn_!=nullptr)
    {
        mysql_close(conn_); //关闭数据库连接
    }
 }

 //数据库连接
bool  MysqlConnection::connect(string ip,string user,string password,string dbname,int port)
{
  
    //尝试建立数据库连接调用mysql_real_connect函数
    MYSQL* ptr = mysql_real_connect(conn_,ip.c_str(),user.c_str(),password.c_str(),dbname.c_str(),port,nullptr,0);
    //建立连接成功
    if(ptr!=nullptr)  
    {
        mysql_query(conn_,"set names gbk");
        //LOG_INFO<<"connect mysql success!";
    }
    else{
        //LOG_INFO<<"connect mysql failed!";
        cerr<<"connect mysql failed! please check the message!"<<endl;
    }
    return ptr!=nullptr;
}


 bool MysqlConnection::update(string sql)  //数据库的增删改
 {
    if(mysql_query(conn_,sql.c_str()))
    {
       //LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
       //<< sql << "update success !";
        return true;
    }
    return false;
 }

MYSQL_RES* MysqlConnection:: query(std::string sql)  //数据库的查询
{
    if(mysql_query(conn_,sql.c_str()))
    {
       //LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
       //<< sql << "query failed! pelase check the message!";
       return nullptr;
    }
    return mysql_use_result(conn_);
}

//更新时间
 void  MysqlConnection::updateTime()
 {
    lastTime_=chrono::steady_clock::now();
 }

 //获取空闲时间
 int  MysqlConnection::getIdleTime()
 {
    return chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now()-lastTime_).count();
 }

 MYSQL* MysqlConnection::get_connect()
 {
    return conn_;
 }