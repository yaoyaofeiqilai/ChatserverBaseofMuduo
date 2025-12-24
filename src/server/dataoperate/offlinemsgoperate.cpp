#include "offlinemsgoperate.hpp"
#include "connectionpool.hpp"
#include "mysqlconnection.hpp"
// 添加离线消息
void OfflineMsgOperate::insertOfflineMsg(int id, string msg)
{
    // 组装sql语句
    char sql[1024];
    sprintf(sql, "insert into OfflineMessage values(%d,'%s')", id, msg.c_str());
    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        // 插入消息
        conn->update(sql);
    }
}

// 移除离线消息
void OfflineMsgOperate::removeOfflineMsg(int id)
{
    char sql[1024];
    sprintf(sql, "delete from OfflineMessage where userid=%d", id);

    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        // 删除所有的离线消息
        conn->update(sql);
    }
}

// 查询离线消息
vector<string> OfflineMsgOperate::queryOfflineMsg(int id)
{
    // 组装sql语句
    string sql;
    vector<string> msglist;
    sql= "select message from OfflineMessage where userid=?";

    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn) // 连接数据库
    {
        QueryResult res = conn->query(sql,id); // 返回查询结果
        if (!res.empty())
        {
           for(auto& row:res)
           {
            msglist.push_back(row["message"]);
           }
        }
    }
    return msglist;
}