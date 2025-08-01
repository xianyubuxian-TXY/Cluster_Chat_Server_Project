#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include<muduo/net/TcpConnection.h>
#include<unordered_map>
#include<functional>
#include<mutex>
using namespace std;
using namespace muduo;
using namespace muduo::net;

#include "json.hpp"
using json=nlohmann::json;

#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"

//表示处理消息的事件回调类型
using MsgHandler = function<void(const TcpConnectionPtr& conn,json &js,Timestamp)>;

//聊天服务器业务类（单例模式）
class ChatService{

public:
    //获取单例对象的接口函数
    static ChatService* instance();
    //获取消息的处理器
    MsgHandler getHandler(int msgid);
    //处理登录业务
    void login(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //单人聊天业务
    void oneChat(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //添加好友业务
    void addFriend(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //创建群聊业务
    void createGroupChat(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //用户加入群聊业务
    void addGroupChat(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //群聊聊天业务
    void groupChat(const TcpConnectionPtr& conn,json &js,Timestamp time);

    //除以用户的异常退出
    void clientCloseException(const TcpConnectionPtr& conn);
    //服务端退出后，重置所有user的状态为offline
    void reset();
private:
    ChatService();

    //存储消息id和其对应的业务处理方法
    unordered_map<int,MsgHandler> _msgHandlerMap;
    
    //记录用户的”连接指针“
    unordered_map<int,const TcpConnectionPtr&> _connPtrMap;
    //互斥锁：保证操作”连接指针“集合的线程安全
    mutex _connMutex;

    //用户数据操作类
    UserModel _userModel;

    //离线消息操作类
    OfflineMsgModel _offlineMsgModel;

    //好友操作类
    FriendModel _friendModel;

    //群聊操作类
    GroupModel _groupModel;
};  

#endif