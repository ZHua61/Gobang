#ifndef _M_SERVER_H_
#define _M_SERVER_H_

#include "db.hpp"
#include "gameroom.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "session.hpp"
#include "util.hpp"

#define WWWROOT "./wwwroot/" // 静态资源根目录

class gobang_server
{
private:
    std::string _web_root; // 静态资源目录
    websocket_server _wssrv;
    user_table _ut;
    online_manager _om;
    gameroom_manager _rm;
    session_manager _sm;
    Matcher _mm;

private:
    void file_handler(websocket_server::connection_ptr &conn)
    {
        // 静态资源请求处理
        // 1.获取到请求URI-资源路径，了解客户端请求的页面文件名称
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        // 2.组合出文件的实际路径：相对根目录+URI
        std::string realpath = _web_root + uri;
        // 3.如果请求的是个目录，增加后缀：login.html
        if (realpath.back() == '/')
        {
            realpath += "login.html";
        }
        // 4.读取文件内容
        Json::Value resp_json;
        std::string body;
        bool ret = File_Util::read(realpath, body);
        if (ret == false)
        {
            body += "<html>";
            body += "<head>";
            body += "<meta charset='UTF-8'/>";
            body += "</head>";
            body += "<body>";
            body += "<h1>404 Not Found</h1>";
            body += "</body>";
            conn->set_status(websocketpp::http::status_code::not_found);
            conn->set_body(body);
            return;
        }
        // 5.设置响应正文
        conn->set_body(body);
        conn->set_status(websocketpp::http::status_code::ok);
    }

    void http_resp(bool result, websocketpp::http::status_code::value status, const std::string &reason, websocket_server::connection_ptr &conn)
    {
        Json::Value resp_json;
        resp_json["result"] = result;
        resp_json["reason"] = reason;
        std::string resp_body;
        Json_Util::serialize(resp_json, resp_body);
        conn->set_status(status);
        conn->set_body(resp_body);
        conn->append_header("Content-Type", "application/json");
        return;
    }

    void reg(websocket_server::connection_ptr &conn)
    {
        // 注册请求处理
        websocketpp::http::parser::request req = conn->get_request();
        // 1.获取到请求体正文
        std::string body = conn->get_request_body();
        // 2.进行反序列化
        Json::Value login_info; // 用于存储反序列化后的JSON数据
        bool ret = Json_Util::deserialize(body, login_info);
        if (ret == false)
        {
            return http_resp(false, websocketpp::http::status_code::bad_request, "请求的正文格式错误", conn);
        }
        // 3.进行数据库操作
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            return http_resp(false, websocketpp::http::status_code::bad_request, "请输入用户名或密码", conn);
        }
        ret = _ut.insert(login_info);
        if (ret == false)
        {
            return http_resp(false, websocketpp::http::status_code::bad_request, "用户名已经被占用", conn);
        }

        return http_resp(true, websocketpp::http::status_code::ok, "注册成功", conn);
    }

    void login(websocket_server::connection_ptr &conn)
    {
        // 登录请求处理
        // 1.获取请求正文，进行反序列化
        websocketpp::http::parser::request req = conn->get_request();
        std::string body = conn->get_request_body();
        Json::Value login_info;
        bool ret = Json_Util::deserialize(body, login_info);
        if (ret == false)
        {
            return http_resp(false, websocketpp::http::status_code::bad_request, "请求的正文格式错误", conn);
        }
        // 2.校验完整性，进行数据库的用户信息验证
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            return http_resp(false, websocketpp::http::status_code::bad_request, "请输入用户名或密码", conn);
        }
        ret = _ut.login(login_info);
        if (ret == false)
        {
            return http_resp(false, websocketpp::http::status_code::bad_request, "用户名或密码错误", conn);
        }
        // 3.返回响应
        // 验证成功，创建session
        uint64_t uid = login_info["id"].asUInt64();
        session_ptr ssp = _sm.create_session(uid, ss_status::LOGIN);
        if (ssp == nullptr)
        {
            return http_resp(false, websocketpp::http::status_code::bad_request, "创建session失败", conn);
        }
        _sm.set_session_expire_time(ssp->get_ssid(), SESSION_TIMEOUT); // 设置session过期时间
        // 4.设置响应头：Set-Cookie，将sessionid通过cookie返回给客户端
        std::string cookie = "SSID=" + std::to_string(ssp->get_ssid());
        conn->append_header("Set-Cookie", cookie);
        return http_resp(true, websocketpp::http::status_code::ok, "登录成功", conn);
    }

    bool get_cookie_value(std::string &cookie, const std::string &key, std::string &val)
    {
        // 以分号为间隔符分割cookie，得到各个cookie值
        std::string sep = ";";
        std::vector<std::string> cookies;
        String_Util::split(cookie, sep, cookies);
        for (auto str : cookies)
        {
            std::vector<std::string> strs;
            String_Util::split(str, "=", strs);
            if (strs.size() != 2)
            {
                continue;
            }
            if (strs[0] == key)
            {
                val = strs[1];
                return true;
            }
        }
        return false; // 如果没有找到对应的key，返回false
    }

    void info(websocket_server::connection_ptr &conn)
    {
        // 用户信息获取的请求处理
        // 1.通过请求头中的cookie获取SSID
        std::string cookie = conn->get_request_header("Cookie");
        if (cookie.empty())
        {
            // 如果没有cookie，返回错误信息：请重新登录
            return http_resp(false, websocketpp::http::status_code::bad_request, "请重新登录", conn);
        }
        std::string ssid_str;
        bool ret = get_cookie_value(cookie, "SSID", ssid_str);
        if (ret == false)
        {
            // 如果cookie中没有SSID，返回错误信息：请重新登录
            return http_resp(false, websocketpp::http::status_code::bad_request, "请重新登录", conn);
        }
        // 2.在session_manager中通过SSID获取session对象
        session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid_str));
        if (ssp == nullptr)
        {
            // 如果没有session对象，、则认为登录过期，需要重新登录
            return http_resp(false, websocketpp::http::status_code::bad_request, "登录过期，请重新登录", conn);
        }
        // 3.从数据库中获取用户信息，序列化后返回给客户端
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        ret = _ut.select_by_id(uid, user_info);
        if (ret == false)
        {
            return http_resp(false, websocketpp::http::status_code::bad_request, "用户信息获取失败,请重新登录", conn);
        }
        std::string resp_body;
        Json_Util::serialize(user_info, resp_body);
        conn->set_status(websocketpp::http::status_code::ok);
        conn->set_body(resp_body);
        conn->append_header("Content-Type", "application/json");
        // 4.刷新session的过期时间
        _sm.set_session_expire_time(ssp->get_ssid(), SESSION_TIMEOUT);
        return;
    }

    void http_callback(websocketpp::connection_hdl hdl)
    {
        websocket_server::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string uri = req.get_uri();
        if (method == "POST" && uri == "/reg")
        {
            return reg(conn);
        }
        else if (method == "POST" && uri == "/login")
        {
            return login(conn);
        }
        else if (method == "GET" && uri == "/info")
        {
            return info(conn);
        }
        else
        {
            return file_handler(conn);
        }
    }

    void ws_resp(websocket_server::connection_ptr conn, Json::Value &resp)
    {
        std::string resp_body;
        Json_Util::serialize(resp, resp_body);
        conn->send(resp_body);
    }

    session_ptr get_session_by_cookie(websocket_server::connection_ptr conn)
    {
        Json::Value err_resp;
        // 1.通过请求头中的cookie获取SSID
        std::string cookie = conn->get_request_header("Cookie");
        if (cookie.empty())
        {
            // 如果没有cookie，返回错误信息：请重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到Cookie，请重新登录";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }
        std::string ssid_str;
        bool ret = get_cookie_value(cookie, "SSID", ssid_str);
        if (ret == false)
        {
            // 如果cookie中没有SSID，返回错误信息：请重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到SSID，请重新登录";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }
        // 2.在session_manager中通过SSID获取session对象
        session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid_str));
        if (ssp == nullptr)
        {
            // 如果没有session对象，则认为登录过期，需要重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "登录过期，请重新登录";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }

        return ssp;
    }

    void wsopen_gamehall(websocket_server::connection_ptr conn)
    {
        // 登录验证-判断当前客户端是否成功登录
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        // 判断当前用户是否重复登录
        Json::Value err_resp; // 错误响应信息
        if (_om.is_in_gamehall(ssp->get_user()) || _om.is_in_gameroom(ssp->get_user()))
        {
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "当前用户重复登录";
            err_resp["result"] = false;
            return ws_resp(conn, err_resp);
        }
        // 将当前客户端及连接加入到游戏大厅
        _om.enter_gamehall(ssp->get_user(), conn);
        // 给客户端返回响应游戏大厅连接成功
        Json::Value resp;
        resp["optype"] = "hall_ready";
        resp["result"] = true;
        ws_resp(conn, resp);
        // 设置session为永久存在
        _sm.set_session_expire_time(ssp->get_ssid(), SESSION_FOREVER);
    }

    void wsopen_gameroom(websocket_server::connection_ptr conn)
    {
        //获取客户端session
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        //当前用户是否在其他游戏房间或者未退出游戏大厅
        Json::Value err_resp; 
        if (_om.is_in_gamehall(ssp->get_user()) || _om.is_in_gameroom(ssp->get_user()))
        {
            err_resp["optype"] = "room_ready";
            err_resp["reason"] = "当前用户重复登录";
            err_resp["result"] = false;
            return ws_resp(conn, err_resp);
        }
        //判断用户是否创建好了房间
        room_ptr rp=_rm.get_room_by_userid(ssp->get_user());
        if (rp.get() == nullptr)
        {
            err_resp["optype"] = "room_ready";
            err_resp["reason"] = "未找到玩家的游戏房间";
            err_resp["result"] = false;
            return ws_resp(conn, err_resp);
        }
        //将当前用户添加到在线用户管理的游戏房间中
        _om.enter_gameroom(ssp->get_user(), conn);
        //将session设置为永久存在
        _sm.set_session_expire_time(ssp->get_ssid(), SESSION_FOREVER);
        // 给客户端返回响应游戏房间连接成功
        Json::Value resp;
        resp["optype"] = "room_ready";
        resp["result"] = true;
        resp["room_id"]=rp->get_room_id();
        resp["uid"]=ssp->get_user();
        resp["white_id"]=rp->get_white_id();
        resp["black_id"]=rp->get_black_id();
        return ws_resp(conn, resp);
    }

    void wsopen_callback(websocketpp::connection_hdl hdl)
    {
        // websocket长链接建立成功后的处理函数
        websocket_server::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            // 游戏大厅长链接
            return wsopen_gamehall(conn); // 进入游戏大厅
        }
        else if (uri == "/room")
        {
            // 游戏房间长链接
            return wsopen_gameroom(conn); // 进入游戏房间
        }
    }

    void wsclose_gamehall(websocket_server::connection_ptr conn)
    {
        // 登录验证-判断当前客户端是否成功登录
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        // 把玩家从游戏大厅中移除
        _om.exit_gamehall(ssp->get_user());
        // 将session恢复生命周期管理，设置定时销毁
        _sm.set_session_expire_time(ssp->get_ssid(), SESSION_TIMEOUT);
        // 给客户端返回响应游戏大厅连接成功
    }

    void wsclose_gameroom(websocket_server::connection_ptr conn)
    {
        //获取会话信息，识别客户端
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        //将玩家从在线用户管理的游戏房间中移除
        _om.exit_gameroom(ssp->get_user());
        //将session恢复生命周期管理，设置定时销毁
        _sm.set_session_expire_time(ssp->get_ssid(), SESSION_TIMEOUT);
        //当房间在所有玩家退出后销毁房间
        _rm.destroy_room_users(ssp->get_user());
    }

    void wsclose_callback(websocketpp::connection_hdl hdl)
    {
        // websocket链接断开前的处理
        websocket_server::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            // 游戏大厅长链接
            return wsclose_gamehall(conn); // 进入游戏大厅
        }
        else if (uri == "/room")
        {
            // 游戏房间长链接
            return wsclose_gameroom(conn); // 进入游戏房间
        }
    }

    void wsmessage_gamehall(websocket_server::connection_ptr conn, websocket_server::message_ptr msg)
    {
        Json::Value resp_json;
        std::string resp_body;
        // 1.身份验证
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        // 2.获取请求信息
        std::string msg_body = msg->get_payload();
        Json::Value req_json;
        bool ret = Json_Util::deserialize(msg_body, req_json);
        if (ret == false)
        {
            resp_json["optype"] = "false";
            resp_json["reason"] = "请求信息解析失败";
            return ws_resp(conn, resp_json);
        }
        // 3.处理请求信息
        if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_start")
        {
            // 开始匹配：将用户添到匹配队列
            _mm.add(ssp->get_user());
            resp_json["optype"] = "match_start";
            resp_json["result"] = "true";
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_stop")
        {
            // 停止匹配：将用户从匹配队列中移除
            _mm.del(ssp->get_user());
            resp_json["optype"] = "match_stop";
            resp_json["result"] = "true";
            return ws_resp(conn, resp_json);
        }
        resp_json["optype"] = "unknow";
        resp_json["result"] = "false";
        return ws_resp(conn, resp_json);
    }

    void wsmessage_gameroom(websocket_server::connection_ptr conn, websocket_server::message_ptr msg)
    {
        // 获取session，身份验证
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        // 获取客户端房间信息
        room_ptr rp = _rm.get_room_by_userid(ssp->get_user());
        Json::Value err_resp; 
        if (rp.get() == nullptr)
        {
            err_resp["optype"] = "false";
            err_resp["reason"] = "未找到玩家的游戏房间";
            err_resp["result"] = false;
            return ws_resp(conn, err_resp);
        }
        // 对消息进行反序列化
        Json::Value req_json;
        std::string req_body = msg->get_payload();
        bool ret = Json_Util::deserialize(req_body, req_json);
        if(ret == false)
        {
            err_resp["optype"] = "false";
            err_resp["reason"] = "请求信息解析失败";
            err_resp["result"] = false;
            return ws_resp(conn, err_resp);
        }
        // 通过房间模块进行消息请求的处理
        return rp->handle_request(req_json);
    }

    void wsmessage_callback(websocketpp::connection_hdl hdl, websocket_server::message_ptr msg)
    {
        // websocket连接通信处理
        websocket_server::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            // 游戏大厅长链接
            return wsmessage_gamehall(conn, msg);
        }
        else if (uri == "/room")
        {
            // 游戏房间长链接
            return wsmessage_gameroom(conn, msg);
        }
    }

public:
    gobang_server(const std::string &host,
                  const std::string &username,
                  const std::string &password,
                  const std::string &dbname,
                  uint16_t port = 3306,
                  const std::string &wwwroot = WWWROOT) : _web_root(wwwroot),
                                                          _ut(host, username, password, dbname, port),
                                                          _rm(&_ut, &_om),
                                                          _sm(&_wssrv),
                                                          _mm(&_rm, &_ut, &_om)
    {
        // 设置日志等级
        _wssrv.set_access_channels(websocketpp::log::alevel::none);
        // 初始化asio调度器
        _wssrv.init_asio();
        _wssrv.set_reuse_addr(true);
        _wssrv.set_http_handler(std::bind(&gobang_server::http_callback, this, std::placeholders::_1));
        _wssrv.set_open_handler(std::bind(&gobang_server::wsopen_callback, this, std::placeholders::_1));
        _wssrv.set_close_handler(std::bind(&gobang_server::wsclose_callback, this, std::placeholders::_1));
        _wssrv.set_message_handler(std::bind(&gobang_server::wsmessage_callback, this, std::placeholders::_1, std::placeholders::_2));
    }
    void start(int port) // 启动服务器
    {
        _wssrv.listen(port);
        _wssrv.start_accept();
        _wssrv.run(); // 运行服务器
    }
};

#endif
