#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include<vector>
#include<string>
#include "user.hpp"
using namespace std;
//好友操作类
class FriendModel
{
public:
    //查询好友列表并返回：用户登录成功时给予用户
    vector<User> query(int userId);

    //添加好友
    void insert(int userId,int friendId);

};


#endif