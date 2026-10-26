#ifndef _M_SESSION_H_
#define _M_SESSION_H_

#include "logger.hpp"
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <unordered_map>

typedef websocketpp::server<websocketpp::config::asio> websocket_server;
typedef enum
{
    UNLOGIN,
    LOGIN
} ss_status;

class session
{
private:
    uint64_t _ssid;                  // session的唯一标识符
    uint64_t _uid;                   // session对应的用户id
    ss_status _status;               // 用户状态
    websocket_server::timer_ptr _tp; // session关联的定时器
public:
    session(uint64_t ssid) : _ssid(ssid), _status(UNLOGIN) { DBG_LOG("SESSION %p 被创建!", this); }
    ~session() { DBG_LOG("SESSION %p 被释放!", this); }

    uint64_t get_ssid() { return _ssid; }
    void set_statu(ss_status statu) { _status = statu; }
    void set_user(uint64_t uid) { _uid = uid; }
    uint64_t get_user() { return _uid; }
    bool is_login() { return (_status == LOGIN); }
    void set_timer(const websocket_server::timer_ptr &tp) { _tp = tp; }
    websocket_server::timer_ptr &get_timer() { return _tp; }
};

#define SESSION_TIMEOUT 30000
#define SESSION_FOREVER -1
using session_ptr = std::shared_ptr<session>;
class session_manager
{
private:
    uint64_t _next_ssid; // ssid计数器
    std::mutex _mutex;
    std::unordered_map<uint64_t, session_ptr> _sessions; // session管理器
    websocket_server *_server;                           // 用于添加定时任务
public:
    session_manager(websocket_server *server) : _next_ssid(1), _server(server) { DBG_LOG("SESSION_MANAGER 初始化完毕!"); }
    ~session_manager() { DBG_LOG("SESSION_MANAGER 被销毁!"); }

    session_ptr create_session(uint64_t uid, ss_status statu)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        session_ptr ssp(new session(_next_ssid));
        ssp->set_statu(statu);
        ssp->set_user(uid); // 设置用户id
        _sessions.insert(std::make_pair(_next_ssid, ssp));
        _next_ssid++;

        return ssp;
    }

    session_ptr get_session_by_ssid(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _sessions.find(ssid);
        if (it == _sessions.end())
        {
            return session_ptr();
        }
        return it->second;
    }

    void append_session(const session_ptr &ssp)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _sessions.insert(std::make_pair(ssp->get_ssid(), ssp)); // 将session插入到unordered_map中
    }

    void remove_session(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _sessions.erase(ssid);
    }

    void set_session_expire_time(uint64_t ssid, int msec)
    {
        // 依赖于websocket_server的定时器功能来完成session生命周期的管理
        // 在HTTP通信时（登录，注册），session应具有生命周期，指定时间无通信后删除
        // 在客户端建立websocket连接时，session应是永久存在的
        // 登录后，创建session，session需要在指定时间无通信后删除
        // 但进入游戏大厅或者游戏房间时，这个session就应该永久存在
        // 等到用户退出游戏大厅或者游戏房间时，这个session应该设置为临时，在长时间无通信后删除

        session_ptr ssp = get_session_by_ssid(ssid); // 获取session
        if (ssp == nullptr)
            return;
        websocket_server::timer_ptr tp = ssp->get_timer();
        // 1.在session存在的情况下，设置为永久存在
        if (tp.get() == nullptr && msec == SESSION_FOREVER)
        {
            return;
        }
        else if (tp.get() == nullptr && msec != SESSION_FOREVER)
        {
            // 2.在session永久存在的情况下，设置指定时间后删除的定时任务
            websocket_server::timer_ptr tmp_tp = _server->set_timer(msec, std::bind(&session_manager::remove_session, this, ssid));
            ssp->set_timer(tmp_tp);
        }
        else if (tp.get() != nullptr && msec == SESSION_FOREVER)
        {
            // 3.在session设置了定时删除的情况下，将session设置为永久存在
            // 取消定时器会导致任务直接执行，因此需要将session重新插入到unordered_map中
            tp->cancel();// 取消定时任务可能并不会立即取消，因此需要将session重新插入到unordered_map中
            ssp->set_timer(websocket_server::timer_ptr()); // 清空定时器
            //_sessions.insert(std::make_pair(ssp->get_ssid(), ssp));（不能直接使用插入）
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));
        }
        else if (tp.get() != nullptr && msec != SESSION_FOREVER)
        {
            // 4.在session设置了定时删除的情况下，将session重置删除时间
            tp->cancel();
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));
            ssp->set_timer(websocket_server::timer_ptr());
            //重新设置session关联的定时器
            websocket_server::timer_ptr tmp_tp = _server->set_timer(msec, std::bind(&session_manager::remove_session, this, ssp->get_ssid()));
            ssp->set_timer(tmp_tp);
        }
    }
};

#endif