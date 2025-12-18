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
    char sql[1024];
    sprintf(sql, "select userid from GroupUser where groupid=%d and userid !=%d", groupid,userid);
    
    vector<int> idVec;
    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        MYSQL_RES *res = conn->query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                if (atoi(row[0]) != userid) // 不包含自己
                {
                    idVec.push_back(atoi(row[0]));
                }
            }
            mysql_free_result(res);
        }
    }
    return idVec;
}

// 查询用户加入的组信息,并返回
vector<Group> GroupOperata::queryGroup(int userid)
{
    char sql[1024];
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from AllGroup a inner join GroupUser b on a.id=b.groupid where b.userid=%d", userid);

    vector<Group> groupVec;
    ConnectionPool<MysqlConnection> *pool = ConnectionPool<MysqlConnection>::getInstance();
    shared_ptr<MysqlConnection> conn = pool->getConnection();
    if (conn)
    {
        MYSQL_RES *res =conn->query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.set_id(atoi(row[0]));
                group.set_name(row[1]);
                group.set_desc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    // 查询组内成员信息
    for (Group &group : groupVec)
    {
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from user a inner join GroupUser b on a.id=b.userid where b.groupid=%d", group.get_id());
        MYSQL_RES *res = conn->query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                GroupUser guser;
                guser.set_id(atoi(row[0]));
                guser.set_name(row[1]);
                guser.set_state(row[2]);
                guser.set_role(row[3]);
                group.get_number().push_back(guser);
            }
            mysql_free_result(res);
        }
    }
    return groupVec;
}
