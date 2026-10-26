#include "gameroom.hpp"
#include "session.hpp"
#include "matcher.hpp"
#include "server.hpp"

#define HOST "127.0.0.1"
#define USER "root"
#define PASSWORD "123456"
#define DATABASE "gobang"
#define PORT 3306

void sql_test()
{
    MYSQL *mysql = MySQL_Util::mysql_create(HOST, USER, PASSWORD, DATABASE, PORT);

    const char *sql = "INSERT stu values(null,'小明',18,53,68,87);";

    bool ret = MySQL_Util::mysql_execute(mysql, sql);
    if (ret == false)
        return;

    MySQL_Util::mysql_destroy(mysql);
    return;
}

void sqltable_test()
{
    user_table ut(HOST, USER, PASSWORD, DATABASE, PORT);
    Json::Value user;
    //user["username"] = "小明";
    //user["password"] = "123123123";
    //ut.insert(user);
    //bool ret =ut.login(user);
    //bool ret = ut.select_by_name("小明",user);
    //bool ret = ut.select_by_id(1, user);
    //bool ret = ut.win(1);
    //bool ret = ut.lose(1);

    //user["username"] = "小明";
    //user["password"] = "123123123";
    return;
}

void json_test()
{
    Json::Value root;      // 创建一个空的Json::Value对象
    std::string body;       // 用于保存序列化后的JSON字符串
    root["姓名"] = "小明"; // 向root对象中添加一个键值对
    root["年龄"] = 18;
    root["成绩"].append(90);
    root["成绩"].append(85);
    root["成绩"].append(78);

    Json_Util::serialize(root, body); // 将root对象序列化为JSON字符串并保存到body变量中
    DBG_LOG("%s",body.c_str());

    Json::Value value; // 创建一个空的Json::Value对象用于解析JSON字符串
    Json_Util::deserialize(body, value); // 将body变量中的JSON字符串解析为json_body对象
    std::cout << "姓名: " << root["姓名"].asString() << std::endl;
    std::cout << "年龄: " << root["年龄"].asInt() << std::endl;
    std::cout << "成绩: ";
    for (Json::Value::iterator it = root["成绩"].begin(); it != root["成绩"].end(); ++it)
    {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    return ;
}
void str_test()
{
    std::string str =",...,123,,234,,,,,345";
    std::vector<std::string> arry;
    String_Util::split(str, ",", arry);

    for(auto s:arry)
    {
        DBG_LOG("%s",s.c_str());
    }
    return ;
}

void file_test()
{
    std::string filename="./makefile";
    std::string body;
    File_Util::read(filename,body);

    std::cout << body << std::endl;
    return ;
}

void online_test()
{
    online_manager om;
    websocket_server::connection_ptr conn;
    uint64_t uid =2;
    om.enter_gameroom(uid, conn);
    if(om.is_in_gameroom(uid))
    {
        DBG_LOG("进入游戏大厅");
    }
    else
    {
        DBG_LOG("未进入游戏大厅");
    }

    om.exit_gameroom(uid);
    if(om.is_in_gameroom(uid))
    {
        DBG_LOG("进入游戏大厅");
    }
    else
    {
        DBG_LOG("未进入游戏大厅");
    }
    return ;
}


int main()
{
    // user_table ut(HOST, USER, PASSWORD, DATABASE, PORT);
    // online_manager om;
    
    // gameroom_manager rm(&ut,&om);
    // room_ptr rp = rm.create_room(10,20);
    // Matcher mt(&rm,&ut,&om);

    gobang_server _server(HOST, USER, PASSWORD, DATABASE, PORT);
    _server.start(9002);
    return 0;
}