#include "mysqlconnection.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <atomic>
using namespace std;

template <typename T>
class ConnectionPool
{
public:
    static ConnectionPool<T> *getInstance();
    // 初始化连接池
    void init_pool(const string &ip, int port, const string &user, const string &password,
                   const string &dbname, int initSize, int maxSize, int idleTime, int timeout);
    shared_ptr<T> getConnection();

private:
    void produceConnection(); // 生产连接的线程的函数

    void monitorConnection(); // 监控连接的线程函数，回收空闲连接
    ConnectionPool() = default;
    ~ConnectionPool();
    // 删除拷贝构造函数和赋值运算符
    ConnectionPool(const ConnectionPool &) = delete;
    ConnectionPool(ConnectionPool &&) = delete;
    ConnectionPool &operator=(const ConnectionPool &) = delete;
    ConnectionPool &operator=(ConnectionPool &&) = delete;
    int maxSize_;              // 连接池最大连接数
    int minSize_;              // 最少连接数
    std::atomic<int> curSize_; // 当前连接数
    string ip_;                // ip地址
    int port_;                 // 端口号
    string user_;              // 用户名
    string password_;          // 密码
    string dbname_;            // 数据库名称
    int idleTime_;             // 连接的最大空闲时间,单位为秒
    int timeout_;              // 连接超时时间,单位毫秒
    // 连接队列
    queue<T *> connQue_;
    // 互斥锁
    mutex connQueMutex_;
    // 条件变量
    condition_variable cv_NeedConn_;
    condition_variable cv_NotEmpty_;
    // 连接池的状态
    atomic<bool> isRunning_;

    // 生产线程
    thread produceThread_;     //不用detach，防止析构时子线程访问无效资源
    // 检测线程
    thread monitorThread_;
};

template <typename T>
ConnectionPool<T> *ConnectionPool<T>::getInstance()
{
    static ConnectionPool<T> pool;
    return &pool;
}

template <typename T>
ConnectionPool<T>::~ConnectionPool()
{
    {
        lock_guard<mutex> lock(connQueMutex_);
        isRunning_ = false;
        cv_NeedConn_.notify_all();
        cv_NotEmpty_.notify_all();
    }

    produceThread_.join();
    monitorThread_.join();
}

template <typename T> // 初始化连接池的函数
void ConnectionPool<T>::init_pool(const string &ip, int port, const string &user, const string &password,
                                  const string &dbname, int initSize, int maxSize, int idleTime, int timeout)
{
    isRunning_ = true;
    ip_ = ip;
    port_ = port;
    user_ = user;
    password_ = password;
    dbname_ = dbname;
    minSize_ = initSize;
    maxSize_ = maxSize;
    idleTime_ = idleTime;
    timeout_ = timeout;
    curSize_ = 0;
    // 建立初始个数的连接
    {
        lock_guard<mutex> lock(connQueMutex_); // 以防万一
        for (int i = 0; i < minSize_; i++)
        {
            T *conn = new T();
            conn->connect(ip_, user_, password_, dbname_, port_);
            connQue_.push(conn);
        }
    }

    // 启动一个新的线程专门负责生产连接
    produceThread_ = thread(&ConnectionPool<T>::produceConnection, this);
    // 启动一个检测线程，回收空闲连接
    monitorThread_ = thread(&ConnectionPool<T>::monitorConnection, this);
}

template <typename T>
shared_ptr<T> ConnectionPool<T>::getConnection() // 获取连接
{
    unique_lock<mutex> lock(connQueMutex_);
    chrono::steady_clock::time_point now = chrono::steady_clock::now(); // 进入等待的起始时间
    while (connQue_.empty() && isRunning_)
    {
        cv_NeedConn_.notify_all(); // 告诉生产连接的线程需要生产连接了
        if (cv_status::timeout == cv_NotEmpty_.wait_for(lock, chrono::milliseconds(timeout_)))
        {
            cout << "数据库请求连接超时1" << endl; // 超时直接退出
            return nullptr;
        }
        else
        {
            chrono::steady_clock::time_point nowtime = chrono::steady_clock::now();
            // 累计等待时间超时
            if (chrono::duration_cast<chrono::milliseconds>(nowtime - now).count() >= timeout_)
            {
                cout << "数据库请求连接超时2" << endl;
                return nullptr;
            }
        }
    }

    // 获取连接
    if (connQue_.empty())
        return nullptr;
    shared_ptr<T> sp(connQue_.front(), [&](T *conn)
                     {
                         {
                             lock_guard<mutex> lock(connQueMutex_);
                             connQue_.push(conn);
                         }
                         conn->updateTime();        // 刷新使用时间
                         cv_NotEmpty_.notify_all(); // 唤醒等待连接的线程
                     });                            // 定义智能指针的删除器，将连接归还到连接队列中，注意删除器这里的参数类型与其封装的指针一致
    connQue_.pop();
    return sp; // 返回指针
}

template <typename T>
void ConnectionPool<T>::produceConnection() // 生产连接的线程的函数
{
    while (isRunning_)
    {
        unique_lock<mutex> lock(connQueMutex_);
        if (!isRunning_)
            return;                            // 防止唤醒丢失
        cv_NeedConn_.wait(lock);               // 等待唤醒
        if (curSize_ < maxSize_ && isRunning_) // 生产新的连接
        {
            lock.unlock();
            T *conn = new T(); // 将连接步骤移至锁外面，减少占用锁的时间
            int res = conn->connect(ip_, user_, password_, dbname_, port_);
            lock.lock();
            if (res)
            {
                connQue_.push(conn);
                curSize_++;
                cv_NotEmpty_.notify_all(); // 通知等待的线程
            }
            else
            {
                delete conn; // 连接失败，释放连接对象
            }
        }
    }
}

template <typename T>
void ConnectionPool<T>::monitorConnection() // 监控连接的线程函数，回收空闲连接
{
    while (isRunning_)
    {
        this_thread::sleep_for(chrono::seconds(idleTime_)); // 每隔一段时间进行一次回收
        {
            lock_guard<mutex> lock(connQueMutex_);
            while ( connQue_.size()> minSize_ && isRunning_)
            {
                T *conn = connQue_.front();
                if (conn->getIdleTime() > idleTime_)
                {
                    connQue_.pop();
                    curSize_--;
                    delete conn; // 释放连接
                }
                else
                {
                    break;
                }
            }
        }
    }
}
