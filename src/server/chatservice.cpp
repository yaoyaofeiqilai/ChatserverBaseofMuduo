#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <iostream>
#include <thread>
using namespace std::placeholders;
using namespace muduo;

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
    useroperate_.resetUserState();
    totalMsgCount_ = 0;
    thread thread_performance(&ChatService::performanceTest, this);
    thread_performance.detach(); 
    // if (redis_.connect())
    // {
    //     redis_.beginlisten()
    //     redis_.init_notify_handler(bind(&ChatService::redisMessageHandler, this, _1, _2));
    // }

    mysqlPool_ = ConnectionPool<MysqlConnection>::getInstance();
    mysqlPool_->init_pool("127.0.0.1",3306,"root","Lsg20041013.","chat",16,1024,60,5000);
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
            // 订阅相应的通道
            // redis_.subscribe(user.get_id());

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
                for (auto &group : grouplist)
                {
                    json grpjs;
                    grpjs["groupid"] = group.get_id();
                    grpjs["groupname"] = group.get_name();
                    grpjs["groupdesc"] = group.get_desc();
                    vector<string> uservec;
                    for (auto &user : group.get_number())
                    {
                        json js;
                        js["numberid"] = user.get_id();
                        js["numbername"] = user.get_name();
                        js["numberstate"] = user.get_state();
                        js["numberrole"] = user.get_role();
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
                resp["offlinemsglist"] = msglist;
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

    // 取消订阅通道
    // redis_.unsubscribe(userid);

    // 修改状态
    User user;
    user.set_id(userid);
    useroperate_.update_state(user);
}

// 注册信息的处理方法
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    //
    // 接收json对象中的数据
    string name = js["name"];
    string pwd = js["password"];
    cout << js.dump() << endl;
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
                break;
            }
        }
    }

    // 取消订阅channel
    // redis_.unsubscribe(user.get_id());

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

    // 查询是否在其他服务器上
    User user = useroperate_.query(toid);
    if (user.get_state() == "online")
    {
        // static thread_local Redis thread_redis;
        // static thread_local bool is_connected = false;

        // if (!is_connected)
        // {
        //     if (thread_redis.connect())
        //     {
        //         is_connected = true;
        //     }
        //     else
        //     {
        //         LOG_ERROR << "Redis connect failed";
        //         return;
        //     }
        // }

    //     // 现在这个 thread_redis 是当前线程独享的，绝对安全
    //     // thread_redis.publish(user.get_id(), js.dump());
    //     // redis_.publish(user.get_id(), js.dump());
    //     return;
    
    }
    // // 收件人不在线，将消息存储在数据库中，等下次上线时转发
    offlineMsgOperate_.insertOfflineMsg(toid, js.dump());
}

// 服务器断开时重置用户状态,静态
void ChatService::ServerOffline(int a)
{
    ChatService::instance()->useroperate_.resetUserState();
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
    vector<int> numid = groupOperata_.queryNumber(groupid, userid);

    // 逐个转发信息
    {
        lock_guard<mutex> lock(ConnMapMutex_);
        for (auto &num : numid)
        {
            auto it = userConnMap_.find(num);
            if (it != userConnMap_.end())
            {
                // 用户在线直接转发
                string chipertext = serverMsgEncrtpt(it->second, js);
                userConnMap_[num]->send(chipertext);
            }
            else
            {
                // 是否在其他服务器上登录
                User user = useroperate_.query(num);
                if (user.get_state() == "online")
                {
                    // 发布到redis中
                    // redis_.publish(num, js.dump());
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
void ChatService::redisMessageHandler(int userid, string message)
{
    lock_guard<mutex> lock(ConnMapMutex_);
    auto it = userConnMap_.find(userid);
    if (it != userConnMap_.end())
    {
        it->second->send(message);
        return;
    }

    // 用户已经下线
    offlineMsgOperate_.insertOfflineMsg(userid, message);
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
    cout<<"-----性能检测线程启动-----"<<endl;
    KeyGuard* ptr= KeyGuard::GetInstance();
    while (true)
    {
        cout<<"当前登录用户数："<< ptr->showMapSize() << endl;
        int last = totalMsgCount_.load();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        cout << "当前总共处理信息数：" << totalMsgCount_.load() << endl;
        cout<<"每秒处理信息数：" << (totalMsgCount_ - last)/5 << endl;     
    }
}