#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

//User表的数据操作类
class UserModel{
public:
    //根据user插入信息
    bool insert(User &user);

    //通过id过去user
    User query(int id);

    //更新数据库中user的state
    bool updateState(User& user);
    
    //重置所有用户的状态为offline
    void resetState();
};

#endif