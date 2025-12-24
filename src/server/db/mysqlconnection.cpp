#include "server/db/mysqlconnection.hpp"

using namespace std;

MysqlConnection::MysqlConnection()
{
    // 初始化MYSQL连接句柄
    conn_ = mysql_init(nullptr);
    if (conn_ == nullptr)
    {
        return;
    }
    lastTime_ = chrono::steady_clock::now();
}

// 析构函数
MysqlConnection::~MysqlConnection()
{
    if (conn_ != nullptr)
    {
        mysql_close(conn_); // 关闭数据库连接
    }
}

// 数据库连接
bool MysqlConnection::connect(string ip, string user, string password, string dbname, int port)
{
    // 尝试建立数据库连接调用mysql_real_connect函数
    MYSQL *ptr = mysql_real_connect(conn_, ip.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0);
    // 建立连接成功
    if (ptr != nullptr)
    {
        mysql_query(conn_, "set names gbk");
    }
    else
    {
        cerr << "connect mysql failed! please check the message!" << endl;
    }
    return ptr != nullptr;
}


// 更新时间
void MysqlConnection::updateTime()
{
    lastTime_ = chrono::steady_clock::now();
}

// 获取空闲时间
int MysqlConnection::getIdleTime()
{
    return chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - lastTime_).count();
}

MYSQL *MysqlConnection::get_connect()
{
    return conn_;
}

// 绑定参数的辅助函数
void MysqlConnection::bind_param(MYSQL_BIND &bind, const int& value)
{
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = (char *)&value;
}

// 重载版本
void MysqlConnection::bind_param(MYSQL_BIND &bind, const string& value)
{
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = (char *)value.c_str();
    bind.buffer_length = value.length();
}