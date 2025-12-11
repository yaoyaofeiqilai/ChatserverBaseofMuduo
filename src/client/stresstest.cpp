#include "client.hpp"
#include "keyguard.hpp"
#include <fcntl.h>
#include <ctime>
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
        exit(-1);
    }

    // 密钥处理
    string aeskey;
    string publickey;
    string buffer(1024, '0');
    int len = recv(clientfd, &buffer[0], 1000, 0);
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
    len = send(clientfd, cipherText.c_str(), cipherText.size(), 0);
    if (len == -1)
    {
        cerr << "send aes key error" << endl;
    }
    if (recv(clientfd, &buffer[0], 1024, 0) != -1) // 接收aeskeyack
    {
        //if(seq%100==0)cout << seq<<"密钥交换成功" << endl;
    }
    else{
        cout<<"出现错误1"<<endl;
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
    len = send(clientfd, logintext.c_str(), logintext.size(), 0);
    if (len == -1)
    {
        cerr << "send login error" << endl;
    }

    // 接收响应
    buffer.resize(1024);
    len = recv(clientfd, &buffer[0], 1024, 0);
    if (len == -1)
    {
        cerr << "recv login ack error" << endl;
    }
    buffer.resize(len);
    // 解密
    buffer = keyguard->MsgAESDecrypt(aeskey, buffer);
    // 解析
    js = json::parse(buffer);
    if (js["msgid"].get<int>() == LOGIN_MSG_ACK && js["errno"].get<int>() == 0)
    {
        if(seq%10==0)cout << "用户" << seq << "登录成功!" << endl;
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
        int toid = rand() % 1000 + 10000;
        chatjs["to"] = toid;
        chatjs["from"] = seq + 10000;
        chatjs["name"] = "testuser" + to_string(seq);
        chatjs["msg"] = "你好,这是用户test" + to_string(seq) + "的消息";
        chatjs["time"] = getCurrentTime();

        string chattext = chatjs.dump();
        chattext = keyguard->MsgAESEncrypt(aeskey, chattext);
        len = send(clientfd, chattext.c_str(), chattext.size(), 0);
        if(seq==0)
        {
            cout<<"当前时间："<<getCurrentTime()<<endl;
        }
        if(len==-1)
        {
            cerr<<"用户"<<seq<<"发送消息失败!"<<endl;
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
        else{
            cout<<"出现错误！！！用户"<<seq<<"已退出"<<endl;
            break;
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
}
int main()
{
    vector<thread> thrs;
    const int usercount = 1000;
    for (int i = 0; i < usercount; ++i)
    {
        thrs.emplace_back(func, i);
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    for (auto &thr : thrs)
    {
        thr.join();
    }
    return 0;
}