#ifndef __M_MATCHER_H__
#define __M_MATCHER_H__

#include "util.hpp"
#include "gameroom.hpp"
#include "db.hpp"
#include "online.hpp"
#include <list>
#include <mutex>
#include <condition_variable>

template <class T>
class Match_Queue
{
private:
    /*使用链表而不直接使用队列是因为有中间删除的需要*/
    std::list<T> _list;
    std::mutex _mutex;
    /*使用条件变量是为了实现阻塞队列的功能*/
    std::condition_variable _cond;

public:
    int size()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.size();
    }
    bool empty()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.empty();
    }
    void wait()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock);
    }
    void push(const T &data) // 数据入队，唤醒线程
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.push_back(data);
        _cond.notify_all();
    }
    bool pop(T &data) // 数据出队，阻塞线程
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_list.empty())
        {
            return false;
        }
        data = _list.front();
        _list.pop_front();
        return true;
    }
    bool remove(T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.remove(data);
        return true;
    }
};

class Matcher
{
private:
    std::mutex _mutex;
    std::condition_variable _cond;
    Match_Queue<uint64_t> _queue_bronze;
    Match_Queue<uint64_t> _queue_silver;
    Match_Queue<uint64_t> _queue_gold;
    std::thread _thread_bronze;
    std::thread _thread_silver;
    std::thread _thread_gold;
    gameroom_manager *_rm;
    user_table *_ut;
    online_manager *_om;

private:
    void handle_match(Match_Queue<uint64_t> &mq)
    {
        while (true)
        {
            // 1.判断队列人数是否大于等于2,小于2则继续等待
            while (mq.size() < 2)
            {
                mq.wait();
            }
            // 2.从队列中取出两个玩家ID
            uint64_t uid1;
            bool ret = mq.pop(uid1);
            if (ret == false)
            {
                continue;
            }
            uint64_t uid2;
            ret = mq.pop(uid2);
            if (ret == false)
            {
                this->add(uid1); // 重新加入队列
                continue;
            }
            // 3.校验玩家是否在线，如果有玩家掉线，则把另一个玩家重新加入队列
            websocket_server::connection_ptr conn1 = _om->get_conn_gamehall(uid1);
            if(conn1.get()==nullptr)
            {
                this->add(uid2);
                continue;
            }
            websocket_server::connection_ptr conn2 = _om->get_conn_gamehall(uid2);
            if(conn2.get()==nullptr)
            {
                this->add(uid1);
                continue;
            }
            // 4.为两个玩家创建一个游戏房间
            room_ptr rp = _rm->create_room(uid1, uid2);
            if (rp == nullptr)
            {
                this->add(uid1);
                this->add(uid2);
                continue;
            }
            // 5.向两个玩家发送响应
            Json::Value resp;
            resp["optype"]="match_success";
            resp["result"]=true;
            std::string body;
            Json_Util::serialize(resp, body);
            conn1->send(body);
            conn2->send(body);
        }
    }
    void th_bronze_entry() { handle_match(_queue_bronze); }
    void th_silver_entry() { handle_match(_queue_silver); }
    void th_gold_entry() { handle_match(_queue_gold); }

public:
    Matcher(gameroom_manager *rm, user_table *ut, online_manager *om) : 
    _rm(rm), _ut(ut), _om(om),
    _thread_bronze(std::thread(&Matcher::th_bronze_entry, this)),
    _thread_silver(std::thread(&Matcher::th_silver_entry, this)),
    _thread_gold(std::thread(&Matcher::th_gold_entry, this))
    {
        DBG_LOG("匹配模块初始化完毕");
    }

    bool add(uint64_t uid)
    {
        // 根据玩家的段位分数，将玩家加入相应的队列
        // 1.根据用户ID获取玩家信息
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user);
        if (ret == false)
        {
            DBG_LOG("用户ID为 %lu 的玩家信息获取失败", uid);
            return false;
        }
        int score = user["score"].asInt();
        // 2.根据玩家段位分数将玩家加入相应的队列
        if (score < 2000)
        {
            _queue_bronze.push(uid);
        }
        else if (score >= 2000 && score < 3000)
        {
            _queue_silver.push(uid);
        }
        else
        {
            _queue_gold.push(uid);
        }
        return true;
    }
    bool del(uint64_t uid)
    {
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user);
        if (ret == false)
        {
            DBG_LOG("用户ID为 %lu 的玩家信息获取失败", uid);
            return false;
        }
        int score = user["score"].asInt();
        // 2.根据玩家段位分数将玩家加入相应的队列
        if (score < 2000)
        {
            _queue_bronze.remove(uid);
        }
        else if (score >= 2000 && score < 3000)
        {
            _queue_silver.remove(uid);
        }
        else
        {
            _queue_gold.remove(uid);
        }
        return true;
    }
};

#endif