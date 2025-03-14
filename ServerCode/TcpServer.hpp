#pragma once
#include "LoopThreadPool.hpp"
#include"Connection.hpp"
#include "Acceptor.hpp"

// TcpServer 类用于创建和管理一个 TCP 服务器
class TcpServer
{
private:
    // 自动增长的连接 ID，用于唯一标识每个连接
    uint64_t _next_id; 
    // 服务器监听的端口号
    int _port; 
    // 非活跃连接的统计时间，即多长时间无通信被认为是非活跃连接
    int _timeout;                                       
    // 是否启动了非活跃连接超时销毁的判断标志
    bool _enable_inactive_release;                      
    // 主线程的 EventLoop 对象，负责监听事件的处理
    EventLoop _baseloop;                                
    // 监听套接字的管理对象
    Acceptor _acceptor;                                 
    // 从属 EventLoop 线程池
    LoopThreadPool _pool;                               
    // 保存管理所有连接对应的 shared_ptr 对象，键为连接 ID，值为连接对象指针
    std::unordered_map<uint64_t, PtrConnection> _conns; 

    // 连接建立成功时的回调函数类型
    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    // 接收到消息时的回调函数类型
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    // 连接关闭时的回调函数类型
    using ClosedCallback = std::function<void(const PtrConnection &)>;
    // 发生任意事件时的回调函数类型
    using AnyEventCallback = std::function<void(const PtrConnection &)>;
    // 通用任务函数类型
    using Functor = std::function<void()>; 

    // 连接建立成功时的回调函数
    ConnectedCallback _connected_callback;
    // 接收到消息时的回调函数
    MessageCallback _message_callback;
    // 连接关闭时的回调函数
    ClosedCallback _closed_callback;
    // 发生任意事件时的回调函数
    AnyEventCallback _event_callback; 

private:
    // 在主线程的 EventLoop 中添加一个定时任务
    void RunAfterInLoop(const Functor &task, int delay)
    {
        // 生成一个新的连接 ID
        _next_id++; 
        // 在主线程的 EventLoop 中添加一个定时任务
        _baseloop.TimerAdd(_next_id, delay, task); 
    }

    // 为新连接构造一个 Connection 进行管理
    void NewConnection(int fd)
    {
        // 生成一个新的连接 ID
        _next_id++; 
        // 创建一个新的连接对象，分配到线程池中的一个 EventLoop 上
        PtrConnection conn(new Connection(_pool.NextLoop(), _next_id, fd)); 
        // 设置接收到消息时的回调函数
        conn->SetMessageCallback(_message_callback); 
        // 设置连接关闭时的回调函数
        conn->SetClosedCallback(_closed_callback); 
        // 设置连接建立成功时的回调函数
        conn->SetConnectedCallback(_connected_callback); 
        // 设置发生任意事件时的回调函数
        conn->SetAnyEventCallback(_event_callback); 
        // 设置服务器内部的连接关闭回调函数
        conn->SetSrvClosedCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1)); 
        // 如果启用了非活跃连接超时销毁功能，则启动该连接的非活跃超时销毁
        if (_enable_inactive_release)
            conn->EnableInactiveRelease(_timeout); 
        // 连接就绪初始化
        conn->Established(); 
        // 将新连接添加到连接管理表中
        _conns.insert(std::make_pair(_next_id, conn)); 
    }

    // 在主线程的 EventLoop 中移除一个连接信息
    void RemoveConnectionInLoop(const PtrConnection &conn)
    {
        // 获取连接的 ID
        int id = conn->Id(); 
        // 在连接管理表中查找该连接
        auto it = _conns.find(id); 
        // 如果找到该连接，则从管理表中移除
        if (it != _conns.end())
        {
            _conns.erase(it);
        }
    }

    // 从管理 Connection 的 _conns 中移除连接信息
    void RemoveConnection(const PtrConnection &conn)
    {
        // 将移除连接信息的任务添加到主线程的 EventLoop 中执行
        _baseloop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn)); 
    }

public:
    // 构造函数，初始化服务器
    TcpServer(int port) : _port(port),
                          _next_id(0),
                          _enable_inactive_release(false),
                          _acceptor(&_baseloop, port),
                          _pool(&_baseloop)
    {
        // 设置接受器的回调函数，当有新连接时调用 NewConnection 函数
        _acceptor.SetAcceptCallback(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
        // 启动监听套接字的读事件监控
        _acceptor.Listen(); 
    }

    // 设置线程池中的线程数量
    void SetThreadCount(int count) { return _pool.SetThreadCount(count); }
    // 设置连接建立成功时的回调函数
    void SetConnectedCallback(const ConnectedCallback &cb) { _connected_callback = cb; }
    // 设置接收到消息时的回调函数
    void SetMessageCallback(const MessageCallback &cb) { _message_callback = cb; }
    // 设置连接关闭时的回调函数
    void SetClosedCallback(const ClosedCallback &cb) { _closed_callback = cb; }
    // 设置发生任意事件时的回调函数
    void SetAnyEventCallback(const AnyEventCallback &cb) { _event_callback = cb; }

    // 启用非活跃连接超时销毁功能，并设置超时时间
    void EnableInactiveRelease(int timeout)
    {
        _timeout = timeout;
        _enable_inactive_release = true;
    }

    // 用于添加一个定时任务
    void RunAfter(const Functor &task, int delay)
    {
        // 将添加定时任务的操作添加到主线程的 EventLoop 中执行
        _baseloop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop, this, task, delay)); 
    }

    // 启动服务器
    void Start()
    {
        // 创建线程池中的线程
        _pool.Create(); 
        // 启动主线程的 EventLoop
        _baseloop.Start(); 
    }
};