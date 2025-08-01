#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>
using namespace std;


void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}


int main(){

    signal(SIGINT,resetHandler); //设置信息处理函数
    EventLoop loop;
    InetAddress addr("127.0.0.1",6000);
    ChatServer server(&loop,addr,"ChatServer");
    server.start();
    loop.loop();

}