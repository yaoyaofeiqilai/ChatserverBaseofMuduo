# ChatserverBaseofMuduo

## 项目简介

这是一个基于 **Muduo网络库** 开发的高性能集群聊天服务器,支持多用户并发通信、群组聊天、好友管理等功能。项目采用 **Nginx** 进行负载均衡,**Redis** 作为消息中间件实现跨服务器通信,**MySQL** 存储用户数据和消息记录。

### 技术栈

- **网络通信**: Muduo (基于Reactor模式的C++网络库)
- **负载均衡**: Nginx TCP反向代理
- **消息中间件**: Redis (Pub/Sub模式)
- **数据存储**: MySQL + Connection Pool
- **数据序列化**: JSON (nlohmann/json)
- **构建工具**: CMake
- **C++标准**: C++17

### 主要功能

✅ 用户注册/登录
✅ 一对一聊天
✅ 群组聊天 (创建群组、加入群组)
✅ 好友管理 (添加好友)
✅ 离线消息存储与转发
✅ 跨服务器通信 (Redis Pub/Sub)
✅ MySQL数据库连接池
✅ AES加密通信 (部分实现)

## 项目结构

```
ChatserverBaseofMuduo/
├── bin/                          # 可执行文件输出目录
├── build/                        # CMake构建目录
├── include/                      # 头文件目录
│   ├── client/                   # 客户端相关
│   │   └── client.hpp
│   ├── public/                   # 公共头文件
│   │   ├── keyguard.hpp          # 密钥管理
│   │   └── public.hpp
│   └── server/                   # 服务器相关
│       ├── chatserver.hpp        # 服务器主类
│       ├── chatservice.hpp       # 业务处理服务类
│       ├── dataoperate/          # 数据操作层
│       │   ├── group.hpp         # 群组模型
│       │   ├── groupoperate.hpp  # 群组操作
│       │   ├── offlinemsgoperate.hpp  # 离线消息
│       │   ├── user.hpp          # 用户模型
│       │   └── useroperate.hpp   # 用户操作
│       ├── db/                   # 数据库连接层
│       │   ├── connectionpool.hpp     # 连接池
│       │   └── mysqlconnection.hpp    # MySQL连接
│       └── redis/
│           └── redis.hpp         # Redis客户端封装
├── src/                          # 源文件目录
│   ├── client/                   # 客户端程序
│   │   ├── main.cpp              # 客户端主函数
│   │   ├── client.cpp            # 客户端逻辑
│   │   └── stresstest.cpp        # 压力测试
│   ├── public/
│   │   └── keyguard.cpp
│   └── server/                   # 服务器程序
│       ├── main.cpp              # 服务器主函数
│       ├── chatserver.cpp        # 服务器实现
│       ├── chatservice.cpp       # 业务服务实现
│       ├── dataoperate/          # 数据操作实现
│       ├── db/                   # 数据库连接实现
│       └── redis/                # Redis实现
├── thridparty/                   # 第三方库
│   └── json.hpp                  # nlohmann/json
├── CMakeLists.txt                # 根CMake配置
├── autobuild.sh                  # 自动构建脚本
└── README.md                     # 项目说明
```

## 快速开始

### 环境要求

- CMake 3.0+
- GCC 7+ 或 Clang 5+ (支持C++17)
- MySQL 5.7+
- Redis 5.0+
- Muduo 网络库
- Nginx (用于集群负载均衡,可选)

### 数据库准备

1. 创建数据库
```sql
CREATE DATABASE chat DEFAULT CHARACTER SET utf8mb4;
```

2. 创建用户表
```sql
CREATE TABLE User (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(50) NOT NULL,
    state ENUM('online', 'offline') DEFAULT 'offline'
);
```

3. 创建群组表
```sql
CREATE TABLE AllGroup (
    id INT PRIMARY KEY AUTO_INCREMENT,
    groupname VARCHAR(50) NOT NULL,
    groupdesc VARCHAR(200) DEFAULT ''
);
```

4. 创建群成员表
```sql
CREATE TABLE GroupUser (
    groupid INT NOT NULL,
    userid INT NOT NULL,
    grouprole ENUM('creator', 'normal') DEFAULT 'normal',
    PRIMARY KEY (groupid, userid)
);
```

5. 创建好友表
```sql
CREATE TABLE Friend (
    userid INT NOT NULL,
    friendid INT NOT NULL,
    PRIMARY KEY (userid, friendid)
);
```

6. 创建离线消息表
```sql
CREATE TABLE OfflineMessage (
    userid INT NOT NULL,
    message TEXT NOT NULL
);
```

### 编译构建

```bash
# 使用自动构建脚本
chmod +x autobuild.sh
./autobuild.sh

# 或手动构建
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

编译完成后,可执行文件将生成在 `bin/` 目录:
- `ChatServer` - 聊天服务器
- `ChatClient` - 聊天客户端

### 运行服务器

```bash
./ChatServer <IP地址> <端口号> <服务器名称>

# 示例
./ChatServer 127.0.0.1 6000 Server1
```

服务器启动后会:
- 监听指定IP和端口
- 连接MySQL数据库
- 连接Redis服务
- 订阅Redis消息频道

### 运行客户端

```bash
./ChatClient <服务器IP> <服务器端口>

# 示例
./ChatClient 127.0.0.1 6000
```

客户端使用交互式菜单:
```
========================
1. login          # 登录
2. register       # 注册
3. quit           # 退出
========================
choice:
```


### 消息类型 (msgid)

| msgid | 功能描述 | 消息方向 |
|-------|---------|---------|
| 1 | 登录 | Client→Server |
| 2 | 注册 | Client→Server |
| 3 | 用户异常退出 | Server 处理 |
| 4 | 一对一聊天 | Client→Server |
| 5 | 添加好友 | Client→Server |
| 6 | 创建群组 | Client→Server |
| 7 | 加入群组 | Client→Server |
| 8 | 群组聊天 | Client→Server |
| 9 | 用户正常退出 | Client→Server |

### 消息格式

项目使用 JSON 格式进行数据交换,示例:

**登录请求:**
```json
{
  "msgid": 1,
  "id": 1,
  "password": "123456"
}
```

**注册请求:**
```json
{
  "msgid": 2,
  "name": "username",
  "password": "123456"
}
```

**单聊消息:**
```json
{
  "msgid": 4,
  "id": 1,
  "toid": 2,
  "message": "Hello!"
}
```

**群聊消息:**
```json
{
  "msgid": 8,
  "id": 1,
  "groupid": 100,
  "message": "Hello Group!"
}
```

## 集群架构

### 单机模式

```
┌─────────────┐
│ ChatClient  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ ChatServer  │
│  - Muduo    │
│  - MySQL    │
│  - Redis    │
└─────────────┘
```

### 集群模式 (Nginx负载均衡)

```
                         ┌─────────────┐
                         │  Nginx      │
                         │  (TCP代理)   │
                         └──────┬──────┘
                                │
              ┌─────────────────┼─────────────────┐
              ▼                 ▼                 ▼
       ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
       │ ChatServer  │  │ ChatServer  │  │ ChatServer  │
       │   Server1   │  │   Server2   │  │   Server3   │
       └──────┬──────┘  └──────┬──────┘  └──────┬──────┘
              │                │                │
              └────────────────┴────────────────┘
                               │
                               ▼
                        ┌─────────────┐
                        │   Redis     │
                        │ (Pub/Sub)   │
                        └──────┬──────┘
                               │
                               ▼
                        ┌─────────────┐
                        │   MySQL     │
                        │ (Database)  │
                        └─────────────┘
```

**跨服务器通信机制:**

1. 每个ChatServer订阅Redis的频道 (如 `chat_channel_1`, `chat_channel_2`)
2. 当用户A(Server1)给用户B(Server2)发消息时:
   - Server1将消息发布到Redis频道
   - Server2从Redis订阅收到消息
   - Server2转发给本机连接的用户B

### Nginx配置示例

```nginx
stream {
    upstream chat_backend {
        server 127.0.0.1:6000 weight=1;
        server 127.0.0.1:6001 weight=1;
        server 127.0.0.1:6002 weight=1;
    }

    server {
        listen 8000;
        proxy_pass chat_backend;
        proxy_timeout 300s;
        proxy_connect_timeout 75s;
    }
}
```

## 性能测试

### 测试环境

- CPU: R7945HX
- 内存: 16G
- 网络: 本地回环

### 测试结果

| 场景 | 并发连接数 | 消息吞吐 |
|------|-----------|---------|
| 单台服务器(全部在线) | 10,000 | 35,000 msg/s |
| 单台服务器(一半在线) | 10,000 | 12,000 msg/s |
| 两台服务器(负载均衡) | 20,000 | 20,000 msg/s |

### 运行压力测试

```bash
# 使用内置的压力测试程序
./ChatClient <ip> <port> stress
```

## 配置说明

### 数据库配置

修改 `src/server/main.cpp` 中的数据库连接信息:
```cpp
static string server = "127.0.0.1";      // MySQL服务器地址
static string user = "root";              // MySQL用户名
static string password = "your_password"; // MySQL密码
static string dbname = "chat";            // 数据库名
```

### Redis配置

Redis连接配置在 `src/server/redis/redis.cpp` 中,默认连接 `127.0.0.1:6379`。

## 核心设计

### 1. Reactor网络模型

基于Muduo网络库的Reactor模式:
```
EventLoop (事件循环)
    ↓
Poller (epoll/kqueue)
    ↓
Channel (事件分发)
    ↓
TcpConnection/TcpServer
```

### 2. 单例业务服务类

`ChatService` 使用单例模式管理所有业务逻辑:
- 消息处理函数映射表 (`msgHandlerMap_`)
- 用户连接管理 (`userConnMap_`)
- 线程安全管理 (`ConnMapMutex_`)

### 3. 连接池设计

- **MySQL连接池**: `ConnectionPool<MysqlConnection>`
- **Redis连接池**: `ConnectionPool<Redis>`
- 减少频繁创建/销毁连接的开销
- 提高数据库访问性能

### 4. 线程安全

- 使用 `std::mutex` 保护共享状态
- `std::atomic<int>` 管理原子计数器
- `std::thread` 创建独立工作线程

## 待完善功能

- [ ] 心跳检测机制
- [ ] 消息确认与重传
- [ ] 在线状态实时同步
- [ ] 消息历史记录查询
- [ ] 文件传输功能
- [ ] Web客户端支持
- [ ] 配置文件的读取
- [ ] 日志系统完善
- [ ] Docker容器化部署

## 常见问题

### Q1: 编译报错 "muduo头文件找不到"

**解决**: 安装Muduo库
```bash
# Ubuntu/Debian
sudo apt-get install libmuduo-dev

# 或从源码编译
```

### Q2: MySQL连接失败

**排查步骤**:
1. 检查MySQL服务是否运行：`systemctl status mysql`
2. 检查连接参数是否正确
3. 检查防火墙设置
4. 确认用户有远程连接权限

### Q3: Redis连接失败

**排查步骤**:
1. 检查Redis服务：`redis-cli ping`
2. 检查Redis配置 `bind` 和 `protected-mode`
3. 确认端口6379未被占用

### Q4: 集群模式下消息无法跨服务器

**检查**:
1. 所有服务器是否订阅相同的Redis频道
2. Redis Pub/Sub是否正常工作
3. 消息的序列化/反序列化是否正确

