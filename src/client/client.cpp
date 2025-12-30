#include "client.hpp"
// 显示当前登录成功用户的基本信息

User currentUser;
vector<User> currentUserFriendList;
vector<Group> currentUserGroupList;
void showCurrentUserData()
{
    cout << "======================login user======================" << endl;
    cout << "current login user => id:" << currentUser.get_id() << " name:" << currentUser.get_name() << endl;
    cout << "----------------------friend list---------------------" << endl;
    if (!currentUserFriendList.empty())
    {
        for (User &user : currentUserFriendList)
        {
            cout << user.get_id() << " " << user.get_name() << " " << user.get_state() << endl;
        }
    }
    cout << "----------------------group list----------------------" << endl;
    if (!currentUserGroupList.empty())
    {
        for (Group &group : currentUserGroupList)
        {
            cout << group.get_id() << " " << group.get_name() << " " << group.get_desc() << endl;
            for (GroupUser &user : (*group.get_number()))
            {
                cout << user.get_id() << " " << user.get_name() << " " << user.get_state()
                     << " " << user.get_role() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

// 客户端登录业务
void login(int &clientfd)
{
    int id = 0;
    char pwd[50] = {0};
    while (true)
    {
        cout << "userid:";
        cin >> id;
        cin.get(); // 读掉缓冲区残留的回车
        if (id > 0 && id < INT32_MAX)
        {
            break;
        }
        else
        {
            cout << "invalid userid,try again!" << endl;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }
    cout << "userpassword:";
    cin.getline(pwd, 50);

    json js;
    js["msgid"] = LOGIN_MSG;
    js["id"] = id;
    js["password"] = pwd;
    string request = js.dump();

    // 加密
    request = msgEncryption(request);
    uint32_t sendLen = htonl(request.size());     // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    int len = send(clientfd, request.c_str(), request.size(), 0);
    if (len == -1)
    {
        cerr << "send login msg error:" << request << endl;
    }
    sem_wait(&acksem);
    if (userIsLogin)
    {
        // 进入菜单界面
        mainMenu(clientfd);
    }
}

// 客户端注册业务
void reg(int &clientfd)
{
    char name[50] = {0};
    char pwd[50] = {0};
    cout << "username:";
    cin.getline(name, 50);
    cout << "userpassword:";
    cin.getline(pwd, 50);

    json js;
    js["msgid"] = REG_MSG;
    js["name"] = name;
    js["password"] = pwd;
    string request = js.dump();
    // 加密
    request = msgEncryption(request);
    uint32_t sendLen = htonl(request.size());     // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    int len = send(clientfd, request.c_str(), request.length(), 0);
    if (len == -1)
    {
        cerr << "send reg msg error:" << request << endl;
    }
}

// 接收线程，由于接收服务端发送的消息
// mian线程用作于写线程，即发送消息内容
// 这样就可以同时接收和发送消息，不会出现发消息(阻塞)导致无法接收消息的情况
void readTaskHandler(int clientfd)
{
    bool keyflag = false;

    for (;;)
    {
        string buffer(10240, '0');
        int len = recv(clientfd, &buffer[0], 10240, 0);
        if (len == -1 || len == 0)
        {
            cerr << "recv error" << endl;
            exit(-1);
        }
        buffer = buffer.substr(0, len);
        if (keyflag)
        {
            // 解密字符串
            KeyGuard *keyguard = KeyGuard::GetInstance();
            string key;
            keyguard->getAESkey(key);
            buffer = keyguard->MsgAESDecrypt(key, buffer);
        }
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (msgtype == ONE_CHAT_MSG)
        {
            cout << js["name"] << ':' << js["msg"].get<string>() << endl;
        }
        if (msgtype == GROUP_CHAT_MSG)
        {
            cout << "[群消息：" << js["groupid"] << ']' << js["username"] << ':' << js["message"] << endl;
        }
        if (msgtype == REG_MSG_ACK)
        {
            if (0 == js["errno"])
            {
                cout << "register success! userid=" << js["userid"] << endl;
            }
            else
            {
                cout << "register failed! errmsg=" << js["errmsg"] << endl;
            }
        }
        if (msgtype == RAS_KEY_MSG)
        {
            // 保存rsa密钥发送aeskey
            keyHandler(clientfd, js);
            keyflag = true;
        }
        if (msgtype == CREATE_GROUP_MSG_ACK)
        {
            if (0 == js["errno"])
            {
                cout << "create group success! groupid=" << js["groupid"] << endl;
            }
            else
            {
                cout << "create group failed! errmsg=" << js["errmsg"] << endl;
            }
        }
        if (msgtype == LOGIN_MSG_ACK)
        {
            loginReaction(js);
            sem_post(&acksem);
        }
    }
}

void loginReaction(json response)
{
    if (0 == response["errno"])
    {
        // 登录成功，记录当前用户的id和name
        currentUser.set_id(response["userid"]);
        currentUser.set_name(response["name"]);
        userIsLogin = true;
        // 记录当前用户的好友列表信息
        if (response.contains("friendlist"))
        {
            // 初始化好友列表
            currentUserFriendList.clear();
            vector<string> vec = response["friendlist"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                User user;
                user.set_id(js["id"].get<int>());
                user.set_name(js["name"].get<string>());
                user.set_state(js["state"].get<string>());
                currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户的群组列表信息
        if (response.contains("grouplist"))
        {
            // 初始化群组列表
            currentUserGroupList.clear();
            vector<string> vec1 = response["grouplist"];
            for (string &groupstr : vec1)
            {
                json grpjs = json::parse(groupstr);
                Group group;
                group.set_id(grpjs["groupid"].get<int>());
                group.set_name(grpjs["groupname"].get<string>());
                group.set_desc(grpjs["groupdesc"].get<string>());

                vector<string> vec2 = grpjs["numberlist"];
                for (string &userstr : vec2)
                {
                    json userjs = json::parse(userstr);
                    GroupUser guser;
                    guser.set_id(userjs["numberid"].get<int>());
                    guser.set_name(userjs["numbername"].get<string>());
                    guser.set_state(userjs["numberstate"].get<string>());
                    guser.set_role(userjs["numberrole"].get<string>());
                    group.get_number()->push_back(guser);
                }
                currentUserGroupList.push_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 如果有离线消息，返回离线消息
        if (response.contains("offlinemsglist"))
        {
            vector<string> msglist = response["offlinemsglist"];
            cout << "------------------you have " << msglist.size() << " offline message------------------" << endl;
            for (string &offmsg : msglist)
            {
                json js = json::parse(offmsg);
                if (js["msgid"] == ONE_CHAT_MSG)
                {
                    cout << js["name"] << ':' << js["msg"].get<string>() << endl;
                }
                else
                {
                    cout << "[群消息：" << js["groupid"] << ']' << js["username"] << ':' << js["message"] << endl;
                }
            }
            cout << "------------------offline message end------------------" << endl;
        }
    }
    else
    {
        // 登录失败
        cerr << response["errmsg"] << endl;
        userIsLogin = false;
    }
}

string getCurrentTime()
{
    time_t now = time(nullptr);
    tm *ltm = localtime(&now);
    char timebuf[32] = {0};
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", ltm);
    return string(timebuf);
}

unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// 用于显示支持的命令
void help(int fd, string str)
{
    for (auto &command : commandMap)
    {
        cout << command.first << ":" << command.second << endl;
    }
}

//"聊天功能实现"
void chat(int clientfd, string str)
{
    // friendid:message
    int index = str.find(':');
    if (index == -1)
    {
        cerr << "the chat command is invalid!" << endl;
        return;
    }
    int friendid = atoi(str.substr(0, index).c_str());
    string message = str.substr(index + 1, str.size() - index);
    if (friendid <= 0 || friendid >= INT32_MAX)
    {
        cerr << "the friendid is invalid format" << endl;
        return;
    }

    // 封装json数据
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["from"] = currentUser.get_id();
    js["name"] = currentUser.get_name();
    js["to"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();

    // 发送数据
    string text = js.dump();
    string cipherText = msgEncryption(text);
    uint32_t sendLen = htonl(cipherText.size());  // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    int len = send(clientfd, cipherText.c_str(), cipherText.length(), 0);
    if (len == -1 || lenHead == -1)
    {
        cerr << "send massage failed" << endl;
    }
}
// 添加好友功能实现
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    if (friendid <= 0 || friendid >= INT32_MAX)
    {
        cerr << "the friendid is invalid format" << endl;
        return;
    }

    // 尝试连接服务器
    json js;
    js["id"] = currentUser.get_id();
    js["friendid"] = friendid;
    js["msgid"] = ADD_FRIEND_MSG;
    string text = js.dump();
    string cipherText = msgEncryption(text);
    uint32_t sendLen = htonl(cipherText.size());  // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    int len = send(clientfd, cipherText.c_str(), cipherText.length(), 0);
    if (len == -1)
    {
        cerr << "send massage failed" << endl;
    }
}
//"创建群组"功能实现
void creategroup(int clientfd, string str)
{
    // groupname:groupdesc
    int index = str.find(':');
    if (index == -1)
    {
        cerr << "the chat command is invalid!" << endl;
        return;
    }
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["groupname"] = str.substr(0, index);
    js["groupdesc"] = str.substr(index + 1, str.size() - index);
    js["userid"] = currentUser.get_id();

    string text = js.dump();
    string cipherText = msgEncryption(text);
    uint32_t sendLen = htonl(cipherText.size());  // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    int len = send(clientfd, cipherText.c_str(), cipherText.length(), 0);
    if (len == -1 || lenHead == -1)
    {
        cerr << "send massage failed" << endl;
    }
}
// 加入群组功能实现
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["groupid"] = groupid;
    js["userid"] = currentUser.get_id();
    // 加密发送数据
    string text = js.dump();
    string cipherText = msgEncryption(text);
    uint32_t sendLen = htonl(cipherText.size());  // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    int len = send(clientfd, cipherText.c_str(), cipherText.length(), 0);
    if (len == -1 || lenHead == -1)
    {
        cerr << "send massage failed" << endl;
    }
}
//"群聊"功能实现
void groupchat(int clientfd, string str)
{
    // groupid:message
    int index = str.find(':');
    if (index == -1)
    {
        cerr << "the chat command is invalid!" << endl;
        return;
    }
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["groupid"] = atoi(str.substr(0, index).c_str());
    js["message"] = str.substr(index + 1, str.size() - index);
    js["userid"] = currentUser.get_id();
    js["username"] = currentUser.get_name();

    string text = js.dump();
    string cipherText = msgEncryption(text);
    uint32_t sendLen = htonl(cipherText.size());  // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    int len = send(clientfd, cipherText.c_str(), cipherText.length(), 0);
    if (len == -1 || lenHead == -1)
    {
        cerr << "send massage failed" << endl;
    }
}

// "注销"功能实现
void loginout(int clientfd, string)
{
    json js;
    js["userid"] = currentUser.get_id();
    js["msgid"] = LOGINOUT_MSG;
    userIsLogin = false;
    // 发送数据
    string text = js.dump();
    string cipherText = msgEncryption(text);
    uint32_t sendLen = htonl(cipherText.size());  // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    int len = send(clientfd, cipherText.c_str(), cipherText.length(), 0);
    if (len == -1 || lenHead == -1)
    {
        cerr << "send massage failed" << endl;
    }
}
// 聊天菜单
void mainMenu(int clientfd)
{
    help();
    string buffer;
    string commandtype;
    while (userIsLogin)
    {
        cout << "---------------------" << endl;
        cout << "plese input command:" << endl;
        getline(cin, buffer);
        // 获取消息的类型
        int index = buffer.find(':');
        if (index == -1)
        {
            commandtype = buffer;
        }
        else
        {
            commandtype = buffer.substr(0, index);
        }

        auto it = commandHandlerMap.find(commandtype);
        if (it == commandHandlerMap.end())
        {
            cerr << "the command is invalid" << endl;
            continue;
        }

        // 调用对应类型的处理函数
        it->second(clientfd, buffer.substr(index + 1, buffer.size() - index));
    }
}
// 初始化密钥
void keyHandler(int &clientfd, json &js)
{
    KeyGuard *keyguard = KeyGuard::GetInstance();
    keyguard->setPublickey(js["publickey"].get<string>());
    string aeskey;
    string publickey;

    keyguard->getPublickey(publickey);
    keyguard->generateAESkey(aeskey);
    aeskey = keyguard->base64_encode(aeskey);
    // 发送AES密钥给服务器
    json aesjs;
    aesjs["msgid"] = AES_KEY_MSG;
    aesjs["aeskey"] = aeskey;
    string text = aesjs.dump();
    string cipherText = keyguard->RsaEncrypt(publickey, text);
    uint32_t sendLen = htonl(cipherText.size());  // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    int len = send(clientfd, cipherText.c_str(), cipherText.size(), 0);
    if (len == -1 || lenHead == -1)
    {
        cerr << "send aes key error" << endl;
    }
}

string msgEncryption(string &str)
{
    // 获取密钥
    KeyGuard *keyguard = KeyGuard::GetInstance();
    string aeskey;
    keyguard->getAESkey(aeskey);
    // 加密
    return keyguard->MsgAESEncrypt(aeskey, str);
}