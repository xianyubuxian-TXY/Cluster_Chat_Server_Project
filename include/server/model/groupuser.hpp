#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

//群内的成员，具有职位(继承user的属性)
class GroupUser: public User
{
public:
    //设置职位
    void setRole(string role){this->_role=role;}
    //获取职位
    string getRole(){return this->_role;}
private:
    string _role;
};

#endif