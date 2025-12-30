#ifndef GROUP_HPP
#define GROUP_HPP
#include <string>
#include <vector>
#include "user.hpp"
using namespace std;


class GroupUser:public User
{
public:
    void set_role(string userrole)
    {
        role=userrole;
    }
    string get_role()
    {
        return role;
    }
private:
    string role; //角色
};


class Group
{
    public:
    Group(string groupname,string groupdesc)
    :name(groupname)
    ,desc(groupdesc)
    {
    }
    Group()=default;

    int get_id(){
        return id;
    }
    string get_name()
    {
        return name;
    }
    string get_desc()
    {
        return desc;
    }
    vector<GroupUser>* get_number()
    {
        return &number;
    }

    //设置值
    void set_id(int groupid)
    {
        id=groupid;
    }
    void set_name(string groupname)
    {
        name=groupname;
    }
    void set_desc(string groupdesc)
    {
        desc=groupdesc;
    }
    private:
    int id;
    string name;
    string desc;
    vector<GroupUser> number;
};
#endif