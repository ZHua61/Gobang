#ifndef _M_ONLINE_H
#define _M_ONLINE_H

#include "util.hpp"
#include <unordered_map>
#include <mutex>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> websocket_server;

class online_manager
{
private:
    std::mutex _mutex;
    // 建立游戏大厅与用户ID连接
    std::unordered_map<uint64_t, websocket_server::connection_ptr> _hall_users;
    // 建立游戏房间与用户ID连接
    std::unordered_map<uint64_t, websocket_server::connection_ptr> _room_users;

public:
    // websocket连接建立时，加入对用户进入游戏大厅或游戏房间的管理
    void enter_gamehall(uint64_t uid, websocket_server::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_users.insert(std::make_pair(uid, conn));
    }

    void enter_gameroom(uint64_t uid, websocket_server::connection_ptr &conn) 
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_users.insert(std::make_pair(uid, conn));
    }

    // websocket连接断开时，用户退出游戏大厅或游戏房间，移除管理
    void exit_gamehall(uint64_t uid) 
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_users.erase(uid);
    }
    
    void exit_gameroom(uint64_t uid) 
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_users.erase(uid);
    }
    
    // 判断用户是否在游戏大厅或游戏房间中
    bool is_in_gamehall(uint64_t uid) 
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _hall_users.find(uid);
        if (it == _hall_users.end())
        {
            return false;
        }
        return true;
    }

    bool is_in_gameroom(uint64_t uid) 
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _room_users.find(uid);
        if (it == _room_users.end())
        {
            return false;
        }
        return true;
    }

    // 通过用户ID获取用户在游戏大厅或游戏房间中关联的socket连接
    websocket_server::connection_ptr get_conn_gamehall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _hall_users.find(uid);
        if (it == _hall_users.end())
        {
            return websocket_server::connection_ptr();// 返回空连接(一个智能指针对象)
        }
        return it->second;// 返回socket连接
    }

    websocket_server::connection_ptr get_conn_gameroom(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _room_users.find(uid);
        if (it == _room_users.end())
        {
            return websocket_server::connection_ptr();// 返回空连接(一个智能指针对象)
        }
        return it->second;// 返回socket连接
    }
};

#endif