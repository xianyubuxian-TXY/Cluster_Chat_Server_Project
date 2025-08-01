#include "usermodel.hpp"
#include "db.hpp"

#include<muduo/base/Logging.h>
#include<iostream>
#include<string>
using namespace std;
//根据user插入信息
bool UserModel::insert(User &user)
{
    //1.组装sql语句
    char sql[1024]={0};
    //主键id是插入db后自动生成，所以这里不用插入id
    sprintf(sql,"insert into User(name,password,state) values('%s','%s','%s')",
        user.getName().c_str(),user.getPwd().c_str(),user.getState().c_str());
    
    MySQL mysql;
    if(mysql.connect())  //检查连接
    {
        if(mysql.update(sql)) //操作数据是否成功
        {
            //获取插入成功后用户数据生成的主键id，并设置到user中
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

//根据user查询信息
User UserModel::query(int id)
{
    char sql[1024]={0};
    sprintf(sql,"select * from User where id=%d",id);
    MySQL mysql;
    if(mysql.connect()) //检查连接
    {
        MYSQL_RES* res=mysql.query(sql);  //查询数据是否存在
        if(res!=nullptr)  //获取结果成功
        {
            auto row=mysql_fetch_row(res);  //res结果中如果行数为0(空集),则mysql_fetch_row返回nullptr
            if(row!=nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPwd(row[2]);
                user.setState(row[3]);
                //释放res资源，否则会出现内存泄露
                mysql_free_result(res);
                return user;
            }
        }
    }
    //返回一个默认user，其id=-1
    return User();
}

//更新用户state字段
bool UserModel::updateState(User &user)
{
    char sql[1024]={0};
    sprintf(sql,"update User set state='%s' where id=%d",user.getState().c_str(),user.getId());
    MySQL mysql;
    //检查连接
    if(mysql.connect())
    {   
        //更新成功
        if(mysql.update(sql))
        {
            return true;
        }
    }
    return false;
}

//重置所有用户的状态为offline
void UserModel::resetState()
{
    char sql[1024]="update User set state='offline' where state='online'";
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.query(sql);
    }
}