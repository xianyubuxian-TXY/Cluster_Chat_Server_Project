#include "user.hpp"
#include "group.hpp"
#include "public.hpp"

#include<vector>
#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include <json.hpp>
#include<thread>
#include<semaphore.h>
#include<atomic>
#include <regex>
using namespace std;
using json=nlohmann::json;

//当前系统登录用户
User g_currentUser;
//当前用户好友列表
vector<User> g_currentUserFriendList;
//当前用户所在群组列表(登录时的列表)
vector<Group> g_currentUserGroupList;


//控制主菜单页面程序
bool isMainMenuRunning=false;

//用于读写线程之间的通信
sem_t rwsem;
//记录登录状态是否成功
atomic_bool g_isLoginSuccess{false};

//接收信息线程
void readTaskHandler(int clientfd);
//获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();
//主聊天页面
void mainMenu(int clientfd);
//从mysql中获取当前用户信息
void getCurrentUserData(json& response);
//用于登录时展示当前用户的基本信息（登录时状态）
void showCurrentUserData();

//聊天客户端程序实现：main线程用作发送信息线程，子线程用作读取信息线程
int main(int argc,char* argv[])
{
    if(argc!=3)
    {
        cerr<<"command invalid. Example: ./ChatClient 127.0.0.1 6000"<<endl;
        exit(-1);
    }
    //获取参数:ip port
    char* ip=argv[1];
    uint16_t port=atoi(argv[2]);


    int clientfd=socket(AF_INET,SOCK_STREAM,0);
    if(clientfd<0)
    {
        cerr<<"create socket failed"<<endl;
        exit(-1);
    }

    sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=inet_addr(ip);

    if(connect(clientfd,(sockaddr*)&addr,sizeof(addr))<0)
    {
        cerr<<"connect server failed."<<endl;
        close(clientfd);
        exit(-1);
    }

    //初始化读写线程通信的信号量
    sem_init(&rwsem,0,0);

    //连接服务器成功，启动接收子线程
    thread readTask(readTaskHandler,clientfd);
    readTask.detach(); //设置分离线程

    //main线程用于接收用户输入，负责发送数据
    while(1)
    {
        //显示首页面菜单：登录、注册、退出
        cout<<"=============================="<<endl;
        cout<<"1.login"<<endl;
        cout<<"2.register"<<endl;
        cout<<"3.exit"<<endl;
        cout<<"=============================="<<endl;
        cout<<"choice:";
        int choice=0;
        cin>>choice;
        cin.get();  //读掉缓冲区残留的回车
        
        //处理输入字母的情况
        if(cin.fail())
        {
            //清除错误标志
            cin.clear();
            // 丢弃无效输入直到遇到换行符
            cin.ignore(numeric_limits<streamsize>::max(),'\n');
            cerr<<"input is invalied"<<endl;
            continue;
        }


        switch(choice)
        {
            //登录
            case 1:
            {
                int id;
                char password[50]={0};
                cout<<"userid:";
                cin>>id;
                cin.get(); //读掉缓冲区残留的回车
                cout<<"user's password:";
                cin.getline(password,50); //cin不允许读取“空格”，cin.getline可以读取“空格”
                //序列化
                json js;
                js["msgid"]=LOGIN_MSG; //业务标识
                js["id"]=id;
                js["password"]=password;
                //转换为字节流，用于发送数据
                string request=js.dump();

                g_isLoginSuccess=false;

                int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len<0)
                {
                    cerr<<"send login msg error:"<<request<<endl;
                }

                //等待信号量，有子线程处理完登录的响应消息后，通知这里
                sem_wait(&rwsem);

                if(g_isLoginSuccess)
                {
                    //进入聊天组菜单页面
                    isMainMenuRunning=true;
                    mainMenu(clientfd);
                }
            }
            break;
            //注册
            case 2:
            {
                char name[50]={0};
                char password[50]={0};
                cout<<"username:"<<endl;
                cin.getline(name,50);
                cout<<"password:"<<endl;
                cin.getline(password,50);
                //序列化
                json js;
                js["msgid"]=REG_MSG; //业务标识
                js["name"]=name;
                js["password"]=password;
                string request=js.dump();
                int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len<0)
                {
                    cerr<<"send register msg error!"<<endl;
                }

                //等待信号量，子线程处理完注册的响应消息后，通知这里
                sem_wait(&rwsem); 
            }
            break;
            case 3:
                close(clientfd);
                //释放信号量
                sem_destroy(&rwsem);
                exit(0);
                break;
            default:
                cerr<<"invalid input!"<<endl;
                break;
        }
    }
    return 0;
}

//处理登录的响应逻辑
void doLoginResponse(json responsejs)
{
    int error=responsejs["error"].get<int>();
    if(0!=error)
    {
        //0:登录失败  2:重复登录 
        cerr<<"login failed:"<<responsejs["errmsg"]<<endl;
        g_isLoginSuccess=false;
    }
    else
    {
        //登录成功
        //设置用户信息
        getCurrentUserData(responsejs);
        //打印用户信息、好友列表、群组列表
        showCurrentUserData(); 
        cout<<3<<endl;

        //显示当前用户的离线消息
        if(responsejs.contains("offlineMessages"))
        {
            vector<string> vec=responsejs["offlineMessages"];
            for(auto &it:vec)
            {
                json js=json::parse(it);
                //time+[id]+name+”said:“+xxx
                int msgtype=js["msgid"];
                if(ONE_CHAT_MSG==msgtype)
                {
                    cout<<js["time"].get<string>()<<" ["<<js["id"]<<"] "<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
                }
                else if(GROUP_CHAT_MSG==msgtype)
                {
                    cout<<"群消息[groupid:"<<js["groupid"]<<"] "<<js["time"].get<string>()<<" ["<<js["userid"].get<int>()<<"] "<<js["username"].get<string>()<<" said: "<<js["message"].get<string>()<<endl;
                }
            }
        }

        g_isLoginSuccess=true;
    }
}

//处理注册的响应逻辑
void doRegResponse(json responsejs)
{

    if(0!=responsejs["error"].get<int>())
    {
        //注册失败
        cerr<<"name is already exist, register error!"<<endl;
    }
    else
    {
        //注册成功
        cout<<"register success! "<<"your userid="<<responsejs["id"]<<",please remember it!"<<endl;
    }

}

//接收信息线程
void readTaskHandler(int clientfd)
{
    while(1)
    {
        char buffer[10240]={0};
        int len=recv(clientfd,buffer,sizeof(buffer),0);
        if(len<0)
        {
            cerr<<"recv failed"<<endl;
            close(clientfd);
            exit(-1);
        }
        //接收ChatServer转发的数据，反序列化生成json数据对象
        json js=json::parse(buffer);
        int msgtype=js["msgid"].get<int>();
        if(LOGIN_MSG_ACK==msgtype)
        {
            //处理登录响应的业务逻辑
            doLoginResponse(js);
            //通知主线程：登录结果处理完成
            sem_post(&rwsem);
        }
        else if(REG_MSG_ACK==msgtype)
        {
            doRegResponse(js);
            //通知主线程：注册结果处理完成
        sem_post(&rwsem);
        }
        else if(ONE_CHAT_MSG==msgtype)
        {
            cout<<js["time"].get<string>()<<" ["<<js["id"]<<"] "<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
        }
        else if(GROUP_CHAT_MSG==msgtype)
        {
            cout<<"buffer"<<buffer<<endl;
            cout<<js["time"].get<string>()<<" 群消息[groupid:"<<js["groupid"].get<int>()<<"] "<<" ["<<js["userid"].get<int>()<<"] "<<js["username"].get<string>()<<" said: "<<js["message"].get<string>()<<endl;
        }
        else if(GET_USERDATA_MSG==msgtype)
        {
            getCurrentUserData(js);
            showCurrentUserData();
        }
    }
}

//从mysql中获取当前用户信息
void getCurrentUserData(json &response)
{
    g_currentUserFriendList.clear();
    g_currentUserGroupList.clear();

    g_currentUser.setId(response["id"].get<int>());
    g_currentUser.setName(response["name"]);
    //获取用户好友列表
    if(response.contains("friends")) //存在好友列表
    {
        vector<string> friendVec=response["friends"];
        for(string &str:friendVec)
        {
            json js=json::parse(str);
            User user;
            user.setId(js["id"].get<int>());
            user.setName(js["name"]);
            user.setState(js["state"]);
            g_currentUserFriendList.push_back(user);
        }
    }
    //获取用户所在群组
    if(response.contains("groups")) //存在群组
    {
        vector<string> groupsVec=response["groups"];
        for(string& groupstr:groupsVec) //获取每个群组
        {
            json groupJs=json::parse(groupstr); //反序列化
            Group group;
            group.setId(groupJs["id"].get<int>());
            group.setName(groupJs["name"]);
            group.setDesc(groupJs["desc"]);

            vector<string> userVec=groupJs["groupUsers"];
            for(string& userstr:userVec) //获取群内每个用户
            {
                json userjs=json::parse(userstr);//反序列化
                GroupUser user;
                user.setId(userjs["id"].get<int>());
                user.setName(userjs["name"]);
                user.setState(userjs["state"]);
                user.setRole(userjs["role"]);
                group.getUsers().push_back(user);
            }
            g_currentUserGroupList.push_back(group);
        }
    }
}


//用于登录是展示自己的用户信息
void showCurrentUserData()
{
    cout<<"================login user==============="<<endl;
    cout<<"current login user => id:"<<g_currentUser.getId()<<" name:"<<g_currentUser.getName()<<endl;
    cout<<"---------------friend list---------------"<<endl;
    if(!g_currentUserFriendList.empty())
    {
        for(auto &it:g_currentUserFriendList)
        {
            cout<<"userid:"<<it.getId()<<" "<<"name:"<<it.getName()<<" "<<"state:"<<it.getState()<<endl;
        }    
    }
    cout<<"---------------group list----------------"<<endl;
    if(!g_currentUserGroupList.empty())
    {
        for(auto &group:g_currentUserGroupList)
        {
            cout<<"groupid:"<<group.getId()<<" "<<"name:"<<group.getName()<<" "<<"desc:"<<group.getDesc()<<endl;
            cout<<"成员列表："<<endl;
            for(auto &user:group.getUsers())
            {
                cout<<"\t"<<"userid:"<<user.getId()<<" "<<"username:"<<user.getName()<<" "<<"userrole:"<<user.getRole()<<" "<<"userstate:"<<user.getState()<<endl;
            }
        }
    }
    cout<<"========================================="<<endl;
}

//"help" command handler
void help(int fd=0,string str="");
//"showcurrentuserdata" command handler
void showcurrentuserdata(int,string);
//"chat" command handler
void chat(int,string);
//"addfriend" command handler
void addfriend(int,string);
//"creategroup" command handler
void creategroup(int,string);
//"addgroup" command handler
void addgroup(int,string);
//"groupchat" command handler
void groupchat(int,string);
//"loginout" command handler
void loginout(int,string);

//系统支持的客户端命令列表
unordered_map<string,string> commandMap={
    {"help","显示所有支持的命令,格式 help"},
    {"show_curren_user_data","展示用户的信息(包括好友列表、所在群组列表等),格式 show_user_current_data:userid:password"},
    {"chat","一对一聊天,格式 chat:friendid:message"},
    {"addfriend","添加好友,格式 addfriend:friendid"},
    {"creategroup","创建群聊,格式 creategroup:groupname:groupdesc"},
    {"addgroup","加入群聊,格式 addgroup:groupid"},
    {"groupchat","群聊,格式 groupchat:groupid:message"},
    {"loginout","注销,格式 loginout"}
};

//注册系统支持的客户端命令处理 void(int,string)：clientfd  message
unordered_map<string,function<void(int,string)>> commandHandlerMap={
    {"help",help},
    {"show_current_user_data",showcurrentuserdata},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"loginout",loginout}
};

//主聊天页面
void mainMenu(int clientfd)
{
    //展示命令列表
    help();
    char buffer[1024]={0};
    while(isMainMenuRunning)
    {
        memset(buffer,0,sizeof(buffer));
        //读取命令
        cin.getline(buffer,1024);
        //解析命令
        string commandbuf(buffer); //便于使用string对象来解析命令
        string command; //命令
        int idx=commandbuf.find(":"); //返回第一个":"的下标
        //help、loginout
        if(idx==-1)
        {
            command=commandbuf;
        }
        //其它命令
        else
        {
            command=commandbuf.substr(0,idx); //(下标，子串长度)
        }
        //命令存在
        auto it=commandHandlerMap.find(command);
        if(it!=commandHandlerMap.end())
        {
            it->second(clientfd,commandbuf.substr(idx+1)); //substr(idx+1): idx+1开始所有字符
        }
        else
        {
            cout<<"command not find."<<endl;    
        }
    }
}

void help(int,string)
{
    cout<<"show command List"<<endl;
    for(auto &it:commandMap)
    {
        cout<<it.first<<":"<<it.second<<endl;
    }
    cout<<endl;
}

//通过“正则表达式”判断是否能够string转化为int类型：处理 “addfriend”  “addfriend：asd” 等id无效情况  
bool getid(const std::string& str, int& id) {
    std::regex pattern("^[-+]?[0-9]+$");  // 匹配正整数、负整数、可选的符号
    if (std::regex_match(str, pattern)) {
        try {
            id = std::stoi(str);  // 转换为整数
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    return false;  // 如果不符合整数的格式，直接返回 false
}

//登陆后获取用户信息（随实时变动）
void showcurrentuserdata(int clientfd,string str)
{
    int idx=str.find(':');
    if(idx==-1)
    {
        cerr<<"show_current_user_data command is invalied"<<endl;
        return;
    }
    int userid=0;
    string useridstr=str.substr(0,idx);
    if(!getid(useridstr,userid))
    {
        cerr<<"show_current_user command is invalied"<<endl;
        return;
    }
    string password=str.substr(idx+1);
    json js;
    js["msgid"]=GET_USERDATA_MSG;
    js["id"]=userid;
    js["password"]=password;
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len<0)
    {
        cerr<<"show_current_user send error"<<endl;
        return;
    }
}



//数据格式：看readTaskHandler怎么解析
void chat(int clientfd,string str)
{
    //寻找第一个":"下标
    int idx=str.find(":");
    if(-1==idx)
    {
        cerr<<"chat command is invalid"<<endl;
        return;
    }

    //获取好友id
    int friendid=-1;
    if(!getid(str.substr(0,idx),friendid))
    {
        cerr<<"chat command is invalied"<<endl;
        return;
    }
    //获取消息内容
    string message=str.substr(idx+1);
    //序列化任务
    json js;
    js["msgid"]=ONE_CHAT_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["toid"]=friendid;
    js["msg"]=message;
    js["time"]=getCurrentTime();
    string buffer=js.dump();
    //发送消息
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len<0)
    {
        cerr<<"send msg error"<<endl;
    }
}

void addfriend(int clientfd,string str)
{
    //获取好友id
    // cout<<"addfriend:"<<str<<endl;
    int friendid=-1;
    if(!getid(str,friendid))
    {
        cerr<<"addfriend command is invalied"<<endl;
        return;
    }
    //序列化命令
    json js;
    js["msgid"]=ADD_FRIEND_MSG;
    js["id"]=g_currentUser.getId();
    js["friendid"]=friendid;
    string buffer=js.dump();
    //发送命令
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len<0)
    {
        cerr<<"send addfriend msg error->"<<buffer<<endl;
    }
}

//"creategroup" command handler
void creategroup(int clientfd,string str)
{
    //解析字符串
    int idx=str.find(':');
    if(-1==idx)
    {
        cerr<<"creategroup command is invalied"<<endl;
        return;
    }
    string groupname=str.substr(0,idx);
    string groupdesc=str.substr(idx+1);
    //序列化
    json js;
    js["msgid"]=CREATE_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupname"]=groupname;
    js["groupdesc"]=groupdesc;

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len<0)
    {
        cerr<<"send creategroup msg error"<<endl;
    }
}

//"addgroup" command handler
void addgroup(int clientfd,string str)
{
    int groupid=0;
    //判断str是否可以转换为整数，同时获取groupid
    if(!getid(str,groupid))
    {
        cerr<<"addgroup command is invalied"<<endl;
        return;
    }
    //序列化
    json js;
    js["msgid"]=ADD_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupid"]=groupid;
    
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len<0)
    {
        cerr<<"send addgroup msg error"<<endl;
    }
}

//"groupchat" command handler
//数据格式：看readTaskHandler怎么解析
void groupchat(int clientfd,string str)
{
    //解析字符串
    int idx=str.find(':');
    if(-1==idx)
    {
        cerr<<"groupchat command is invalied"<<endl;
        return;
    }
    string groupidstr=str.substr(0,idx);
    int groupid=0;
    if(!getid(groupidstr,groupid))
    {
        cerr<<"groupchat command id invalied"<<endl;
        return;
    }
    string message=str.substr(idx+1);
    //序列化
    json js;
    js["msgid"]=GROUP_CHAT_MSG;
    js["userid"]=g_currentUser.getId();
    js["username"]=g_currentUser.getName();
    js["groupid"]=groupid;
    js["message"]=message;
    js["time"]=getCurrentTime();

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len<0)
    {
        cerr<<"send groupchat msg error"<<endl;
    }
}

//"loginout" command handler
void loginout(int clientfd,string)
{
    json js;
    js["msgid"]=LOGINOUT_MSG;
    js["id"]=g_currentUser.getId();

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len<0)
    {
        cerr<<"loginout send error"<<endl;
    }
    else
    {
        isMainMenuRunning=false;
    }
}

//获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    // 获取当前时间点
    auto now = std::chrono::system_clock::now();
    
    // 将时间点转换为时间_t 类型
    time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    string ts=ctime(&now_time_t);
    ts.resize(ts.size()-1);
    return ts;
}