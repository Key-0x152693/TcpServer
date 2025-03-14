#pragma once
#include <iostream>
#include <stdint.h>
#include <functional>
#include <sys/epoll.h>

// 前向声明 EventLoop 类，避免头文件循环包含
class EventLoop;

// Channel 类用于管理文件描述符的事件，与 EventLoop 配合使用
class Channel
{
private:
    int _fd;  // 文件描述符，代表要监控的对象，如套接字
    EventLoop *_loop;  // 指向所属的 EventLoop 对象，用于事件循环和处理
    uint32_t _events;  // 当前需要监控的事件，使用 epoll 事件标志位表示
    uint32_t _revents; // 当前连接触发的事件，由 epoll 实际返回的事件
    // 定义事件回调函数类型，使用 std::function 包装无参数无返回值的函数
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;  // 可读事件被触发时调用的回调函数
    EventCallback _write_callback; // 可写事件被触发时调用的回调函数
    EventCallback _error_callback; // 错误事件被触发时调用的回调函数
    EventCallback _close_callback; // 连接断开事件被触发时调用的回调函数
    EventCallback _event_callback; // 任意事件被触发时调用的回调函数

public:
    // 构造函数，初始化 Channel 对象
    // loop: 所属的 EventLoop 对象
    // fd: 要监控的文件描述符
    Channel(EventLoop *loop, int fd) : _fd(fd), _events(0), _revents(0), _loop(loop) {}

    // 获取文件描述符
    int Fd() { return _fd; }

    // 获取想要监控的事件
    uint32_t Events() { return _events; }

    // 设置实际就绪的事件，由 epoll 返回的事件更新此值
    void SetREvents(uint32_t events) { _revents = events; }

    // 设置可读事件的回调函数
    void SetReadCallback(const EventCallback &cb) { _read_callback = cb; }

    // 设置可写事件的回调函数
    void SetWriteCallback(const EventCallback &cb) { _write_callback = cb; }

    // 设置错误事件的回调函数
    void SetErrorCallback(const EventCallback &cb) { _error_callback = cb; }

    // 设置连接断开事件的回调函数
    void SetCloseCallback(const EventCallback &cb) { _close_callback = cb; }

    // 设置任意事件的回调函数
    void SetEventCallback(const EventCallback &cb) { _event_callback = cb; }

    // 判断当前是否监控了可读事件
    bool ReadAble() { return (_events & EPOLLIN); }

    // 判断当前是否监控了可写事件
    bool WriteAble() { return (_events & EPOLLOUT); }

    // 启动读事件监控
    void EnableRead()
    {
        // 将 EPOLLIN 标志位添加到 _events 中，表示要监控可读事件
        _events |= EPOLLIN;
        // 调用 Update 函数更新 epoll 对该文件描述符的监控事件
        Update();
    }

    // 启动写事件监控
    void EnableWrite()
    {
        // 将 EPOLLOUT 标志位添加到 _events 中，表示要监控可写事件
        _events |= EPOLLOUT;
        // 调用 Update 函数更新 epoll 对该文件描述符的监控事件
        Update();
    }

    // 关闭读事件监控
    void DisableRead()
    {
        // 从 _events 中移除 EPOLLIN 标志位，表示不再监控可读事件
        _events &= ~EPOLLIN;
        // 调用 Update 函数更新 epoll 对该文件描述符的监控事件
        Update();
    }

    // 关闭写事件监控
    void DisableWrite()
    {
        // 从 _events 中移除 EPOLLOUT 标志位，表示不再监控可写事件
        _events &= ~EPOLLOUT;
        // 调用 Update 函数更新 epoll 对该文件描述符的监控事件
        Update();
    }

    // 关闭所有事件监控
    void DisableAll()
    {
        // 将 _events 置为 0，表示不监控任何事件
        _events = 0;
        // 调用 Update 函数更新 epoll 对该文件描述符的监控事件
        Update();
    }

    // 移除监控，具体实现可能在其他文件中
    void Remove();

    // 更新 epoll 对该文件描述符的监控事件，具体实现可能在其他文件中
    void Update();

    // 事件处理函数，当连接触发事件时调用此函数
    void HandleEvent()
    {
        // 检查是否有可读事件、对端关闭连接或紧急数据可读事件
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            // 如果设置了可读事件回调函数，则调用该函数
            if (_read_callback)
                _read_callback();
        }
        // 检查是否有可写事件
        if (_revents & EPOLLOUT)
        {
            // 如果设置了可写事件回调函数，则调用该函数
            if (_write_callback)
                _write_callback();
        }
        // 检查是否有错误事件
        else if (_revents & EPOLLERR)
        {
            // 如果设置了错误事件回调函数，则调用该函数
            if (_error_callback)
                _error_callback(); // 一旦出错，可能会释放连接，所以优先处理
        }
        // 检查是否有连接断开事件
        else if (_revents & EPOLLHUP)
        {
            // 如果设置了连接断开事件回调函数，则调用该函数
            if (_close_callback)
                _close_callback();
        }
        // 如果设置了任意事件回调函数，则调用该函数
        if (_event_callback)
            _event_callback();
    }
};