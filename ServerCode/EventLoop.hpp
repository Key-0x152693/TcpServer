#pragma once
#include "../Log.hpp"
#include "Poller.hpp"
#include "TimerWheel.hpp"
#include <mutex>
#include <thread>
#include <functional>
#include <sys/eventfd.h>

// EventLoop类，负责事件循环、任务调度和定时器管理
class EventLoop
{
private:
    // 定义Functor类型，为一个无参数无返回值的函数对象
    using Functor = std::function<void()>;
    // 存储当前EventLoop所在线程的ID
    std::thread::id _thread_id; 
    // eventfd文件描述符，用于唤醒可能因IO事件监控而阻塞的线程
    int _event_fd;              
    // 智能指针，管理一个Channel对象，用于处理eventfd的事件
    std::unique_ptr<Channel> _event_channel;
    // Poller对象，用于进行所有描述符的事件监控
    Poller _poller;             
    // 任务池，存储待执行的任务
    std::vector<Functor> _tasks; 
    // 互斥锁，用于保证任务池操作的线程安全
    std::mutex _mutex;           
    // 定时器模块对象，用于管理定时任务
    TimerWheel _timer_wheel;     

public:
    // 执行任务池中的所有任务
    void RunAllTask()
    {
        // 临时存储任务的向量
        std::vector<Functor> functor;
        {
            // 加锁，确保任务池操作的线程安全
            std::unique_lock<std::mutex> _lock(_mutex);
            // 将任务池中的任务交换到临时向量中
            _tasks.swap(functor);
        }
        // 遍历临时向量，执行其中的每个任务
        for (auto &f : functor)
        {
            f();
        }
        return;
    }

    // 创建一个eventfd文件描述符
    static int CreateEventFd()
    {
        // 创建一个非阻塞的、在exec时关闭的eventfd
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        // 如果创建失败，记录错误日志并终止程序
        if (efd < 0)
        {
            LOG(ERROR, "CREATE EVENTFD FAILED!!\n");
            abort(); 
        }
        return efd;
    }

    // 读取eventfd中的数据
    void ReadEventfd()
    {
        // 用于存储读取的数据
        uint64_t res = 0;
        // 从eventfd中读取数据
        int ret = read(_event_fd, &res, sizeof(res));
        // 如果读取失败
        if (ret < 0)
        {
            // 如果是被信号打断或者无数据可读，直接返回
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }
            // 否则记录错误日志并终止程序
            LOG(ERROR, "READ EVENTFD FAILED!\n");
            abort();
        }
        return;
    }

    // 向eventfd中写入数据，用于唤醒可能阻塞的线程
    void WeakUpEventFd()
    {
        // 要写入的数据
        uint64_t val = 1;
        // 向eventfd中写入数据
        int ret = write(_event_fd, &val, sizeof(val));
        // 如果写入失败
        if (ret < 0)
        {
            // 如果是被信号打断，直接返回
            if (errno == EINTR)
            {
                return;
            }
            // 否则记录错误日志并终止程序
            LOG(ERROR, "READ EVENTFD FAILED!\n");
            abort();
        }
        return;
    }

    // 构造函数，初始化EventLoop对象
    EventLoop() : _thread_id(std::this_thread::get_id()),
                  _event_fd(CreateEventFd()),
                  _event_channel(new Channel(this, _event_fd)),
                  _timer_wheel(this)
    {
        // 为eventfd的Channel对象设置可读事件的回调函数
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventfd, this));
        // 启动eventfd的读事件监控
        _event_channel->EnableRead();
    }

    // 启动事件循环，包括事件监控、事件处理和任务执行
    void Start()
    {
        while (1)
        {
            // 1. 事件监控，获取所有就绪的Channel对象
            std::vector<Channel *> actives;
            _poller.Poll(&actives);
            // 2. 事件处理，调用每个就绪Channel的事件处理函数
            for (auto &channel : actives)
            {
                channel->HandleEvent();
            }
            // 3. 执行任务池中的所有任务
            RunAllTask();
        }
    }

    // 判断当前线程是否是EventLoop所在的线程
    bool IsInLoop()
    {
        return (_thread_id == std::this_thread::get_id());
    }

    // 断言当前线程是EventLoop所在的线程
    void AssertInLoop()
    {
        assert(_thread_id == std::this_thread::get_id());
    }

    // 判断任务是否在当前线程中执行，如果是则直接执行，否则加入任务池
    void RunInLoop(const Functor &cb)
    {
        if (IsInLoop())
        {
            return cb();
        }
        return QueueInLoop(cb);
    }

    // 将任务加入任务池，并唤醒可能阻塞的线程
    void QueueInLoop(const Functor &cb)
    {
        {
            // 加锁，确保任务池操作的线程安全
            std::unique_lock<std::mutex> _lock(_mutex);
            // 将任务加入任务池
            _tasks.push_back(cb);
        }
        // 唤醒可能因没有事件就绪而阻塞的线程
        WeakUpEventFd();
    }

    // 添加或修改描述符的事件监控
    void UpdateEvent(Channel *channel) { return _poller.UpdateEvent(channel); }
    // 移除描述符的事件监控
    void RemoveEvent(Channel *channel) { return _poller.RemoveEvent(channel); }
    // 添加一个定时任务
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) { return _timer_wheel.TimerAdd(id, delay, cb); }
    // 刷新/延迟定时任务
    void TimerRefresh(uint64_t id) { return _timer_wheel.TimerRefresh(id); }
    // 取消定时任务
    void TimerCancel(uint64_t id) { return _timer_wheel.TimerCancel(id); }
    // 判断是否存在指定ID的定时任务
    bool HasTimer(uint64_t id) { return _timer_wheel.HasTimer(id); }
};

// Channel类的Remove方法实现，调用EventLoop的RemoveEvent方法移除事件监控
void Channel::Remove() { return _loop->RemoveEvent(this); }
// Channel类的Update方法实现，调用EventLoop的UpdateEvent方法更新事件监控
void Channel::Update() { return _loop->UpdateEvent(this); }

// TimerWheel类的TimerAdd方法实现，将定时器添加操作封装到任务中，在EventLoop线程中执行
void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}

// 刷新/延迟定时任务，将操作封装到任务中，在EventLoop线程中执行
void TimerWheel::TimerRefresh(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}

// 取消定时任务，将操作封装到任务中，在EventLoop线程中执行
void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}