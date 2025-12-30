#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <iostream>
#include <thread>
using namespace std::placeholders;
using namespace muduo;

// 静态成员变量定义
string ChatService::serverName_;

// 获取唯一实例的方法
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 构造函数
ChatService::ChatService()
{
    msgHandlerMap_.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    msgHandlerMap_.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    msgHandlerMap_.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    msgHandlerMap_.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    msgHandlerMap_.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    msgHandlerMap_.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    msgHandlerMap_.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    msgHandlerMap_.insert({LOGINOUT_MSG, std::bind(&ChatService::userLoginout, this, _1, _2, _3)});
    msgHandlerMap_.insert({AES_KEY_MSG, std::bind(&ChatService::clientAESkey, this, _1, _2, _3)});
    // 重置用户状态
    // useroperate_.resetUserState();

    totalMsgCount_ = 0;
    thread thread_performance(&ChatService::performanceTest, this);
    thread_performance.detach();

    if (subscribeRedis_.connect("127.0.0.1", "", "yaoyaofeiqilai1111", "", 6379))
    { // 订阅消息
        subscribeRedis_.init_notify_handler(bind(&ChatService::redisMessageHandler, this, _1));
        cout << "服务器id" << serverName_ << endl;
        subscribeRedis_.subscribe(serverName_);
        subscribeRedis_.beginlisten();
    }

    mysqlPool_ = ConnectionPool<MysqlConnection>::getInstance();
    mysqlPool_->init_pool("127.0.0.1", 3306, "root", "Lsg20041013.", "chat", 16, 100, 60, 5000);

    redisPool_ = ConnectionPool<Redis>::getInstance();
    redisPool_->init_pool("127.0.0.1", 6379, "", "yaoyaofeiqilai1111", "", 16, 100, 60, 5000);
}

// 登录信息的处理方法
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 获取用户输入的id和密码
    int id = js["id"];
    string pwd = js["password"];

    // 查询返回结果
    User user = useroperate_.query(id);
    // 比对密码是否正确
    if (user.get_id() != -1 && pwd == user.get_password())
    {
        // 如果已经在线
        if (user.get_state() == "online")
        {
            json resp;
            resp["msgid"] = LOGIN_MSG_ACK;
            resp["errno"] = 2; // 已经登录放回2的错误码
            resp["errmsg"] = "the user is online can not login again";
            string chipertext = serverMsgEncrtpt(conn, resp);
            conn->send(chipertext); // 回应用户报文
        }
        else
        {
            // 密码正确,修改用户的登录状态
            user.set_state("online");
            // 更新数据库中用户的状态
            useroperate_.update_state(user);

            // 将用户连接添加到连接表中
            {
                lock_guard<mutex> lock(ConnMapMutex_);
                userConnMap_.insert({user.get_id(), conn});
            }
            json resp;
            resp["msgid"] = LOGIN_MSG_ACK;
            resp["userid"] = user.get_id();
            resp["name"] = user.get_name();
            resp["errno"] = 0; // 错误码等于零时表示没有错误

            // 将用户信息加入redis数据库中
            {
                auto redisConn = redisPool_->getConnection();
                redisConn->hset("onlineUser", to_string(user.get_id()), serverName_);
            }
            // 登录成功后，查询用户的好友列表
            vector<User> friendlist = useroperate_.queryFriend(id);
            if (!friendlist.empty())
            {
                // 由于User是自定义类型，json["xxx"]=friendlist 会报错
                // 先将User类型转成;json字符串类型，再传入json对象中
                for (auto &f : friendlist)
                {
                    json js;
                    js["id"] = f.get_id();
                    js["name"] = f.get_name();
                    js["state"] = f.get_state();
                    resp["friendlist"].push_back(js.dump());
                }
            }

            // 查询用户的群组列表
            vector<Group> grouplist = groupOperata_.queryGroup(id);
            if (!grouplist.empty())
            {
                auto redisConn = redisPool_->getConnection();
                for (auto &group : grouplist)
                {
                    json grpjs;
                    grpjs["groupid"] = group.get_id();
                    grpjs["groupname"] = group.get_name();
                    grpjs["groupdesc"] = group.get_desc();
                    vector<string> uservec;
                    for (auto &user : (*group.get_number()))
                    {
                        json js;
                        js["numberid"] = user.get_id();
                        js["numbername"] = user.get_name();
                        js["numberstate"] = user.get_state();
                        js["numberrole"] = user.get_role();
                        redisConn->sadd(to_string(group.get_id()), std::to_string(user.get_id()));
                        uservec.push_back(js.dump());
                    }
                    grpjs["numberlist"] = uservec; // 群组成员
                    resp["grouplist"].push_back(grpjs.dump());
                }
            }

            // 查询用户是否有离线消息
            vector<string> msglist = offlineMsgOperate_.queryOfflineMsg(user.get_id());
            if (msglist.size())
            {
                //resp["offlinemsglist"] = msglist;
                offlineMsgOperate_.removeOfflineMsg(user.get_id()); // 删除对应的消息
            }
            string chipertext = serverMsgEncrtpt(conn, resp);
            conn->send(chipertext); // 回应用户登录报文，同时返回好友列表
        }
    }
    else
    {
        // 密码错误
        json resp;
        resp["msgid"] = LOGIN_MSG_ACK;
        resp["errno"] = 1; // 表示注册失败
        resp["errmsg"] = "the id or the password is error";
        string chipertext = serverMsgEncrtpt(conn, resp);
        conn->send(chipertext); // 回应
    }
}

// 用户退出业务
void ChatService::userLoginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["userid"];

    // 删除连接表相关对象
    {
        lock_guard<mutex> lock(ConnMapMutex_);
        auto it = userConnMap_.find(userid);
        if (it != userConnMap_.end())
        {
            userConnMap_.erase(it);
        }
    }

    // 删除redis中的用户信息
    {
        auto redisConn = redisPool_->getConnection();
        redisConn->hdel("onlineUser", to_string(userid));
    }

    // 修改状态
    User user;
    user.set_id(userid);
    useroperate_.update_state(user);
}

// 注册信息的处理方法
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 接收json对象中的数据
    string name = js["name"];
    string pwd = js["password"];
    //cout << js.dump() << endl;
    // 根据数据创建对象
    User user;
    user.set_name(name);
    user.set_password(pwd);
    user.set_state("offline");
    if (useroperate_.insert(user)) // 通过useroperate对象实现对数据的操作
    {
        // 注册成功
        json resp;
        resp["msgid"] = REG_MSG_ACK;
        resp["userid"] = user.get_id();
        resp["errno"] = 0; // 错误码等于零时表示没有错误
        string chipertext = serverMsgEncrtpt(conn, resp);
        conn->send(chipertext); // 回应用户注册报文
    }
    else
    {
        json resp;
        resp["msgid"] = REG_MSG_ACK;
        resp["errno"] = 1; // 表示注册失败
        string chipertext = serverMsgEncrtpt(conn, resp);
        conn->send(chipertext); // 回应
    }
}

// 获取消息类型对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    auto ptr = msgHandlerMap_.find(msgid);
    if (ptr == msgHandlerMap_.end()) // 消息类型不存在
    {
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time) -> void
        {
            LOG_ERROR << "msgid:" << msgid << "can not find the handler!";
        };
    }
    return msgHandlerMap_[msgid];
}

// 处理异常退出的方法
void ChatService::closeException(const TcpConnectionPtr &conn)
{
    User user;
    {
        // 对userConnMap_进行遍历，查找对应的id
        lock_guard<mutex> lock(ConnMapMutex_);
        for (auto it = userConnMap_.begin(); it != userConnMap_.end(); it++)
        {

            if (it->second == conn)
            {
                user.set_id(it->first);
                userConnMap_.erase(it);
                // 删除缓存数据库中的信息
                {
                    auto redisConn = redisPool_->getConnection();
                    redisConn->hdel("onlineUser", to_string(user.get_id()));
                }
                break;
            }
        }
    }

    // 更新用户状态
    user.set_state("offline");
    useroperate_.update_state(user);

    // 删除对应的aeskey
    KeyGuard *keyguard = KeyGuard::GetInstance();
    keyguard->removekey(conn);
}

// 一对一聊天处理方法，msgid=5
//{"msgid":5,"from":1,"name":zhangsan,"to":2,"msg":"h1111111a"}
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    totalMsgCount_++;
    int toid = js["to"];
    {
        lock_guard<mutex> lock(ConnMapMutex_);
        auto it = userConnMap_.find(toid);
        if (it != userConnMap_.end())
        {
            // 收件人在线,直接转发信息
            string chipertext = serverMsgEncrtpt(it->second, js);
            it->second->send(chipertext);
            return;
        }
    }

    // 在redis上查看是否在线
    auto redisConn = redisPool_->getConnection();
    string serverName = redisConn->hget("onlineUser", std::to_string(toid));
    if (serverName != "")
    {
        // 转发信息
        redisConn->publish(serverName, js.dump());
    }
    // 收件人不在线，将消息存储在数据库中，等下次上线时转发
    offlineMsgOperate_.insertOfflineMsg(toid, js.dump());
}

// 服务器断开时重置用户状态,静态
void ChatService::ServerOffline()
{
    lock_guard<mutex> lock(ConnMapMutex_);
    for (auto it=userConnMap_.begin(); it!=userConnMap_.end(); it++)
    {
        useroperate_.resetUserState(it->first);
        auto redisConn = redisPool_->getConnection();
        redisConn->hdel("onlineUser", to_string(it->first));
    }
    cout << "服务器已退出" << endl;
    exit(0);
}

// 添加好友的业务代码
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];
    int friendid = js["friendid"];
    // 存储好友信息
    useroperate_.insertFriend(userid, friendid);
}

// 创建群组的消息
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    Group group;
    int userid = js["userid"];
    group.set_name(js["groupname"]);
    group.set_desc(js["groupdesc"]);
    if (groupOperata_.createGroup(group))
    {
        // 添加创建人
        groupOperata_.addGroup(userid, group.get_id(), "creator");

        // 发送回应报文
        json resp;
        resp["msgid"] = CREATE_GROUP_MSG_ACK;
        resp["errno"] = 0;
        resp["groupid"] = group.get_id();
        string chipertext = serverMsgEncrtpt(conn, resp);
        conn->send(chipertext);
    }
}

// 加入群组的业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["userid"];
    int groupid = js["groupid"];
    groupOperata_.addGroup(userid, groupid, "normal");

    // 发送回应报文
    json resp;
    resp["msgid"] = ADD_GROUP_MSG_ACK;
    resp["errno"] = 0;
    string chipertext = serverMsgEncrtpt(conn, resp);
    conn->send(chipertext);
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 获取这个群成员id
    int groupid = js["groupid"];
    int userid = js["userid"];
    auto redisConn = redisPool_->getConnection();
    // 去redis数据库中查询
    vector<string> tmp = redisConn->smembers(to_string(groupid));
    vector<int> numid;
    if (tmp.empty())
    { // 去Mysql数据库中查询
        numid = groupOperata_.queryNumber(groupid, userid);
    }
    else
    {
        for (auto &id : tmp)
        {
            numid.push_back(stoi(id));
        }
    }

    // 逐个转发信息
    {
        lock_guard<mutex> lock(ConnMapMutex_);
        for (auto &num : numid)
        {
            if(num==userid)continue; // 如果是自己,则不转发
            auto it = userConnMap_.find(num);
            if (it != userConnMap_.end())
            {
                // 用户在线直接转发
                js["to"] = num;
                string chipertext = serverMsgEncrtpt(it->second, js);
                userConnMap_[num]->send(chipertext);
            }
            else
            {
                string serverName=redisConn->hget("onlineUser", to_string(num));
                if (serverName!="")
                {
                    // 发布到redis中
                    js["to"] = num;
                    redisConn->publish(serverName, js.dump());
                }
                else
                { // 存入离线消息列表
                    offlineMsgOperate_.insertOfflineMsg(num, js.dump());
                }
            }
        }
    }
}

// 处理从redis上监听的信息处理函数
void ChatService::redisMessageHandler(string message)
{
    // 解析字符串
    json js = json::parse(message);
    int userid = js["to"];
    {
        lock_guard<mutex> lock(ConnMapMutex_);
        auto it = userConnMap_.find(userid);
        if (it != userConnMap_.end())
        {
            // 收件人在线,直接转发信息
            string chipertext = serverMsgEncrtpt(it->second, js);
            it->second->send(chipertext);
            return;
        }
    }
}

// AES密钥交换
void ChatService::clientAESkey(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string aeskey_base64 = js["aeskey"];
    KeyGuard *keyguard = KeyGuard::GetInstance();
    string aeskey = keyguard->base64_decode(aeskey_base64);
    keyguard->addkey(conn, aeskey);

    json resp;
    resp["msgid"] = AES_KEY_MSG_ACK;
    resp["errno"] = 0;
    string chipertext = serverMsgEncrtpt(conn, resp);
    conn->send(chipertext);
}

string ChatService::serverMsgEncrtpt(const TcpConnectionPtr &conn, json &js)
{
    KeyGuard *keyguard = KeyGuard::GetInstance();
    string aeskey;
    string text;
    keyguard->findAESkey(conn, aeskey);
    text = js.dump();
    return keyguard->MsgAESEncrypt(aeskey, text);
}

void ChatService::performanceTest()
{
    cout << "-----性能检测线程启动-----" << endl;
    KeyGuard *ptr = KeyGuard::GetInstance();
    while (true)
    {
        cout << "当前登录用户数：" << ptr->showMapSize() << endl;
        int last = totalMsgCount_.load();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        cout << "当前总共处理信息数：" << totalMsgCount_.load() << endl;
        cout << "每秒处理信息数：" << (totalMsgCount_ - last) / 5 << endl;
    }
}

// 设置服务器名字
void ChatService::setServerName(const string &name)
{
    serverName_ = name;
}