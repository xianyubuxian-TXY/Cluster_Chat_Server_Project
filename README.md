# 项目简介
通过muduo网络库实现的集群聊天服务器（C++）

# 项目依赖
Linux、Json、boost、muduo、cmake、mysql、redis、nginx

mysql数据库结构：
create database chat;
use chat;

create table User(
	id int primary key auto_increment comment "用户id",
	name varchar(50) not null unique comment "用户名",
	password varchar(50) not null comment "用户名",
	state enum('online','offline') default 'offline' comment "当前登录状态"
);

create table Friend(
	userid int not null comment "用户id",
	friendid int not null comment "好友id",
	primary key (userid,friendid)
);

create table AllGroup(
	id int primary key auto_increment comment "组id",
	groupname varchar(50) not null comment "组名称",
	groupdesc varchar(200) default ""
);

create table GroupUser(
	groupid int primary key comment "组id",
	userid int not null comment "组员id",
	grouprole enum('creator','normal') default 'normal' comment "组内角色"
);

create table OfflineMessage(
	userid int primary key comment "用户id",
	message varchar(500) not null comment "离线消息（存储Json字符串）"
);

