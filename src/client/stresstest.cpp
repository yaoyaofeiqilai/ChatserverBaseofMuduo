#include "client.hpp"
#include "keyguard.hpp"
#include <fcntl.h>
#include <ctime>
#include <sys/resource.h>
atomic_bool userIsLogin;
sem_t acksem;
void func(int seq)
{
    string ip = "127.0.0.1";
    uint16_t port = atoi("8000");
    // 创建客户端网络嵌套字scoket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        cerr << "create scoket failed!";
        exit(-1);
    }

    int opt = 1;
    if (setsockopt(clientfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        cerr << "setsockopt SO_REUSEADDR failed!" << endl;
        close(clientfd);
        exit(-1);
    }

    // 设置SO_LINGER为0，避免TIME_WAIT
    struct linger ling = {1, 0}; // 启用，超时0
    setsockopt(clientfd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    // 填写客户端连接的服务器信息
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET; // 协议ipv4
    server.sin_addr.s_addr = inet_addr(ip.c_str());
    server.sin_port = htons(port);

    // 尝试连接服务器
    if (connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)) == -1)
    {
        cerr << "connect server failed!" << endl;
        close(clientfd);
        return;
    }

    // 密钥处理
    string aeskey;
    string publickey;
    string buffer(1024, '0');
    int len = recv(clientfd, &buffer[0], 1024, 0);
    if (len <= 0)
    {
        // 服务器关闭或出错，直接退出当前用户线程，不要抛异常
        // cerr << "用户" << seq << "连接已断开" << endl;
        close(clientfd);
        return;
    }
    buffer.resize(len);
    json jsone = json::parse(buffer);
    publickey = jsone["publickey"].get<string>();
    // 发送aes密钥
    KeyGuard *keyguard = KeyGuard::GetInstance();
    keyguard->generateAESkey(aeskey);
    string aeskey_base64 = keyguard->base64_encode(aeskey);
    // 发送AES密钥给服务器
    json aesjs;
    aesjs["msgid"] = AES_KEY_MSG;
    aesjs["aeskey"] = aeskey_base64;
    string text = aesjs.dump();
    string cipherText = keyguard->RsaEncrypt(publickey, text);
    uint32_t sendLen = htonl(cipherText.size());  // 转为网络字节序
    int lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    len = send(clientfd, cipherText.c_str(), cipherText.size(), 0);
    if (len == -1 || lenHead == -1)
    {
        cerr << "send aes key error" << endl;
    }
    len = recv(clientfd, &buffer[0], 1024, 0);
    if (len <= 0) // 接收aeskeyack
    {
        close(clientfd);
        cout << "接收异常" << endl;
        return;
        // if(seq%100==0)cout << seq<<"密钥交换成功" << endl;
    }

    // 登录字符串
    json js;
    js["msgid"] = LOGIN_MSG;
    js["id"] = seq + 10000;
    js["password"] = "123456";
    // 加密
    string logintext = js.dump();
    logintext = keyguard->MsgAESEncrypt(aeskey, logintext);
    // 发送
    sendLen = htonl(logintext.size());        // 转为网络字节序
    lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
    len = send(clientfd, logintext.c_str(), logintext.size(), 0);
    if (len == -1 || lenHead == -1)
    {
        cerr << "send login error" << endl;
    }

    // 接收响应
    buffer.resize(1000000);
    len = recv(clientfd, &buffer[0], 1000000, 0);
    if (len == -1)
    {
        cerr << "recv login ack error" << endl;
    }
    buffer.resize(len);
    // 解密
    buffer = keyguard->MsgAESDecrypt(aeskey, buffer);

    try
    { // 解析
        js = json::parse(buffer);
    }
    catch (json::exception &e)
    {
        cout << e.what() << endl;
        cout<<len<<endl;
        cout<<buffer<<endl;
        return;
    }
    if (js["msgid"].get<int>() == LOGIN_MSG_ACK && js["errno"].get<int>() == 0)
    {
        if (seq % 10 == 0)
            cout << "用户" << seq << "登录成功!" << endl;
    }
    else
    {
        cout << js["errmsg"].get<string>() << endl;
        close(clientfd);
        return;
    }

    // 设置fd为非阻塞
    int flag = fcntl(clientfd, F_GETFL, 0);
    fcntl(clientfd, F_SETFL, flag | O_NONBLOCK);

    srand((unsigned)time(NULL));
    // 循环发送接收信息
    while (true)
    {
        // 随机向一名用户发送信息
        json chatjs;
        chatjs["msgid"] = ONE_CHAT_MSG;
        int toid = rand() % 20000 + 10000;
        chatjs["to"] = toid;
        chatjs["from"] = seq + 10000;
        chatjs["name"] = "testuser" + to_string(seq);
        chatjs["msg"] = "hello,this is the test" + to_string(seq) + "s message";
        chatjs["time"] = getCurrentTime();

        string chattext = chatjs.dump();
        chattext = keyguard->MsgAESEncrypt(aeskey, chattext);
        sendLen = htonl(chattext.size());         // 转为网络字节序
        lenHead = send(clientfd, &sendLen, 4, 0); // 先发长度
        len = send(clientfd, chattext.c_str(), chattext.size(), 0);
        if (len == -1 || lenHead == -1)
        {
            cerr << "用户" << seq << "发送消息失败!" << endl;
        }
        // 接收信息
        buffer.resize(1024);
        len = recv(clientfd, &buffer[0], 1024, 0);
        if (len == -1 && errno == EAGAIN)
        {
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }
        else if (len != -1)
        {
            // buffer.resize(len);
            // buffer = keyguard->MsgAESDecrypt(aeskey, buffer);
            // js = json::parse(buffer);
            // if (js["msgid"].get<int>() == ONE_CHAT_MSG)
            // {
            //     cout << "用户" << seq << "收到来自用户" << js["from"] << "的消息:" << js["msg"].get<string>() << endl;
            // }
        }
        else
        {
            cout << "出现错误！！！用户" << seq << "已退出" << endl;
            break;
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
}
int main()
{
    int start;
    cin >> start;
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = 100000;
    rl.rlim_max = 100000;
    setrlimit(RLIMIT_NOFILE, &rl);

    // 提高进程/线程数限制
    rl.rlim_cur = 100000;
    rl.rlim_max = 100000;
    setrlimit(RLIMIT_NPROC, &rl);
    this_thread::sleep_for(chrono::seconds(1));
    vector<thread> thrs;

    const int usercount = 5000;
    for (int i = start; i < usercount + start; ++i)
    {
        thrs.emplace_back(func, i);
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    cout << "所有用户线程创建完毕!" << endl;

    thread t([&]()
             {
        while(true)
        {
            cout<<getCurrentTime()<<endl;
            this_thread::sleep_for(chrono::seconds(1));
        } });
    t.detach();

    for (auto &thr : thrs)
    {
        thr.join();
    }
    return 0;
}