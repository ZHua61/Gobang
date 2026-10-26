#ifndef __M_UTIL_H__
#define __M_UTIL_H__

#include "logger.hpp"
#include <string>
#include <iostream>
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
#include <memory>
#include <sstream>
#include <vector>
#include <fstream>

class MySQL_Util//封装实现数据库的连接&初始化，句柄销毁与语句执行
{
public:
    static MYSQL *mysql_create(
        const std::string &host,
        const std::string &username,
        const std::string &password,
        const std::string &dbname,
        uint16_t port = 3306)// 创建句柄并连接数据库
    {
        // 初始化Mysql句柄
        MYSQL *mysql = mysql_init(NULL); 
        if (mysql == NULL)
        {
            ERROR_LOG("mysql_init failed!");
            return NULL;
        }
        // 连接数据库
        if (mysql_real_connect(mysql, host.c_str(), username.c_str(), password.c_str(), dbname.c_str(), port, NULL, 0) == NULL)
        {
            ERROR_LOG("mysql_real_connect failed:%s", mysql_error(mysql));
            mysql_close(mysql);
            return NULL;
        }
        // 设置客户端字符集
        if (mysql_set_character_set(mysql, "utf8") != 0)
        {
            ERROR_LOG("mysql_set_character_set failed:%s", mysql_error(mysql));
            mysql_close(mysql);
            return NULL;
        }
        // 已经在mysql_real_connect中选择了数据库
        return mysql;
    }

    static bool mysql_execute(MYSQL *mysql, const std::string &sql)// 执行sql语句
    {
        int ret = mysql_query(mysql, sql.c_str());
        
        if (ret != 0)
        {
            ERROR_LOG("sql:%s\n", sql.c_str());
            ERROR_LOG("mysql_query failed:%s", mysql_error(mysql));
            
            return false;
        }
        return true;
    }

    static void mysql_destroy(MYSQL *mysql)// 释放句柄
    {
        if (mysql != NULL)
        {
            mysql_close(mysql);
            return; 
        }
        return;
    }
};

class Json_Util//封装实现json序列化和反序列化
{
    public:
        static bool serialize(const Json::Value &js, std::string &str)// 将Json::Value对象序列化为JSON字符串
        {

            // 实例化一个Json::StreamWriterBuilder对象
            Json::StreamWriterBuilder swb;
            swb["emitUTF8"] = true;
            swb["indentation"] = "  ";  
            // 通过swb对象创建一个Json::StreamWriter对象，使用智能指针管理
            std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
            // 使用sw对象将root对象中的数据序列化
            std::stringstream ss;
            int ret =sw->write(js, &ss);
            if(ret !=0)
            {
                ERROR_LOG("Json serializa failed!"); // 如果序列化失败，输出错误信息
                return false;
            }

            str= ss.str(); // 序列化后的JSON字符串

            return true; // 返回true
        }

        static bool deserialize(const std::string &str, Json::Value &js)// 将JSON字符串反序列化为Json::Value对象
        {
            // 实例化一个Json::CharReaderBuilder对象
            Json::CharReaderBuilder crb;
            // 使用charbReaderBuilder类创建Json::CharReader对象,使用智能指针管理
            std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
            // 定义一个Json::Value对象存储解析后的数据
            std::string error_msg;
            // 使用Json::CharReader对象进行反序列化
            bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &js, &error_msg);
            if (!ret)
            {
                ERROR_LOG("Json deserialize failed:%s", error_msg.c_str()); // 如果反序列化失败，输出错误信息
            
                return false;
            }
            return true; 
        }
};

class String_Util//封装实现字符串分割处理
{
    public:
        static int split(
            const std::string &src,/*源字符串*/
            const std::string &sep,/*分隔符*/
            std::vector<std::string> &res)
        {
            size_t pos, idx = 0;
            while (idx < src.size())
            {
                pos = src.find(sep, idx);
                if (pos == std::string::npos) // 如果src中没有sep
                {
                    res.push_back(src.substr(idx));
                    break;
                }
                if (pos == idx) // 如果src中sep连续出现
                {
                    idx+=sep.size();
                    continue; // 继续下一次查找
                }

                res.push_back(src.substr(idx, pos - idx));
                idx = pos + sep.size();
            }
            return res.size(); // 返回分割后的字符串数量
        }
};

class File_Util//封装实现文件读写处理(对于HTML文件数据进行读写)
{
    public:
        static bool read(const std::string &filename, std::string &body)
        {
            std::ifstream ifs(filename,std::ios::binary);
            if (!ifs.is_open())
            {
                ERROR_LOG("file open failed:%s", filename.c_str()); 
                return false;
            }

            size_t fsize=0;
            ifs.seekg(0, std::ios::end);// 移动文件指针到文件末尾
            fsize=ifs.tellg();// 获取文件大小
            ifs.seekg(0, std::ios::beg);// 移动文件指针到文件开头

            body.resize(fsize);// 为body分配足够的空间
            ifs.read((char*)&body[0],fsize);
            if(ifs.good()==false)
            {
                ERROR_LOG("file read failed:%s", filename.c_str()); 
                ifs.close();
                return false;
            }
            
            ifs.close();
            return true;
        }

};

#endif
