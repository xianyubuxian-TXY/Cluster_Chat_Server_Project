#include "friendmodel.hpp"
#include "db.hpp"
#include<muduo/base/Logging.h>
//获取好友列表：用户登录成功时给予用户
vector<User> FriendModel::query(int userId)
{
    char sql[1024];
    sprintf(sql,"select a.id,a.name,a.state from User a inner join Friend b on b.friendid=a.id where b.userid=%d",userId);
    MySQL mysql;
    vector<User> arr;  //好友列表
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        LOG_INFO<<"1";
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                LOG_INFO<<"2";
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                arr.push_back(user);
            }
            mysql_free_result(res); //释放资源
        }
    }
    return arr;
}

//添加好友
void FriendModel::insert(int userId,int friendId)
{
    char sql[1024]={0};
    sprintf(sql,"insert into Friend values(%d,%d)",userId,friendId);
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}