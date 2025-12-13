#include "keyguard.hpp"

void KeyGuard::generateAESkey(string &key)
{
    key.resize(AES_KEY_LENGTH);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(&key[0]), AES_KEY_LENGTH) != 1)
    {
        cout << "生成AES密钥失败" << endl;
    }
    this->aeskey_ = key;
    return;
}

void KeyGuard::getAESkey(string &aeskey)
{
    aeskey = this->aeskey_;
}
KeyGuard *KeyGuard::GetInstance()
{
    static KeyGuard instance;
    return &instance;
}

string KeyGuard::MsgAESEncrypt(const string &key, const string &msg)
{
    // 使用链式AES加密
    // 第一步初始化,加载算法
    // OpenSSL_add_all_algorithms(); // 在新版OpenSSL中已废弃，且不应每次都调用
    // 第二步生成密钥和iv
    string iv(16, '0');
    RAND_bytes(reinterpret_cast<unsigned char *>(&iv[0]), 16);

    // 第三步创建上下文
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        cout << "create ctx error" << endl;
    }
    // 初始化上下文
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                       reinterpret_cast<const unsigned char *>(key.data()), reinterpret_cast<const unsigned char *>(iv.data()));

    // 第四步进行加密
    vector<unsigned char> cipherMsg(msg.size() + EVP_MAX_BLOCK_LENGTH); // 存储加密后的字符串，长度+32，分块加密可能加长
    int len1 = 0, len2 = 0;

    if (EVP_EncryptUpdate(ctx, &cipherMsg[0], &len1, reinterpret_cast<const unsigned char *>(msg.data()), msg.size()) != 1)
    {
        cout << "加密失败" << endl;
    }
    if (EVP_EncryptFinal_ex(ctx, &cipherMsg[len1], &len2) != 1)
    {
        cout << "加密失败" << endl;
    }
    EVP_CIPHER_CTX_free(ctx);
    return iv + string(cipherMsg.begin(), cipherMsg.begin() + len1 + len2);
}

string KeyGuard::MsgAESDecrypt(const string &key, const string &cipherTxt)
{
    string iv = cipherTxt.substr(0, 16);
    string ciphermsg = cipherTxt.substr(16);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        cout << "create ctx error" << endl;
    }
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           reinterpret_cast<const unsigned char *>(key.data()),
                           reinterpret_cast<const unsigned char *>(iv.data())) != 1)
    {
        cerr << "初始化失败" << endl;
    }

    vector<unsigned char> msg(ciphermsg.size() + 32);
    int len1 = 0, len2 = 0;

    // 第一次解密
    if (EVP_DecryptUpdate(ctx, msg.data(), &len1, reinterpret_cast<const unsigned char *>(ciphermsg.data()), ciphermsg.size()) != 1)
    {
        cerr << "解密失败1" << endl;
    }
    // 第二次解密
    if (EVP_DecryptFinal_ex(ctx, msg.data() + len1, &len2) != 1)
    {
        cerr << "解密失败2" << endl;
    }
    string result = string(msg.begin(), msg.begin() + len1 + len2);

    // 清理解密上下文
    EVP_CIPHER_CTX_free(ctx);
    return result;
}

// RSA加密
// 生成密钥对
void KeyGuard::generateRSAkey(int keysize)
{
    cout << "生成密钥对" << endl;
    // 生成rsa对象和大数
    RSA *rsa = RSA_new();
    BIGNUM *bn = BN_new();
    BN_set_word(bn, RSA_F4);
    // RSA_F4是经过验证的良好数字
    if (RSA_generate_key_ex(rsa, keysize, bn, nullptr) != 1)
    {
        cout << "创建rsa密钥失败" << endl;
        RSA_free(rsa);
        BN_free(bn);
    }

    // 导出公钥,创建基本输入输出BIO,从rsa中导入数据到BIO,在将数据导入字符串
    BIO *pubBio = BIO_new(BIO_s_mem());
    PEM_write_bio_RSAPublicKey(pubBio, rsa);
    char *pubKeyData = nullptr;
    int pubLen = BIO_get_mem_data(pubBio, &pubKeyData);
    this->publickey_ = string(pubKeyData, pubLen);
    // 导出私钥
    BIO *pribio = BIO_new(BIO_s_mem());
    PEM_write_bio_RSAPrivateKey(pribio, rsa, nullptr, nullptr, 0, nullptr, nullptr);
    char *priKeyData = nullptr;
    int priLen = BIO_get_mem_data(pribio, &priKeyData);
    this->privatekey_ = string(priKeyData, priLen);

    BIO_free(pubBio);
    BIO_free(pribio);
    RSA_free(rsa);
    BN_free(bn);
    return;
}

// rsa加密
string KeyGuard::RsaEncrypt(const string &pubkey, string &text)
{
    int maxLength = 245; // 密文加密的最大长度，rsa对加密长度有限制
    if (text.length() > maxLength)
    {
        cout << "无法加密，密文过长" << endl;
        return "";
    }

    // 读入公钥
    BIO *bio = BIO_new_mem_buf(pubkey.c_str(), -1);
    RSA *rsa = PEM_read_bio_RSAPublicKey(bio, nullptr, nullptr, nullptr);

    if (!rsa)
    {
        cout << "公钥加载失败" << endl;
        return "";
    }

    // RSA 加密操作产生的密文长度总是等于其密钥的模长
    int rsaSize = RSA_size(rsa);
    vector<unsigned char> encryted(rsaSize);

    int reslen = RSA_public_encrypt(text.length(),
                                    reinterpret_cast<const unsigned char *>(text.data()),
                                    &encryted[0], rsa, RSA_PKCS1_PADDING);

    RSA_free(rsa);
    BIO_free(bio);
    if (reslen == -1)
    {
        cout << "加密失败" << endl;
    }
    return string(encryted.begin(), encryted.begin() + reslen);
}

string KeyGuard::RsaDecrypt(const string &prikey, string &cipherText)
{
    // 读入私钥
    BIO *bio = BIO_new_mem_buf(prikey.c_str(), -1);
    RSA *rsa = PEM_read_bio_RSAPrivateKey(bio, nullptr, nullptr, nullptr);
    if (!rsa)
    {
        cout << "私钥加载失败" << endl;
        return "";
    }

    int rsaSize = RSA_size(rsa);
    vector<unsigned char> decrypted(rsaSize);

    int reslen = RSA_private_decrypt(cipherText.length(),
                                     reinterpret_cast<const unsigned char *>(cipherText.data()), decrypted.data(), rsa,
                                     RSA_PKCS1_PADDING);
    BIO_free(bio);
    if (reslen == -1)
    {
        cout << "解密失败" << endl;
        return "";
    }
    return string(decrypted.begin(), decrypted.begin() + reslen);
}

void KeyGuard::setPublickey(string publickey)
{
    this->publickey_ = publickey;
}
void KeyGuard::addkey(const TcpConnectionPtr &conn, string aeskey)
{
    lock_guard<mutex> lock(ConntoAESkeyMapMutex_);
    ConntoAESkeyMap_[conn] = aeskey;
    if(0==ConntoAESkeyMap_.size()%100)cout<<"Map的大小"<<ConntoAESkeyMap_.size()<<endl;
}

void KeyGuard::removekey(const TcpConnectionPtr &conn)
{
    lock_guard<mutex> lock(ConntoAESkeyMapMutex_);
    if (ConntoAESkeyMap_.find(conn) != ConntoAESkeyMap_.end())
    {
        ConntoAESkeyMap_.erase(conn);
    }
   if(0==ConntoAESkeyMap_.size()%100)cout<<"Map的大小"<<ConntoAESkeyMap_.size()<<endl;
}

void KeyGuard::updatekey(const TcpConnectionPtr &conn, string newaeskey)
{
    lock_guard<mutex> lock(ConntoAESkeyMapMutex_);
    ConntoAESkeyMap_[conn] = newaeskey;
}

bool KeyGuard::findAESkey(const TcpConnectionPtr &conn, string &aeskey)
{
    lock_guard<mutex> lock(ConntoAESkeyMapMutex_);
    if (ConntoAESkeyMap_.find(conn) != ConntoAESkeyMap_.end())
    {
        aeskey = ConntoAESkeyMap_[conn];
        return true;
    }
    return false;
}

void KeyGuard::getPublickey(string &pubkey)
{
    pubkey = publickey_;
}

void KeyGuard::getPrivatekey(string &prikey)
{
    prikey = privatekey_;
}

// Base64 字符集 (RFC 4648)
/**
 * @brief 将原始字节数据编码为 Base64 字符串。
 * @param raw_data 原始数据（可以是二进制数据，如您的 AES 密钥）。
 * @return Base64 编码后的字符串。
 */
std::string KeyGuard::base64_encode(const std::string &raw_data)
{
    const std::string BASE64_CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string encoded_string;
    int i = 0, j = 0;
    // 用于存储 3 个输入字节和 4 个输出 Base64 索引
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    size_t data_len = raw_data.length();
    // 将 string 转换为 const unsigned char*，以便按字节处理
    const unsigned char *bytes_to_encode = (const unsigned char *)raw_data.data();

    while (data_len--)
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
            // 3个8位字节（24位） -> 4个6位组
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;                                     // 取第1字节的前6位
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4); // 取第1字节的后2位 + 第2字节的前4位
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6); // 取第2字节的后4位 + 第3字节的前2位
            char_array_4[3] = char_array_3[2] & 0x3f;                                            // 取第3字节的后6位

            // 映射到 Base64 字符
            for (i = 0; (i < 4); i++)
            {
                encoded_string += BASE64_CHARS[char_array_4[i]];
            }
            i = 0;
        }
    }

    // 处理剩余不足 3 个字节的部分
    if (i)
    {
        for (j = i; j < 3; j++)
        {
            char_array_3[j] = '\0'; // 用 0 补齐
        }

        // 重新计算 4 个 6 位组
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        // 只有 (i + 1) 个字符是有效的 Base64 字符
        for (j = 0; (j < i + 1); j++)
        {
            encoded_string += BASE64_CHARS[char_array_4[j]];
        }

        // 使用 '=' 进行填充
        while ((i++ < 3))
        {
            encoded_string += '=';
        }
    }
    return encoded_string;
}

/**
 * @brief 将 Base64 字符串解码为原始字节数据。
 * @param encoded_string Base64 编码后的字符串。
 * @return 原始数据（可能是二进制数据）。
 */
std::string KeyGuard::base64_decode(const std::string &encoded_string)
{
    const std::string BASE64_CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    size_t in_len = encoded_string.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string decoded_string;

    // lambda 函数：检查字符是否是有效的 Base64 字符或填充符
    auto is_base64 = [](unsigned char c) -> bool
    {
        return (isalnum(c) || (c == '+') || (c == '/'));
    };

    // 循环处理 Base64 字符串，每次处理 4 个字符
    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
    {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4)
        {
            // 查找 Base64 字符在字符表中的索引（即 6 位值）
            for (i = 0; i < 4; i++)
            {
                char_array_4[i] = BASE64_CHARS.find(char_array_4[i]);
            }

            // 4个6位组（24位） -> 3个8位字节
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);         // 第1组 + 第2组的前2位
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2); // 第2组的后4位 + 第3组的前4位
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];                 // 第3组的后2位 + 第4组

            // 重新组合成 3 个字节
            for (i = 0; (i < 3); i++)
            {
                decoded_string += char_array_3[i];
            }
            i = 0;
        }
    }

    // 处理末尾的填充和剩余部分
    if (i)
    {
        for (j = i; j < 4; j++)
        {
            char_array_4[j] = 0;
        }

        for (j = 0; j < 4; j++)
        {
            char_array_4[j] = BASE64_CHARS.find(char_array_4[j]);
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++)
        {
            decoded_string += char_array_3[j];
        }
    }
    return decoded_string;
}


int KeyGuard::showMapSize()
{
    return ConntoAESkeyMap_.size();
}