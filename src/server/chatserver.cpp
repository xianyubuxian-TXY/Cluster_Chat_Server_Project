#include "chatserver.hpp"
#include "json.hpp"
#include "chatservice.hpp"

#include<functional>
#include<string>
#include<muduo/base/Logging.h>
using namespace std;
using namespace placeholders;
using namespace nlohmann;

//构造函数
ChatServer::ChatServer(EventLoop* loop,const InetAddress& listenAddr,const string& nameArg)
: _server(loop,listenAddr,nameArg)
, _loop(loop)
{
    //注册链接的回调函数
    _server.setConnectionCallback(bind(&ChatServer::onConnection,this,_1));
    //注册消息回调的函数
    _server.setMessageCallback(bind(&ChatServer::onMessage,this,_1,_2,_3));
    //设置线程数量
    _server.setThreadNum(4);
}

//启动服务
void ChatServer::start(){
    _server.start();
}

// 连接链接的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    //客户端断开连接
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown(); //关闭链接，释放资源
    }
}

//读写事件相关的回调函数
void ChatServer::onMessage(const TcpConnectionPtr& conn,Buffer* buffer,Timestamp time)
{
    string buf=buffer->retrieveAllAsString();
    //数据的反序列化
    json js=json::parse(buf);

    //达到的目的："完全解耦"网络模块的代码和业务模块的代码——>通过“单例模式 + 回调函数”
    //通过js["msgid"]获取——>业务handler(conn  js  time)
    auto msgHandler=ChatService::instance()->getHandler(js["msgid"].get<int>());
    //回调消息绑定号的事件处理器，来执行相应的业务处理
    msgHandler(conn,js,time);
}