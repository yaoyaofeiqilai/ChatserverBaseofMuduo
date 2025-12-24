// 数据模块的代码
#ifndef MYSQLCONNECTION_HPP
#define MYSQLCONNECTION_HPP

#include <mysql/mysql.h>
#include <string>
#include <chrono>
#include "muduo/base/Logging.h"
#include <iostream>
#include <vector>
#include <cstring> // 为 memset 提供支持
#include <map>
// 数据库操作类
using namespace std;
class MysqlConnection
{
public:
    // 定义数据库查询结果类
    using QueryResult = vector<map<string, string>>;
    MysqlConnection();
    ~MysqlConnection();

    bool connect(std::string ip, std::string user, std::string password, std::string dbname, int port);

    void updateTime(); // 更新使用时间
    int getIdleTime(); // 获取空闲时间
    MYSQL *get_connect();

    // 采用预编译的方法防止sql注入
    template <typename... Args>
    bool update(const std::string &sql, Args &&...args); // 数据库的增删改

    template <typename... Args>
    QueryResult query(const std::string &sql, Args &&...args); // 数据库的查询

private:
    MYSQL *conn_;
    std::chrono::steady_clock::time_point lastTime_; // 记录上次使用时间

    // 绑定参数的辅助函数，update,query中调用,用于绑定参数到MYSQL_BIND对象中
    void bind_param(MYSQL_BIND &bind, const int &value);
    void bind_param(MYSQL_BIND &bind, const std::string &value);
};

template <typename... Args>
bool MysqlConnection::update(const std::string &sql, Args &&...args)
{
    // 1.获取Mysql预编译句柄并初始化
    MYSQL_STMT *stmt = mysql_stmt_init(conn_);
    if (!stmt)
    {
        std::cerr << "获取sql模板失败" << std::endl;
        return false;
    }

    // 2.预编译传进来的sql语句
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length())) // mysql_stmt_prepare返回值为0为成功
    {
        mysql_stmt_close(stmt);
        return false;
    }

    // 3.绑定参数包
    const int num = sizeof...(args);
    if (num > 0)
    {
        int i = 0;
        std::vector<MYSQL_BIND> binds(num);
        std::memset(binds.data(), 0, binds.size() * sizeof(MYSQL_BIND));
        // 使用，的折叠表达式依次展开执行bind_param
        (bind_param(binds[i++], args), ...);
        // 以三参数为例，相当于(bind_param(bind[0],arg0),((bind_param(bind[1],arg1),bind_param(bind[2],arg2)))

        // 绑定参数
        mysql_stmt_bind_param(stmt, binds.data());
    }

    // 执行sql语句
    if (mysql_stmt_execute(stmt))
    {
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    return true;
} // 数据库的增删改

using QueryResult = vector<map<string, string>>;
template <typename... Args>
QueryResult MysqlConnection::query(const std::string &sql, Args &&...args) // 数据库的查询
{
    QueryResult result;
    // 1.准备句柄
    MYSQL_STMT *stmt = mysql_stmt_init(conn_);
    if (!stmt)
        return result;
    // 2.预编译sql语句
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length()))
    {
        cerr << sql << "预编译失败" << endl;
        mysql_stmt_close(stmt);
        return result;
    }
    // 3.绑定参数
    const int num = sizeof...(args);
    if (num)
    {
        vector<MYSQL_BIND> binds(num);
        memset(binds.data(), 0, binds.size() * sizeof(MYSQL_BIND));
        // 折叠表达式
        int i = 0;
        // 传入参数
        (bind_param(binds[i++], args), ...);
        // 绑定参数
        mysql_stmt_bind_param(stmt, binds.data());
    }
    // 执行语句
    if (mysql_stmt_execute(stmt) != 0)
    {
        cerr << "执行预编译指令失败" << endl;
        mysql_stmt_close(stmt);
        return result;
    }

    // 绑定结果
    // 1.获取结果集的元数据（列信息）
    MYSQL_RES *res = mysql_stmt_result_metadata(stmt);
    if (!res)
    {
        cerr << "获取结果集元数据失败" << endl;
        mysql_stmt_close(stmt);
        return result;
    }
    // 获取元数据中的列数
    int column_count = mysql_num_fields(res); // mysql_num_fields返回结果集中总共有多少列
    // mysql_fetch_fields：返回一个数组，包含了每一列的详细定义（如列名 fields[i].name）
    MYSQL_FIELD *fields = mysql_fetch_fields(res); // 获取所有列的信息

    // 2.为每一列准备缓冲区
    vector<MYSQL_BIND> result_binds(column_count, {0});
    vector<std::vector<char>> buffers(column_count, std::vector<char>(1024));
    vector<unsigned long> lengths(column_count);
    vector<char> is_null(column_count);
    //逐个绑定查询的单元格,指定指向的缓冲区，长度等
    for(int i = 0; i < column_count; i++)
    {
        result_binds[i].buffer_type=MYSQL_TYPE_STRING;//统一接收为字符串类型
        result_binds[i].buffer=buffers[i].data();   //指定信息缓冲区
        result_binds[i].buffer_length=buffers[i].size();//缓冲区长度
        result_binds[i].length=&lengths[i];          //实际接收长度
        result_binds[i].is_null=(bool*)&is_null[i];        //是否为空
    }

    //3.绑定结果集
    mysql_stmt_bind_result(stmt,result_binds.data());
    //将结果加载到本地
    mysql_stmt_store_result(stmt);

    //4.逐行获取结果
    while(!mysql_stmt_fetch(stmt))
    {
        std::map<string,string> row;   //列名，信息
        for(int i=0;i<column_count;i++)
        {
            if(is_null[i]){
                row[fields[i].name]="NULL";
            }
            else
            {
                row[fields[i].name]=string(buffers[i].data(),lengths[i]);
            }
        }
        result.push_back(row);
    }

    //5.释放资源
    mysql_stmt_close(stmt);
    mysql_free_result(res);
    return result;
}
#endif