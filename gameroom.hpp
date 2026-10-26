#ifndef __M_GAMEROOM_H__
#define __M_GAMEROOM_H__

#include "db.hpp"
#include "util.hpp"
#include "online.hpp"
#include <vector>
#include <string>

#define BORAD_ROW 15
#define BORAD_COL 15
#define CHESS_WHITE 1
#define CHESS_BLACK 2

typedef enum
{
    GAME_START,
    GAME_OVER
} room_statu;

class gameroom
{
private:
    uint64_t _room_id;
    room_statu _statu;
    int _player_num;
    uint64_t _white_id;
    uint64_t _black_id;
    user_table *_player_table;
    online_manager *_online_user;
    std::vector<std::vector<int>> _board;

private:
    bool five(int row, int col, int row_offset, int col_offset, int color)
    {
        //(row, col)为下棋位置，row_offset,col_offset为方向偏移量，color为棋子颜色
        int count = 1;
        int search_row = row + row_offset;
        int search_col = col + col_offset;
        while (search_row >= 0 && search_row < BORAD_ROW &&
               search_col >= 0 && search_col < BORAD_COL &&
               _board[search_row][search_col] == color)
        {
            count++;
            search_row += row_offset;
            search_col += col_offset;
        }
        search_row = row - row_offset;
        search_col = col - col_offset;
        while (search_row >= 0 && search_row < BORAD_ROW &&
               search_col >= 0 && search_col < BORAD_COL &&
               _board[search_row][search_col] == color)
        {
            count++;
            search_row -= row_offset;
            search_col -= col_offset;
        }
        return (count >= 5); // 返回是否连续5个棋子
    }

    uint64_t check_win(int row, int col, int color)
    {
        // 在当前棋子四个方向上检查是否有连续5个棋子
        if 
        (
            /*横向判断*/ five(row, col, 0, 1, color) ||
            /*纵向判断*/ five(row, col, 1, 0, color) ||
            /*斜向判断*/ 
            five(row, col, -1, 1, color) || /*右上斜线*/
            five(row, col, -1, -1, color) /*左上斜线*/
        )
        {
            return color == CHESS_WHITE ? _white_id : _black_id; // 返回胜利玩家ID
        }
        return 0; // 返回0表示没有胜利玩家
    }

public:
    gameroom(uint64_t room_id, user_table *player_table, online_manager *online_user)
        : _room_id(room_id), _statu(GAME_START), _player_num(0),
          _player_table(player_table), _online_user(online_user),
          _board(BORAD_ROW, std::vector<int>(BORAD_COL, 0))
    {
        DBG_LOG("%lu 房间创建成功", _room_id);
    }
    ~gameroom()
    {
        DBG_LOG("%lu 房间销毁成功", _room_id);
    }
    uint64_t get_room_id() { return _room_id; }
    room_statu get_statu() { return _statu; }
    int player_num() { return _player_num; }
    void add_white_user(uint64_t uid)
    {
        _white_id = uid;
        _player_num++;
    }
    void add_black_user(uint64_t uid)
    {
        _black_id = uid;
        _player_num++;
    }
    uint64_t get_white_id() { return _white_id; }
    uint64_t get_black_id() { return _black_id; }

    /*处理下棋行为*/
    Json::Value handle_chess(Json::Value &req)
    {
        Json::Value json_resp = req;
        // 审查玩家是否都在线，若任意一个玩家不在，则判另一位玩家胜利
        uint64_t cur_id = req["uid"].asUInt64();
        if (_online_user->is_in_gameroom(_white_id) == false)
        {
            json_resp["result"] = "true";
            json_resp["reason"] = "对方掉线，你获得胜利！";
            json_resp["winner"] = _black_id;

            return json_resp;
        }
        if (_online_user->is_in_gameroom(_black_id) == false)
        {
            json_resp["result"] = "true";
            json_resp["reason"] = "对方掉线，你获得胜利！";
            json_resp["winner"] = _white_id;

            return json_resp;
        }
        // 审查请求中的棋子位置是否符合规则
        if (_board[req["row"].asInt()][req["col"].asInt()] != 0)
        {
            json_resp["result"] = "false";
            json_resp["reason"] = "当前位置已有棋子！";

            return json_resp;
        }

        int cur_color = cur_id == _white_id ? CHESS_WHITE : CHESS_BLACK; // 获取当前玩家棋子颜色
        _board[req["row"].asInt()][req["col"].asInt()] = cur_color;      // 设置棋子位置
        // 4.判断是否有玩家胜利
        uint64_t winner_id = check_win(req["row"].asInt(), req["col"].asInt(), cur_color);
        if (winner_id != 0)
        {
            json_resp["reason"] = "五星连珠！获得胜利";
        }
        json_resp["optype"] = "put_chess";
        json_resp["result"] = "true";
        json_resp["winner"] = winner_id;
        return json_resp;
    }

    /*处理聊天行为*/
    Json::Value handle_chat(Json::Value &req)
    {
        Json::Value json_resp = req;
        // 检查敏感词
        std::string msg = req["message"].asString();
        size_t pos = msg.find("菜"); // 检查敏感词
        if (pos != std::string::npos)
        {
            json_resp["result"] = "false";
            json_resp["reason"] = "聊天内容包含敏感词";
            return json_resp;
        }
        // 广播聊天信息
        json_resp["result"] = "true";
        return json_resp;
    }

    /*处理用户退出行为*/
    void handle_exit(uint64_t uid)
    {
        // 需要判断玩家是否是中途退出
        Json::Value json_resp;
        if (_statu == GAME_START)
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = "true";
            json_resp["reason"] = "对方已退出游戏，你获得胜利！";
            json_resp["room_id"] = _room_id;
            json_resp["uid"] = uid;
            json_resp["row"] = -1;
            json_resp["col"] = -1;
            json_resp["winner"] = uid == _white_id ? _black_id : _white_id;
            broadcast(json_resp);
            _statu = GAME_OVER;
        }
        _player_num--;
        return;
    }

    /*处理用户请求行为,在内部区分请求类型，并调用相应处理函数并得到响应，然后进行广播*/
    void handle_request(Json::Value &req)
    {
        // 校验请求中的房间号是否匹配
        Json::Value json_resp;
        uint64_t room_id = req["room_id"].asUInt64();
        if (room_id != _room_id)
        {
            json_resp["optype"] = req["optype"].asString();
            json_resp["result"] = "false";
            json_resp["reason"] = "房间号不正确";
            broadcast(json_resp);
            return; // 返回错误响应并结束函数
        }
        // 根据请求类型调用相应处理函数
        if (req["optype"].asString() == "put_chess")
        {
            json_resp = handle_chess(req);
            if (json_resp["winner"].asUInt64() != 0)
            {
                // 更新数据库玩家数据
                uint64_t winner_id = json_resp["winner"].asUInt64();
                uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
                _player_table->win(winner_id);
                _player_table->lose(loser_id);

                _statu = GAME_OVER; // 设置房间状态为游戏结束
            }
        }
        else if (req["optype"].asString() == "chat")
        {
            json_resp = handle_chat(req);
        }
        else
        {
            json_resp["optype"] = req["optype"].asString();
            json_resp["result"] = "false";
            json_resp["reason"] = "未知请求类型";
        }

        broadcast(json_resp);
        return; // 返回响应信息并结束函数
    }

    /*将指定信息广播给所有用户*/
    void broadcast(Json::Value &rsp)
    {
        // 对响应信息进行序列化，将Json::Value对象转换为json格式字符串
        std::string body;
        Json_Util::serialize(rsp, body);
        // 获取房间的所有用户通信连接,并发送消息
        websocket_server::connection_ptr white_conn = _online_user->get_conn_gameroom(_white_id);
        if (white_conn.get() != nullptr)
        {
            white_conn->send(body); // 发送消息给白方用户
        }
        websocket_server::connection_ptr black_conn = _online_user->get_conn_gameroom(_black_id);
        if (black_conn.get() != nullptr)
        {
            black_conn->send(body); // 发送消息给黑方用户
        }

        return;
    }
};

using room_ptr = std::shared_ptr<gameroom>; // 定义房间指针类型
class gameroom_manager
{
private:
    uint64_t _next_room_id;
    user_table *_player_table;
    online_manager *_online_user;
    std::unordered_map<uint64_t, room_ptr> _rooms;
    std::unordered_map<uint64_t, uint64_t> _users_rooms;
    std::mutex _mutex;

public:
    gameroom_manager(user_table *player_table, online_manager *online_user)
        : _next_room_id(1), _player_table(player_table), _online_user(online_user)
    {
        DBG_LOG("房间管理器创建成功"); // 初始化日志记录器
    }
    ~gameroom_manager()
    {
        DBG_LOG("房间管理即将销毁");
    }

    // 为两个用户创建房间，返回房间的管理指针
    room_ptr create_room(uint64_t uid1, uint64_t uid2)
    {
        // 两个用户在匹配大厅中匹配，若匹配成功则创建房间
        // 校验两个用户是否在匹配大厅，只有在线才需要创建房间
        if (_online_user->is_in_gamehall(uid1) == false || _online_user->is_in_gamehall(uid2) == false)
        {
            DBG_LOG("用户不在匹配大厅，无法创建房间");
            return room_ptr();
        }

        // 创建房间，将用户添加到房间中
        std::unique_lock<std::mutex> lock(_mutex);
        room_ptr rp(new gameroom(_next_room_id, _player_table, _online_user));
        rp->add_white_user(uid1);
        rp->add_black_user(uid2);

        // 将房间管理起来
        _rooms.insert(std::make_pair(_next_room_id, rp));
        _users_rooms.insert(std::make_pair(uid1, _next_room_id));
        _users_rooms.insert(std::make_pair(uid2, _next_room_id));
        _next_room_id++;

        return rp;
    }

    // 获取指定房间ID的房间信息
    room_ptr get_room_by_roomid(uint64_t room_id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _rooms.find(room_id);
        if (it == _rooms.end())
        {
            DBG_LOG("房间不存在");
            return room_ptr();
        }
        return it->second; // 返回房间信息
    }

    // 通过用户ID获取其所在的房间信息
    room_ptr get_room_by_userid(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto uit = _users_rooms.find(uid);
        if (uit == _users_rooms.end())
        {
            DBG_LOG("用户不在任意一个房间中");
            return room_ptr();
        }

        uint64_t rid = uit->second;
        auto rit = _rooms.find(rid);
        if (rit == _rooms.end())
        {
            DBG_LOG("房间不存在");
            return room_ptr();
        }
        return rit->second; // 返回房间信息
    }

    // 通过房间ID销毁房间
    void destroy_room(uint64_t room_id)
    {
        // 房间信息是通过shared_ptr管理的，因此只需要将shared_ptr从_rooms中删除即可
        room_ptr rp = get_room_by_roomid(room_id);
        if (rp.get() == nullptr)
        {
            return;
        }

        // 通过房间信息，移除房间中的用户
        uint64_t uid1 = rp->get_white_id();
        uint64_t uid2 = rp->get_black_id();
        _users_rooms.erase(uid1);
        _users_rooms.erase(uid2);

        _rooms.erase(room_id);
    }

    // 删除指定房间中的用户，若房间内没有用户，则删除房间，在用户断开连接时被调用
    void destroy_room_users(uint64_t uid)
    {
        room_ptr rp = get_room_by_userid(uid);
        if (rp.get() == nullptr)
        {
            return;
        }
        rp->handle_exit(uid);
        if (rp->player_num() == 0)
        {
            destroy_room(rp->get_room_id());
        }
    }
};
#endif
