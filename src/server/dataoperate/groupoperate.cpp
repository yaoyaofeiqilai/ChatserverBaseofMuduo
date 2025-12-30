#include "group.hpp"
#include "groupoperate.hpp"
#include "mysqlconnection.hpp"
#include "connectionpool.hpp"
// 创建组
bool GroupOperata::createGroup(Group &group)
{
    char sql[1024];
    sprintf(sql, "insert into AllGroup(groupname,groupdesc) values('%s','%s')", group.get_name().c_str(), group.get_desc().c_str());

    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        if (conn->update(sql))
        {
            group.set_id(mysql_insert_id(conn->get_connect()));
            return true;
        }
    }
    return false;
}

// 加入组
bool GroupOperata::addGroup(int userid, int groupid, string role)
{
    char sql[1024];
    sprintf(sql, "insert into GroupUser values(%d,%d,'%s')", groupid, userid, role.c_str());

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

// 查询组成员,用于转发消息
vector<int> GroupOperata::queryNumber(int groupid, int userid)
{
    //需要预编译的sql语句
    string sql="select userid from GroupUser where groupid=? ";
    
    vector<int> idVec;
    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        QueryResult res = conn->query(sql,groupid);
        if (!res.empty())
        {
           for(auto &row:res)
           {
               idVec.push_back(stoi(row["userid"].c_str()));
           }
        }
    }
    return idVec;
}

// 查询用户加入的组信息,并返回
vector<Group> GroupOperata::queryGroup(int userid)
{
    //预编译的sql语句
    string sql="select a.id,a.groupname,a.groupdesc from AllGroup a inner join GroupUser b on a.id=b.groupid where b.userid=?";

    vector<Group> groupVec;
    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        QueryResult res =conn->query(sql,userid);
        if (!res.empty())
        { 
            Group group;
            for(auto& row:res)
            {
                group.set_id(stoi(row["id"]));
                group.set_desc(row["groupdesc"]);
                group.set_name(row["groupname"]);
                groupVec.push_back(group);
            }
        }
    }

    // 查询组内成员信息
    sql= "select a.id,a.name,a.state,b.grouprole from user a inner join GroupUser b on a.id=b.userid where b.groupid=?";
    for (Group &group : groupVec)
    {
        QueryResult res = conn->query(sql,group.get_id());
        if (!res.empty())
        {
            GroupUser guser;
            for(auto& row:res)
            {
                guser.set_id(stoi(row["id"]));
                guser.set_name(row["name"]);
                guser.set_role(row["grouprole"]);
                guser.set_state(row["state"]);
                group.get_number()->push_back(guser);
            }
        }
    }
    return groupVec;
}
