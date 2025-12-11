#include "db.hpp"
#include "muduo/base/Logging.h"

static string server = "127.0.0.1";
static string user = "root";
static string password = "Lsg20041013.";
static string dbname = "chat";

 Mysql:: Mysql()
 :conn_(nullptr)
 {
 }

 //析构函数
 Mysql::~Mysql()
 {
    if(conn_!=nullptr)
    {
        mysql_close(conn_); //关闭数据库连接
    }
 }

 //数据库连接
bool  Mysql::connect()
{
    // 初始化MYSQL连接句柄
    conn_ = mysql_init(nullptr);
    if (conn_ == nullptr) {
        //LOG_INFO << "mysql_init failed!";
        return false;
    }

    //尝试建立数据库连接调用mysql_real_connect函数
    MYSQL* ptr = mysql_real_connect(conn_,server.c_str(),user.c_str(),password.c_str(),dbname.c_str(),3306,nullptr,0);
    //建立连接成功
    if(ptr!=nullptr)  
    {
        mysql_query(conn_,"set names gbk");
        //LOG_INFO<<"connect mysql success!";
    }
    else{
        //LOG_INFO<<"connect mysql failed!";
    }
    return ptr!=nullptr;
}


 bool Mysql::update(std::string sql)  //数据库的增删改
 {
    if(mysql_query(conn_,sql.c_str()))
    {
       //LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
       //<< sql << "update success !";
        return false;
    }
    return true;
 }


MYSQL_RES* Mysql:: query(std::string sql)  //数据库的查询
{
    if(mysql_query(conn_,sql.c_str()))
    {
       //LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
       //<< sql << "query failed! pelase check the message!";
       return nullptr;
    }
    return mysql_use_result(conn_);
}

//返回数据库的连接
MYSQL* Mysql::get_connect()
{
    return conn_;
}