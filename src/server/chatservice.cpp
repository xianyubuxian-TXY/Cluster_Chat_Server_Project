#include "chatservice.hpp"
#include "public.hpp"
#include<iostream>
#include <muduo/base/Logging.h>
#include<muduo/base/Logging.h>
using namespace muduo;

//获取单例对象的接口函数
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}

//构造函数
ChatService::ChatService(){
    //注册业务函数
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginOut,this,_1,_2,_3)});
    _msgHandlerMap.insert({GET_USERDATA_MSG,std::bind(&ChatService::getCurrentUserData,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroupChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroupChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});
}

//获取消息的处理器
MsgHandler ChatService::getHandler(int msgid){
    auto it=_msgHandlerMap.find(msgid);
    if(it!=_msgHandlerMap.end())
    {
        return _msgHandlerMap[msgid];
    }
    else  //msgid没有对应的事件处理回调
    {
        //返回一个默认处理器：空操作
        return [=](const TcpConnectionPtr& conn,json &js,Timestamp time){
            //使用<muduo/base/Logging.h>中的logging功能
            LOG_ERROR<<"msgid:"<<msgid<<" can't find handler!.";
        };
    }

}

//获取用户信息
json ChatService::getUserData(json& js)
{
    int id=js["id"].get<int>();
    string password=js["password"];
    User user=_userModel.query(id);
    json response;
    if(user.getId()==id && user.getPwd()==password)
    {
        //查询离线消息并返回
        vector<string> offlineMsgs=_offlineMsgModel.query(id); //获取离线消息
        if(!offlineMsgs.empty()) //消息非空
        {
            response["offlineMessages"]=offlineMsgs;  //将离线消息放入response，转发给用户
            _offlineMsgModel.remove(id); //删除该用户的离线消息
        }

        //查询好友信息并返回
        vector<string> friends;
        vector<User> friendsVec=_friendModel.query(id);
        if(!friendsVec.empty())
        {
            for(auto it:friendsVec)
            {
                json js;
                js["id"]=it.getId();
                js["name"]=it.getName();
                js["state"]=it.getState();
                friends.push_back(js.dump());
            }
            response["friends"]=friends;
        }

        //查询群组信息并返回
        vector<string> groups;
        vector<Group> groupsVec=_groupModel.queryGroups(id);
        if(!groupsVec.empty())
        {
            for(auto &it:groupsVec)
            {
                //群组信息
                json js;
                js["id"]=it.getId();
                js["name"]=it.getName();
                js["desc"]=it.getDesc();

                //群组内成员信息
                vector<string> groupUserVec;
                for(auto &usr: it.getUsers())
                {
                    json groupUser;
                    groupUser["id"]=usr.getId();
                    groupUser["name"]=usr.getName();
                    groupUser["state"]=usr.getState();
                    groupUser["role"]=usr.getRole();
                    groupUserVec.push_back(groupUser.dump());
                }
                js["groupUsers"]=groupUserVec;
                groups.push_back(js.dump());
            }
            response["groups"]=groups;
        }        
        //用户信息
        response["id"]=user.getId();
        response["name"]=user.getName();
    }
    return response;
}

//获取用户信息业务(id，password)
void ChatService::getCurrentUserData(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    json response=getUserData(js);
    response["msgid"]=GET_USERDATA_MSG;
    conn->send(response.dump());
}

//处理登录业务:根据 “id+password” 登录
void ChatService::login(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    LOG_INFO<<"user login";
    int id=js["id"].get<int>();
    string pwd=js["password"];

    User user=_userModel.query(id); //根据id获取user
    if(user.getId()==id && user.getPwd()==pwd) //用户存在且密码正确
    {
        json response;
         //已经处于登录状态，不能重复登录
        if(user.getState()=="online")
        {
            response["error"]=2;  //0:没出错  1：出错  2：已登录
            response["errmsg"]="user have logined. cann't login again!";
            response["msgid"]=LOGIN_MSG_ACK;
        }
        //登录成功
        else  
        {
            //更新用户状态 state offline->online ：user与数据库要同步更新
            user.setState("online");
            _userModel.updateState(user);

            //记录用户的“连接指针”，用于信息转发
            {
                lock_guard<mutex> lock(_connMutex); //线程安全
                _connPtrMap.insert({id,conn});
            }
            response=getUserData(js);
            
            response["error"]=0;
            response["msgid"]=LOGIN_MSG_ACK;
        }
        conn->send(response.dump()); //回复信息
    }
    else
    {
        //用户不存在或密码错误
        json response;
        response["error"]=1;
        response["errmsg"]="id or password invalid!";
        response["msgid"]=LOGIN_MSG_ACK;
        conn->send(response.dump());
    }
}

//处理注册业务
void ChatService::reg(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    //LOG_DEBUG<<"user reg";
    string name=js["name"];
    string pwd=js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    int res=_userModel.insert(user);
    if(res)  //注册成功
    {
        json response;
        response["msgid"]=REG_MSG_ACK; //消息为“注册回复类型”
        response["error"]=0; //0：注册成功  1：注册失败
        response["id"]=user.getId();
        conn->send(response.dump());
    }
    else //注册失败
    {
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["error"]=1;
        conn->send(response.dump());
    }
}

//单人聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    int toId=js["toid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_connPtrMap.find(toId); //寻找对应的“连接指针”
        if(it!=_connPtrMap.end())
        {
            //toId在线，服务器转发信息
            it->second->send(js.dump()); //服务器转发给目标用户
            return;
        }
    }

    //toId不在线，存储离线信息(是mysql操作，自动确保了线程安全)
    _offlineMsgModel.insert(toId,js.dump());
}

//添加好友业务: id friend
void ChatService::addFriend(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    LOG_INFO<<1;
    int userid=js["id"].get<int>();
    int friendid=js["friendid"].get<int>();
    LOG_INFO<<userid<<" "<<friendid;
    //添加好友信息
    _friendModel.insert(userid,friendid);
}

//创建群聊业务
void ChatService::createGroupChat(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    int userId=js["id"].get<int>();
    string groupName=js["groupname"];
    string groupDesc=js["groupdesc"];
    Group group(-1,groupName,groupDesc);
    //创建新群组信息
    if(_groupModel.createGroup(group))
    {
        //存储群主信息
        _groupModel.addGroup(userId,group.getId(),CREATOR);
    }
}

//用户加入群聊业务
void ChatService::addGroupChat(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    int userId=js["id"].get<int>();
    int groupId=js["groupid"].get<int>();
    _groupModel.addGroup(userId,groupId,NORMAL);
}

//群聊聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    //先获取群内成员
    int userId=js["userid"].get<int>();
    int groupId=js["groupid"].get<int>();
    vector<int> users=_groupModel.queryGroupUsers(userId,groupId);

    //因为muduo是多线程网络库：要确保线程安全
    lock_guard<mutex> lock(_connMutex);
    for(auto &id:users) //遍历群内每个成员
    {   
        auto it=_connPtrMap.find(id); //寻找成员与服务器的连接是否存在——>即用户是否在线
        if(it!=_connPtrMap.end())
        {
            //用户在线：转发群聊消息
            auto conn=it->second; //获取成员与服务器的连接
            conn->send(js.dump());
        }
        else
        {
            //用户不在线：存储离线群聊消息
            _offlineMsgModel.insert(id,js.dump());
        }
    }
}


//处理注销业务
void ChatService::loginOut(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_connPtrMap.find(userid);
        if(it!=_connPtrMap.end())
        {
            _connPtrMap.erase(it);
        }
    }

    //更新用户状态
    User user(userid,"","","offline");
    user.setState("offline");
    _userModel.updateState(user); //更新mysql中user的状态为“offline”
}

//用户退出:因为是用户退出时自动调用，而不是用户调用，所以不用放入Handler
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    lock_guard<mutex> lock(_connMutex);
    User user;

    for(auto it=_connPtrMap.begin();it!=_connPtrMap.end();++it)
    {
        if(it->second==conn) //找到对应的user
        {
            user.setId(it->first); //设置id
            _connPtrMap.erase(it); //从map中删除
            break;
        }
    }

    //更新用户状态 
    if(user.getId()!=-1) //检查id!=-1: 因为我们在运行过程中有创建临时user的情况，进一步安全保证
    {  
        user.setState("offline");
        _userModel.updateState(user); //更新mysql中user的状态为“offline”
    }
}



//服务端退出后，重置所有user的状态为offline
void ChatService::reset()
{
    _userModel.resetState();
}