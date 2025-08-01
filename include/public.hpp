#ifndef PUBLIC_H
#define PUBLIC_H

/*
server和client的公共文件
*/

#define CREATOR "creator"
#define NORMAL "normal"

//消息类型
enum EnMsgType
{
    LOGIN_MSG=1, //登录消息
    LOGIN_MSG_ACK,
    REG_MSG,  //注册消息
    REG_MSG_ACK,  //注册消息回复
    ONE_CHAT_MSG, //单人聊天信息
    ADD_FRIEND_MSG, //添加好友信息
    CREATE_GROUP_MSG, //创建群信息
    ADD_GROUP_MSG, //加入群信息
    GROUP_CHAT_MSG, //群聊信息
};

#endif