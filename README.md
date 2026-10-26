# Cpp-WebSocket_Project

> 基于 C++ 与 WebSocket 开发的五子棋游戏对战平台

本项目是一个 **C++11 + WebSocket** 的多用户网页五子棋对战平台：玩家通过浏览器访问服务器，完成 **注册 / 登录** 后进入游戏大厅，按段位发起 **自动匹配**；匹配成功后双方进入同一游戏房间，在 **15 × 15** 棋盘上轮流落子对战，并支持房间内 **实时聊天**。对局胜负会实时结算到 MySQL 中的玩家段位分数，形成「匹配 — 对局 — 升降分」的完整闭环。

## 功能特性

- 用户注册、登录（密码经 MySQL `PASSWORD()` 加密存储）
- Cookie + Session 身份识别与会话生命周期管理（临时会话 ⇄ 永久会话）
- 按段位分档（青铜 / 白银 / 黄金）的自动对战匹配
- 15 × 15 五子棋对局：落子、四方向五连判定、胜负结算
- 房间内实时聊天（含基础敏感词过滤）
- 中途断线自动判负，退出房间后自动销毁房间
- 客户端为纯静态页面，由服务端一并托管（HTML / CSS / JS + jQuery + WebSocket）

## 技术栈

| 层次 | 技术 |
| ---- | ---- |
| 开发系统 | Linux (Ubuntu) |
| 服务端语言 | C++11 |
| 网络框架 | websocketpp（asio，同时承载 HTTP 与 WebSocket，提供定时器） |
| 数据库 | MySQL（C API：`libmysqlclient`） |
| JSON | jsoncpp |
| 客户端 | HTML / CSS / JavaScript + jQuery + WebSocket |
| 构建 | makefile（`g++ -std=c++11`） |
| 并发 | C++ 标准线程 / 互斥锁 / 条件变量 |

## 项目结构

```text
.
├── gobang.cc                 # 程序入口：数据库配置、启动服务器（监听 9002）
├── server.hpp                # 服务器整合层：HTTP / WebSocket 路由与业务编排
├── session.hpp               # 会话管理：Cookie(SSID) 与登录状态
├── db.hpp                    # 数据管理：用户表注册 / 登录 / 查询 / 胜负结算
├── db.sql                    # 数据库与用户表建表脚本
├── online.hpp                # 在线用户管理：uid 与连接映射
├── gameroom.hpp              # 游戏房间管理：落子判定、广播、结算
├── matcher.hpp               # 玩家匹配：分档阻塞队列 + 配对线程
├── util.hpp                  # 工具模块：MySQL / JSON / 字符串 / 文件封装
├── logger.hpp                # 日志模块：带线程 / 时间 / 文件 / 行号
├── makefile                  # 构建脚本
├── wwwroot/                  # 前端静态资源
│   ├── register.html         # 注册页
│   ├── login.html            # 登录页
│   ├── game_hall.html        # 游戏大厅
│   ├── game_room.html        # 游戏房间
│   ├── css/                  # 样式
│   ├── js/jquery.min.js      # jQuery
│   └── image/background.jpeg # 背景图
├── 开发设计文档/              # 模块设计文档与项目总结
└── .vscode/                  # VS Code 智能提示配置
```

## 环境依赖

- Linux（Ubuntu 22.04 已验证）
- g++（支持 C++11）、make
- MySQL 8.0 及 `libmysqlclient-dev`
- jsoncpp（`libjsoncpp-dev`）
- websocketpp（header-only）+ Boost.Asio（`libboost-dev` / `libboost-system-dev`）
- pthread

Ubuntu 下可执行：

```bash
sudo apt update
sudo apt install -y g++ make libmysqlclient-dev libjsoncpp-dev \
    libwebsocketpp-dev libboost-dev libboost-system-dev
```

> websocketpp 为 header-only 库；若发行版未提供 `libwebsocketpp-dev`，将 websocketpp 头文件放到 `/usr/include/websocketpp` 即可。

## 数据库准备

```bash
mysql -uroot -p < db.sql
```

建库建表脚本会创建 `gobang` 数据库与 `users` 表。随后按本地环境修改 `gobang.cc` 顶部的数据库配置：

```cpp
#define HOST "127.0.0.1"   // MySQL 主机
#define USER "root"        // MySQL 用户名
#define PASSWORD "123456"  // MySQL 密码
#define DATABASE "gobang"  // 数据库名
#define PORT 3306          // MySQL 端口
```

## 编译与运行

```bash
make          # 生成可执行文件 gobang
./gobang      # 监听 9002 端口
```

浏览器访问：<http://127.0.0.1:9002/register.html>

> 服务端的静态资源根目录为相对路径 `./wwwroot/`，请在项目根目录下启动程序。
> 仓库不包含编译生成的 `gobang` 二进制文件（见 `.gitignore`），请自行编译。

## 设计文档

| 文档 | 内容 |
| ---- | ---- |
| [项目总结](开发设计文档/项目总结.markdown) | 项目简介、架构、核心流程与设计亮点 |
| [封装服务器模块设计](开发设计文档/封装服务器模块设计.markdown) | websocketpp 服务器搭建与 HTTP / WebSocket 分发 |
| [会话模块设计](开发设计文档/会话模块设计.markdown) | Cookie / Session 与超时管理 |
| [数据管理模块设计](开发设计文档/数据管理模块设计.markdown) | MySQL 封装与用户表操作 |
| [在线用户管理模块设计](开发设计文档/在线用户管理模块设计.markdown) | 大厅 / 房间在线连接管理 |
| [游戏房间管理模块](开发设计文档/游戏房间管理模块.markdown) | 房间、落子判定、广播与结算 |
| [玩家匹配模块设计](开发设计文档/玩家匹配模块设计.markdown) | 分档匹配队列与配对线程 |
| [工具模块设计](开发设计文档/工具模块设计.markdown) | MySQL / JSON / 字符串 / 文件工具 |
| [日志模块设计](开发设计文档/日志模块设计.markdown) | 日志宏与多线程日志 |
