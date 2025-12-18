// 数据模块的代码
#ifndef MYSQLCONNECTION_HPP
#define MYSQLCONNECTION_HPP

#include <mysql/mysql.h>
#include <string>
#include <chrono>
using namespace std;
// 数据库操作类
class MysqlConnection
{
public:
    MysqlConnection();

    ~MysqlConnection();

    bool connect(string ip,string user,string password,string dbname,int port);

    bool update(string); // 数据库的增删改

    MYSQL_RES *query(std::string); // 数据库的查询

    void updateTime();  //更新使用时间
    int  getIdleTime();    //获取空闲时间

    MYSQL* get_connect();
private:
    MYSQL *conn_;
    chrono::steady_clock::time_point lastTime_; // 记录上次使用时间
};
#endif