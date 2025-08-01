/*
muduo网络库给用户提供了两个主要的类
TcpServer：用于编写服务器程序
TcpClient：用于编写客户端程序

muduo库：epoll+线程池
好处：能够把网络I/O的代码和业务代码区分开
    1.网络I/O的代码有网络库封装好了，用户主要关注业务代码即可
    2.业务方面关心点：用户的连接和断开  用户的可读写事件
*/

#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<iostream>
#include<functional>
#include<string>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

/*基于muduo网络库开发服务器程序
1.组合TcpServer对象
2.创建EventLoop事件循环对象的指针
3.明确TcpServer构造函数需要什么参数，进而输出ChatServer的构造函数
4.在当前服务器类的构造函数中，注册处理连接的回调函数和处理读写事件的回调函数
5.设置合适的服务端线程数量，muduo库会自己分配I/O线程和worker线程
*/

class ChatServer{

public:
    ChatServer(EventLoop* loop,  //事件循环
    const InetAddress& listenAddr,  //IP+port
    const string& nameArg)  //服务器的名字
    : _server(loop,listenAddr,nameArg)
    , _loop(loop)
    {
        //给服务器”注册“用户连接的创建和断开的”回调函数“
        _server.setConnectionCallback(bind(&ChatServer::onConnection,this,_1));
        
        //给服务器”注册“用户读写事件的”回调函数“
        _server.setMessageCallback(bind(&ChatServer::onMessage,this,_1,_2,_3));

        //设置服务器端的线程数量 1个I/O线程   3个worker线程
        _server.setThreadNum(4);
    }

    //开启事件循环
    void start(){
        _server.start();
    }
    
    
private:
    //专门处理用户的连接创建和断开 epoll listenfd accept
    void onConnection(const TcpConnectionPtr& conn)
    {
        if(conn->connected()) //连接创建
        {
            cout<<conn->peerAddress().toIpPort()<<" -> "<<conn->localAddress().toIpPort()<<
            "state:online"<<endl;
        }
        else  //连接断开
        {
            cout<<conn->peerAddress().toIpPort()<<" -> "<<conn->localAddress().toIpPort()<<
            "state:offline"<<endl;
            conn->shutdown(); //close(fd)：回收fd资源
            //_loop->quit();  关闭服务器
        }
    }

    //专门处理用户读写事件
    void onMessage(const TcpConnectionPtr& conn, //连接  
                    Buffer* buf,  //缓冲区
                    Timestamp time)  //接收到数据的时间信息
    {
        string s=buf->retrieveAllAsString();  //将buf中的信息转为字符串
        string t=time.toString(); //将时间转为字符串
        cout<<"recv data:"<<s<<" time:"<<t<<endl;
        conn->send(buf); //原封返回
    }

    TcpServer _server; //#1   //muduo::net::TcpServer _server;
    EventLoop *_loop;  //# epoll

};

int main(){
    EventLoop loop; //epoll
    InetAddress addr("127.0.0.1",6000);
    ChatServer server(&loop,addr,"ChatServer");

    server.start();  //启动服务：将listenfd通过epoll_ctl添加到epoll上，从而可以启动epoll_wait等待新用户的连接
    loop.loop(); //epoll_wait以阻塞方式等待新用户连接，已连接用户的读写事件等

    return 0;
}