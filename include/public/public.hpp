#ifndef  PUBLIC_HPP
#define  PUBLIC_HPP

//定义消息的类型
enum MsgType
{
    LOGIN_MSG=1,  //登录消息
    LOGIN_MSG_ACK,
    LOGINOUT_MSG, //注销消息
    REG_MSG,   //注册消息
    REG_MSG_ACK, //注册消息的回应消息
    ONE_CHAT_MSG,  //一对一聊天消息
    Offline_MSG , //离线消息
    ADD_FRIEND_MSG, //添加好友消息
    ADD_FRIEND_MSG_ACK,

    CREATE_GROUP_MSG, //创建群组
    CREATE_GROUP_MSG_ACK,
    ADD_GROUP_MSG, //加入群组
    ADD_GROUP_MSG_ACK,
    GROUP_CHAT_MSG, //群组聊天消息

    RAS_KEY_MSG, //rsa公钥传输消息
    AES_KEY_MSG, //aes密钥传输消息
    AES_KEY_MSG_ACK
};
#endif