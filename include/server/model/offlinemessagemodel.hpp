#ifndef OFFLINEMESSAGEMODEL_H
#define OFFLINEMESSAGEMODEL_H

#include<string>
#include<vector>
using namespace std;

//操作用户离线消息类（当用户登录后，将离线消息全部转发给用户并删除）
class OfflineMsgModel{
public:
    //插入离线消息
    void insert(int id,string msg);

    //删除离线消息（用户登录后，将离线消息发送用户并要删除）
    void remove(int id);

    //查询离线消息并返回
    vector<string> query(int id);
};


#endif