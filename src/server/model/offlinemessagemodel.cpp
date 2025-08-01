#include "offlinemessagemodel.hpp"
#include "db.hpp"
#include<iostream>
#include<muduo/base/Logging.h>

//插入离线消息
void OfflineMsgModel::insert(int id,string msg)
{
    char sql[1024];
    //拼接sql语句
    sprintf(sql,"insert into OfflineMessage values(%d,'%s')",id,msg.c_str());
    MySQL mysql;
    //检查连接
    if(mysql.connect())
    {
        //执行插入语句
        mysql.update(sql);
    }
}

//删除离线消息（用户登录后，将离线消息发送用户并要删除）
void OfflineMsgModel::remove(int id)
{
    char sql[1024];
    //拼接sql语句
    sprintf(sql,"delete from OfflineMessage where userid=%d",id);
    MySQL mysql;
    //检查连接
    if(mysql.connect())
    {  
        mysql.update(sql);
    }
}

//查询离线消息并返回
vector<string> OfflineMsgModel::query(int id)
{
    char sql[1024];
    sprintf(sql,"select * from OfflineMessage where userid=%d",id);
    MySQL mysql;
    vector<string> arr;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                arr.push_back(row[1]);
            }
            mysql_free_result(res);   
        }
    }
    return arr;
}