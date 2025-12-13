#ifndef KEYGUARD_HPP
#define KEYGUARD_HPP
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <muduo/net/TcpConnection.h>
#include <algorithm> // 用于 std::find
#include <cctype>    // 用于 isalnum
#include <mutex>
using namespace std;
using namespace muduo::net;

const int AES_KEY_LENGTH = 32; // 256位
class KeyGuard
{
private:
    string privatekey_;
    string publickey_;
    string aeskey_;
    unordered_map<string, string> aesKeyMap_;
    unordered_map<TcpConnectionPtr, string> ConntoAESkeyMap_;
    KeyGuard() = default;
    ~KeyGuard() = default;

    mutex ConntoAESkeyMapMutex_;

    // 在AES加解密函数中，每次都调用OpenSSL_add_all_algorithms()是不必要的，
    // 而且这个函数在OpenSSL 1.1.0+中已经废弃。
    // 通常在程序启动时初始化一次即可。

public:
    // 单例模式
    KeyGuard(const KeyGuard &) = delete;
    KeyGuard &operator=(const KeyGuard &) = delete;
    KeyGuard(KeyGuard &&) = delete;
    KeyGuard &operator=(KeyGuard &&) = delete;
    static KeyGuard *GetInstance();

    // AES加密解密
    string MsgAESEncrypt(const string &key, const string &msg);
    string MsgAESDecrypt(const string &key, const string &cipherTxt);
    void generateAESkey(string &key);
    void getAESkey(string &aeskey);
    // rsa加密
    void generateRSAkey(int keysize = 2048);
    string RsaEncrypt(const string &pubkey, string &text);
    string RsaDecrypt(const string &prikey, string &cipherText);
    void setPublickey(string publickey);
    void getPublickey(string &pubkey);
    void getPrivatekey(string &prikey);
    // AES密钥管理
    void addkey(const TcpConnectionPtr &conn, string aeskey);
    void removekey(const TcpConnectionPtr &conn);
    void updatekey(const TcpConnectionPtr &conn, string newaeskey);
    bool findAESkey(const TcpConnectionPtr &conn, string &aeskey);
    //base64编码
    string base64_decode(const std::string& encoded_string);
    string base64_encode(const std::string& raw_data);

    //Map大小
    int showMapSize();
};

#endif // KEYGUARD_HPP