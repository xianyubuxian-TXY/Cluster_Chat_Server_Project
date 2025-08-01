#include "json.hpp"
using json = nlohmann::json;

#include<iostream>
#include<vector>
#include<map>
#include<string>
using namespace std;

string func1(){
    json js;
    js["msg_type"]=2;
    js["from"]="zhang san";
    js["to"]="li si";
    js["msg"]="what are you doing?";
    string sendbuf=js.dump(); //将json转化为字符串
    return sendbuf;
}

void func2(){
    json js;
    js["arr"]={1,2,3,4,5};
    js["msg"]["li si"]="hello";
    js["msg"]["zhang san"]="hello";
    cout<<js.dump()<<endl;
}

void func3(){
    vector<int> arr={1,2,3,4,5};
    map<int,string> m;
    m.insert({1,"黄山"});
    m.insert({2,"华山"});
    m.insert({3,"泰山"});
    json js;
    js["vec"]=arr;
    js["path"]=m;
    cout<<js<<endl;
    cout<<js.dump()<<endl;

}

int main(){
    string recv=func1();
    json jsbuf=json::parse(recv);  //将字符串反序列化为json
    cout<<jsbuf["msg_type"]<<" "<<jsbuf["from"]<<" "<<jsbuf["to"]<<" "<<jsbuf["msg"]<<endl;
    return 0;
}