#include "redis.hpp"
#include <iostream>

Redis::Redis()
    :_public_context(nullptr)
    ,_subcribe_context(nullptr)
{}

Redis::~Redis()
{
    if(_public_context!=nullptr)
    {
        redisFree(_public_context);
        _public_context=nullptr;
    }
    if(_subcribe_context!=nullptr)
    {
        redisFree(_subcribe_context);
        _subcribe_context=nullptr;
    }
}

//连接reids服务器
bool Redis::connect()
{
    //负责publish发布消息的上下文连接
    _public_context=redisConnect("127.0.0.1",6379); //连接redis服务器(ip,port)
    if(_public_context==nullptr)
    {
        cerr<<"connect redis failed!"<<endl;
        return false;
    }

    //负责subscribe订阅消息的上下文连接
    _subcribe_context=redisConnect("127.0.0.1",6379);
    if(_subcribe_context==nullptr)
    {
        cerr<<"connect redis failed!"<<endl;
        return false;
    }

    //在独立线程中接收消息：因为subscribe会阻塞当前线程，将接收消息的操作放在独立线程中运行
    //使用lambda表达式捕获this指针，调用成员函数observer_channel_message
    /*
    如果你把 & 改成了其他捕获方式，就可能无法正确访问类的成员函数。以下是常见的捕获方式：
        1.捕获值： [=]：所有外部变量按值捕获，this 指针无法访问类成员。
        2.捕获引用： [&]：所有外部变量按引用捕获，this 指针会被捕获，可以访问类的成员。
        3.捕获指定变量： [this, var1, var2]：仅捕获 this 和 var1、var2，其余变量不捕获。
    */
    thread t([&](){
        observer_channel_message();
    });
    t.detach(); //分离线程，独立运行

    cout<< "connect redis server success!" << endl;
    return true;
}

/*
redis三条指令：
1. PUBLISH channel message：向指定的通道发布消息
2. SUBSCRIBE channel：订阅指定的通道，接收该通道发布的消息
3. UNSUBSCRIBE channel：取消订阅指定的通道

注意：PUBLISH命令是“非阻塞”的，即执行完后立即返回结果，而SUBSCRIBE和UNSUBSCRIBE命令是“阻塞”的，即会一直等待通道上有消息发布
*/


/*
redisCommand命令解析：
1.先将客户端要发送的命令缓存到本地，即调用“redisAppendCommand函数”
2.然后将缓存区的命令发送到redis server，即调用“redisBufferWrite函数”
3.最后调用“redisGetReply函数”获取redis server的响应结果
   redisGetReply函数会“阻塞等待”redis server的响应结果，直到有结果返回
   如果没有结果返回，则会一直阻塞在这里，直到redis server有响应结果返回
*/

//向redis指定的通道channel发布消息
//为什么直接使用redisCommand？因为publish命令一执行redis server就会立即回复，不会阻塞
bool Redis::publish(int channel,string message)
{
    redisReply *reply=(redisReply*)redisCommand(_public_context,"PUBLISH %d %s",channel,message.c_str());
    if(nullptr==reply)
    {
        cerr<<"publish message error: "<<_public_context->errstr<<endl;
        return false;
    }
    //redisCommand的返回值是动态结构体，要释放掉
    freeReplyObject(reply);
    return true;
}

//向redis指定的通道subscribe订阅消息
//不直接使用redisCommand函数，而是拆分使用redisAppendCommand和redisBufferWrite函数,避免redisCommand中调用redisGetReply阻塞当前线程
bool Redis::subscribe(int channel)
{
    //SUBSRIBE命令本身会造成线程阻塞等待通道里面发来消息，这里只做订阅通道，不接收通道消息
    //通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    //只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源
    
    /*
    理论上，Redis 中可以有 无限数量 的频道，具体的限制取决于 Redis 服务器的内存和资源。
    故：用用户id进行频道订阅，用户id是唯一的，且不会超过Redis的限制
    */
    if(REDIS_ERR==redisAppendCommand(this->_subcribe_context,"SUBSCRIBE %d",channel))
    {
        cerr<<"subscribe channel error: "<<this->_subcribe_context->errstr<<endl;
        return false;
    }
    
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done=0;
    while(!done)
    {
        if(REDIS_ERR==redisBufferWrite(this->_subcribe_context,&done))
        {
            cerr<<"subscribe channel error: "<<this->_subcribe_context->errstr<<endl;
            return false;
        }
    }

    //redisGetReply

    return true;
}

//向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(int channel)
{
    //SUBSRIBE命令本身会造成线程阻塞等待通道里面发来消息，这里只做订阅通道，不接收通道消息
    //通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    //只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源
    
    if(REDIS_ERR==redisAppendCommand(this->_subcribe_context,"UNSUBSCRIBE %d",channel))
    {
        cerr<<"subscribe channel error: "<<this->_subcribe_context->errstr<<endl;
        return false;
    }
    
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done=0;
    while(!done)
    {
        if(REDIS_ERR==redisBufferWrite(this->_subcribe_context,&done))
        {
            cerr<<"subscribe channel error: "<<this->_subcribe_context->errstr<<endl;
            return false;
        }
    }

    //redisGetReply

    return true;
}

/*
typedef struct redisReply {
    int type;           // 回复的类型（例如：整数、字符串、数组等）
    long long integer;  // 如果回复是整数，存储整数值
    double dval;        // 如果回复是双精度数值，存储其值
    char *str;          // 如果回复是字符串，存储字符串的值
    size_t len;         // 字符串长度
    redisReply **element; // 如果回复是数组，存储各个元素
    size_t elements;    // 数组的元素数量
} redisReply;
*/

/*
redisReply是hiredis库中定义的结构体，用于存储redis server的响应结果：
    reply->element[0]：操作类型（"subscribe" 或 "message"）
    reply->element[1]：频道名称（即你订阅的频道名）
    reply->element[2]：消息内容（即发布到该频道的消息内容

注意：redisReply是动态分配的，要手动释放掉：freeReplyObject(reply)
*/

//在独立线程中接收订阅通道的消息
void Redis::observer_channel_message()
{
    //注意：这里的reply是动态分配的内存，需要在使用完后释放
    redisReply *reply=nullptr;
    while(REDIS_OK==redisGetReply(this->_subcribe_context,(void**)&reply))
    {
        //订阅收到的消息是一个带三元素的数组
        if(reply!=nullptr && reply->element[2]!=nullptr && reply->element[2]->str!=nullptr)
        {
            //给业务层上报通道上发生的消息: 回调函数(通道号，通道消息)
            _notify_message_handler(atoi(reply->element[1]->str),reply->element[2]->str);
        }

        freeReplyObject(reply);
    }

    cerr<<"observer channel message error: "<<this->_subcribe_context->errstr<<endl;
}

//初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int,string)> fn)
{
    this->_notify_message_handler=fn;
}