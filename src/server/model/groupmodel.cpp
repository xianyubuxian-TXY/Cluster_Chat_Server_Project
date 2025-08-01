#include "groupmodel.hpp"
#include "db.hpp"

//在mysql插入group
bool GroupModel::createGroup(Group& group)
{
    char sql[1024];
    sprintf(sql,"insert into AllGroup (groupname,groupdesc) values('%s','%s')"
            ,group.getName().c_str(),group.getDesc().c_str());
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            //创建成功后，设置群id
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

//加入群聊
void GroupModel::addGroup(int userid,int groupid,string role)
{
    char sql[1024];
    sprintf(sql,"insert into GroupUser values(%d,%d,'%s')",groupid,userid,role.c_str());

    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}

//user查询用户所在群组信息,组内成员
vector<Group> GroupModel::queryGroups(int userid)
{
    char sql[1024];
    //获取群组信息
    sprintf(sql,"select a.id,a.groupname,a.groupdesc from AllGroup a inner join on GroupUser b where a.id=b.groupid and b.userid=%d",userid);
    MySQL mysql;
    vector<Group> groupVec;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    for(auto &group:groupVec)
    {
        //获取组内成员信息
        sprintf(sql,"select a.id,a.name,a.state,b.grouprole from User a inner join on GroupUser b where a.id=b.userid and b.id=%d",group.getId());
        MYSQL_RES* res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {  
                GroupUser groupUser;
                groupUser.setId(atoi(row[0]));
                groupUser.setName(row[1]);
                groupUser.setState(row[2]);
                groupUser.setRole(row[3]);
                group.getUsers().push_back(groupUser);
            }
            mysql_free_result(res);
        }
    }
    return groupVec;
}

//user根据groupid查询所在群内处自己外的所有用户id（用于群聊：每一个转发即可）
vector<int> GroupModel::queryGroupUsers(int userid,int groupid)
{
    char sql[1024];
    sprintf(sql,"select userid from GroupUser where groupid=%d and userid!=%d",groupid,userid);
    MySQL mysql;
    vector<int> idVec;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {  
                idVec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return idVec;
}