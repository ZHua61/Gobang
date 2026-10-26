#ifndef __M_DB_H__
#define __M_DB_H__

#include "util.hpp"
#include <mutex>
#include <assert.h>

class user_table
{
private:
    MYSQL *_mysql;     // 操作句柄
    std::mutex _mutex; // 互斥锁

public:
    user_table(
        const std::string &host,
        const std::string &username,
        const std::string &password,
        const std::string &dbname,
        uint16_t port = 3306)
    {
        _mysql = MySQL_Util::mysql_create(host, username, password, dbname, port); // 获取数据库连接
        assert(_mysql != NULL);
    }
    ~user_table()
    {
        MySQL_Util::mysql_destroy(_mysql); \
        _mysql == NULL;
    }

    bool insert(Json::Value &user)// 新用户注册
    {
        #define INSERT_USER "INSERT users values(null, '%s', password('%s'),1000,0,0);"//定义一个格式化插入语句
        
        if(user["username"].isNull() || user["password"].isNull())
        {
            DBG_LOG("Please enter username or password!\n");
            return false;
        }
        char sql[4096] = {0};
        sprintf(sql, INSERT_USER, user["username"].asCString(), user["password"].asCString());
        
        bool ret = MySQL_Util::mysql_execute(_mysql, sql);
        if (!ret)
        {
            DBG_LOG("Insert user failed!\n");
            return false;
        }
        return true;
    }

    bool login(Json::Value &user)          // 用户登录验证，返回用户信息
    {
        #define LOGIN_USER "SELECT id, score, total_count,win_count FROM users WHERE username='%s' AND password=password('%s');"
        char sql[4096] = {0};
        sprintf(sql, LOGIN_USER, user["username"].asCString(), user["password"].asCString());
        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex);//加锁，防止多线程同时查询
            bool ret = MySQL_Util::mysql_execute(_mysql, sql);
            if (ret==false)
            {
                DBG_LOG("login user failed!\n");
                return false;
            }
            //仅只能查询到一个用户数据，否则返回false
            res=mysql_store_result(_mysql);
            if(res==NULL)
            {
                DBG_LOG("Login user information does not exist!\n");
                return false;
            }
        }
 
        int row_count = mysql_num_rows(res);
        
        if(row_count!=1)
        {
            DBG_LOG("The retrieved user information is not unique!\n");
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res); // 获取查询结果
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stol(row[2]);
        user["win_count"] = std::stol(row[3]);
        mysql_free_result(res);
        return true;
    }

    bool select_by_name(const std::string &name,Json::Value &user) // 根据用户名查询用户信息
    {
        #define SELECT_BY_NAME "SELECT id, score, total_count,win_count FROM users WHERE username='%s';"

        char sql[4096] = {0};
        sprintf(sql, SELECT_BY_NAME, name.c_str());
        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex);//加锁，防止多线程同时查询
            
            bool ret = MySQL_Util::mysql_execute(_mysql, sql);
            if (!ret)
            {
                DBG_LOG("No corresponding user was found based on the username!\n");
                return false;
            }

            res = mysql_store_result(_mysql);//获取查询结果
            if (res == NULL)
            {
                DBG_LOG("User information does not exist!\n");
                return false;
            }
        }

        int row_count = mysql_num_rows(res);
        if(row_count!=1)
        {
            DBG_LOG("The retrieved user information is not unique!\n");
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);//获取查询结果
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["username"] = name;
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stol(row[2]);
        user["win_count"] = std::stol(row[3]);
        mysql_free_result(res);
        return true;
    }

    bool select_by_id(uint64_t id,Json::Value &user)   // 根据用户ID查询用户信息
    {
        #define SELECT_BY_ID "SELECT username, score, total_count,win_count FROM users WHERE id='%lu';"

        char sql[4096] = {0};
        sprintf(sql, SELECT_BY_ID, id);
        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex);//加锁，防止多线程同时查询
            
            bool ret = MySQL_Util::mysql_execute(_mysql, sql);
            if (!ret)
            {
                DBG_LOG("No corresponding user was found based on the id!\n");
                return false;
            }

            res = mysql_store_result(_mysql);//获取查询结果
            if (res == NULL)
            {
                DBG_LOG("User information does not exist!\n");
                return false;
            }
        }

        int row_count = mysql_num_rows(res);
        if(row_count!=1)
        {
            DBG_LOG("The retrieved user information is not unique!\n");
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);//获取查询结果
        user["id"]=(Json::UInt64)id;
        user["username"] = row[0];
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stol(row[2]);
        user["win_count"] = std::stol(row[3]);
        mysql_free_result(res);
        return true;
    } 

    bool win(uint64_t id)      // 胜利增加段位分数，游戏场次增加，胜利场次增加
    {
        #define WIN "UPDATE users SET score=score+30,total_count=total_count+1,win_count=win_count+1 WHERE id='%lu';"
        
        char sql[4096] = {0};
        sprintf(sql, WIN, id);
        bool ret = MySQL_Util::mysql_execute(_mysql, sql);
        if (!ret)
        {
            DBG_LOG("Update win user info failed!\n");
            return false;
        }
        return true;
    }

    bool lose(int id)     // 失败减少段位分数，游戏场次增加,其他不变
    {
        #define LOSE "UPDATE users SET score=score-30,total_count=total_count+1 WHERE id='%d';"
        
        char sql[4096] = {0};
        sprintf(sql, LOSE, id);
        bool ret = MySQL_Util::mysql_execute(_mysql, sql);
        if (!ret)
        {
            DBG_LOG("Update lose user info failed!\n");
            return false;
        }
        return true;
    }
};

#endif