#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"

//Group与GroupUser共同的句柄类
class GroupModel{
public:
    //在mysql插入group
    bool createGroup(Group& group);
    //加入群
    void addGroup(int userid,int groupid,string role);
    //user查询用户所在群组信息
    vector<Group> queryGroups(int userid);
    //user根据groupid查询所在群内处自己外的所有用户id（用于群聊：每一个转发即可）
    vector<int> queryGroupUsers(int userid,int groupid);
};


#endif