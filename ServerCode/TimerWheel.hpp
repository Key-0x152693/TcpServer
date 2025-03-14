#pragma once
#include "../Log.hpp"
#include "Channel.hpp"
#include <sys/timerfd.h>
#include <vector>
#include <memory>
#include <unordered_map>

using namespace log_ns;

// 定义任务函数类型，是一个无返回值、无参数的函数对象
using TaskFunc = std::function<void()>;
// 定义释放函数类型，是一个无返回值、无参数的函数对象
using ReleaseFunc = std::function<void()>;

// 定时器任务类，代表一个具体的定时任务
class TimerTask
{
private:
    uint64_t _id;         // 定时器任务对象的唯一标识符
    uint32_t _timeout;    // 定时任务的超时时间，单位为秒
    bool _canceled;       // 标记定时任务是否被取消，false 表示未取消，true 表示已取消
    TaskFunc _task_cb;    // 定时器任务要执行的回调函数
    ReleaseFunc _release; // 用于删除 TimerWheel 中保存的定时器对象信息的回调函数

public:
    // 构造函数，初始化定时器任务的相关信息
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb) 
        : _id(id), _timeout(delay), _task_cb(cb), _canceled(false) {}

    // 析构函数，如果任务未被取消，则执行任务回调函数，并调用释放函数
    ~TimerTask()
    {
        if (_canceled == false)
            _task_cb();
        _release();
    }

    // 取消定时器任务，将 _canceled 标记为 true
    void Cancel() { _canceled = true; }

    // 设置释放函数
    void SetRelease(const ReleaseFunc &cb) { _release = cb; }

    // 获取定时任务的超时时间
    uint32_t DelayTime() { return _timeout; }
};

// 定时器轮类，实现定时器的管理和调度
class TimerWheel
{
private:
    // 定义弱指针类型，用于存储定时器任务的弱引用
    using WeakTask = std::weak_ptr<TimerTask>;
    // 定义共享指针类型，用于存储定时器任务的共享引用
    using PtrTask = std::shared_ptr<TimerTask>;

    int _tick;     // 当前的时间刻度，类似于时钟的秒针，走到哪里就释放哪里的任务
    int _capacity; // 定时器轮的容量，即最大延迟时间，单位为秒
    // 定时器轮，二维向量，每个元素是一个存储定时器任务共享指针的向量
    std::vector<std::vector<PtrTask>> _wheel;
    // 存储定时器任务的映射，键为任务 ID，值为定时器任务的弱指针
    std::unordered_map<uint64_t, WeakTask> _timers;

    EventLoop *_loop; // 事件循环指针，用于将定时器任务的操作放入事件循环中执行
    int _timerfd;     // 定时器文件描述符，用于触发定时器事件
    // 定时器通道，用于处理定时器文件描述符的事件
    std::unique_ptr<Channel> _timer_channel;

private:
    // 从 _timers 映射中移除指定 ID 的定时器任务
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }

    // 创建定时器文件描述符，并设置定时器的超时时间和间隔时间
    static int CreateTimerfd()
    {
        // 创建一个单调时钟的定时器文件描述符
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            // 记录错误日志并终止程序
            LOG(ERROR, "TIMERFD CREATE FAILED!\n");
            abort();
        }

        // 定义定时器的超时时间和间隔时间
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;    // 第一次超时时间为 1 秒后
        itime.it_value.tv_nsec = 0;
        itime.it_interval.tv_sec = 1; // 第一次超时后，每次超时的间隔为 1 秒
        itime.it_interval.tv_nsec = 0;

        // 设置定时器的超时时间和间隔时间
        timerfd_settime(timerfd, 0, &itime, NULL);
        return timerfd;
    }

    // 读取定时器文件描述符，获取从上一次读取之后超时的次数
    int ReadTimefd()
    {
        uint64_t times;
        // 有可能因为其他描述符的事件处理花费事件比较长，然后在处理定时器描述符事件的时候，有可能就已经超时了很多次
        // 读取定时器文件描述符，获取超时次数
        int ret = read(_timerfd, &times, 8);
        if (ret < 0)
        {
            // 记录错误日志并终止程序
            LOG(ERROR, "READ TIMEFD FAILED!\n");
            abort();
        }
        return times;
    }

    // 每秒钟调用一次，模拟时钟的秒针向前走一步
    void RunTimerTask()
    {
        // 更新时间刻度，取模操作确保刻度在容量范围内循环
        _tick = (_tick + 1) % _capacity;
        // 清空当前刻度对应的定时器任务向量，释放其中的共享指针
        _wheel[_tick].clear(); 
    }

    // 处理定时器超时事件，根据实际超时次数执行相应的任务
    void OnTime()
    {
        // 读取定时器文件描述符，获取超时次数
        int times = ReadTimefd();
        // 根据超时次数，多次调用 RunTimerTask 函数
        for (int i = 0; i < times; i++)
        {
            RunTimerTask();
        }
    }

    // 在事件循环的线程中添加一个定时器任务
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb)
    {
        // 创建一个新的定时器任务对象
        PtrTask pt(new TimerTask(id, delay, cb));
        // 设置定时器任务的释放函数
        pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        // 计算定时器任务应该放置的位置
        int pos = (_tick + delay) % _capacity;
        // 将定时器任务添加到对应的位置
        _wheel[pos].push_back(pt);
        // 将定时器任务的弱指针添加到 _timers 映射中
        _timers[id] = WeakTask(pt);
    }

    // 在事件循环的线程中刷新一个定时器任务
    void TimerRefreshInLoop(uint64_t id)
    {
        // 在 _timers 映射中查找指定 ID 的定时器任务
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 未找到定时器任务，无法刷新
        }
        // 通过弱指针获取定时器任务的共享指针
        PtrTask pt = it->second.lock(); 
        if (!pt) return; // 如果共享指针为空，说明任务已被释放
        // 获取定时器任务的超时时间
        int delay = pt->DelayTime();
        // 计算定时器任务应该放置的新位置
        int pos = (_tick + delay) % _capacity;
        // 将定时器任务添加到新的位置
        _wheel[pos].push_back(pt);
    }

    // 在事件循环的线程中取消一个定时器任务
    void TimerCancelInLoop(uint64_t id)
    {
        // 在 _timers 映射中查找指定 ID 的定时器任务
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 未找到定时器任务，无法取消
        }
        // 通过弱指针获取定时器任务的共享指针
        PtrTask pt = it->second.lock();
        if (pt)
            pt->Cancel(); // 取消定时器任务
    }

public:
    // 构造函数，初始化定时器轮的相关信息
    TimerWheel(EventLoop *loop) 
        : _capacity(60), _tick(0), _wheel(_capacity), _loop(loop),
          _timerfd(CreateTimerfd()), _timer_channel(new Channel(_loop, _timerfd))
    {
        // 设置定时器通道的读事件回调函数为 OnTime
        _timer_channel->SetReadCallback(std::bind(&TimerWheel::OnTime, this));
        // 启动定时器通道的读事件监控
        _timer_channel->EnableRead(); 
    }

    // 在事件循环中添加一个定时器任务
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);

    // 在事件循环中刷新一个定时器任务
    void TimerRefresh(uint64_t id);

    // 在事件循环中取消一个定时器任务
    void TimerCancel(uint64_t id);

    // 检查是否存在指定 ID 的定时器任务
    //这个接口存在线程安全问题--这个接口实际上不能被外界使用者调用，只能在模块内，在对应的EventLoop线程内执行
    bool HasTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return false; // 未找到定时器任务
        }
        return true;
    }
};


