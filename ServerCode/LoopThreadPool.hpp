#pragma once
#include"EventLoop.hpp"
#include <condition_variable>

// LoopThread 类用于管理一个单独的线程，该线程运行一个 EventLoop 实例
class LoopThread
{
private:
    /*用于实现_loop获取的同步关系，避免线程创建了，但是_loop还没有实例化之前去获取_loop*/
    std::mutex _mutex;             // 互斥锁，用于保护对 _loop 的访问
    std::condition_variable _cond; // 条件变量，用于线程间的同步
    EventLoop *_loop;              // EventLoop 指针变量，这个对象需要在线程内实例化
    std::thread _thread;           // EventLoop 对应的线程

private:
    /*实例化 EventLoop 对象，唤醒_cond上有可能阻塞的线程，并且开始运行EventLoop模块的功能*/
    void ThreadEntry()
    {
        // 在新线程中创建一个 EventLoop 实例
        EventLoop loop;
        {
            // 加锁，确保对 _loop 的访问是线程安全的
            std::unique_lock<std::mutex> lock(_mutex); 
            // 将 _loop 指针指向新创建的 EventLoop 实例
            _loop = &loop;
            // 唤醒所有在 _cond 上等待的线程
            _cond.notify_all(); 
        }
        // 启动 EventLoop 的事件循环
        loop.Start(); 
    }

public:
    /*创建线程，设定线程入口函数*/
    // 构造函数，初始化 _loop 为 NULL，并创建一个新线程，线程入口函数为 ThreadEntry
    LoopThread() : _loop(NULL), _thread(std::thread(&LoopThread::ThreadEntry, this)) {}

    /*返回当前线程关联的EventLoop对象指针*/
    EventLoop *GetLoop()
    {
        EventLoop *loop = NULL;
        {
            // 加锁，确保对 _loop 的访问是线程安全的
            std::unique_lock<std::mutex> lock(_mutex); 
            // 等待 _loop 不为 NULL，即等待 EventLoop 实例化完成
            _cond.wait(lock, [&]()
                       { return _loop != NULL; }); 
            // 将 _loop 的值赋给 loop
            loop = _loop; 
        }
        // 返回 EventLoop 指针
        return loop; 
    }
};

// LoopThreadPool 类用于管理一个 EventLoop 线程池
class LoopThreadPool
{
private:
    int _thread_count;  // 线程池中的线程数量
    int _next_idx;      // 用于轮询选择下一个 EventLoop 的索引
    EventLoop *_baseloop; // 主线程的 EventLoop
    std::vector<LoopThread *> _threads; // 存储 LoopThread 对象的指针
    std::vector<EventLoop *> _loops;    // 存储 EventLoop 对象的指针

public:
    // 构造函数，初始化线程数量为 0，索引为 0，并保存主线程的 EventLoop 指针
    LoopThreadPool(EventLoop *baseloop) : _thread_count(0), _next_idx(0), _baseloop(baseloop) {}

    // 设置线程池中的线程数量
    void SetThreadCount(int count) { _thread_count = count; }

    // 创建线程池中的所有线程和对应的 EventLoop
    void Create()
    {
        // 如果线程数量大于 0
        if (_thread_count > 0)
        {
            // 调整 _threads 和 _loops 向量的大小
            _threads.resize(_thread_count);
            _loops.resize(_thread_count);
            // 循环创建 LoopThread 对象，并获取对应的 EventLoop 指针
            for (int i = 0; i < _thread_count; i++)
            {
                _threads[i] = new LoopThread();
                _loops[i] = _threads[i]->GetLoop();
            }
        }
        return;
    }

    // 轮询选择下一个 EventLoop 指针
    EventLoop *NextLoop()
    {
        // 如果线程数量为 0，返回主线程的 EventLoop 指针
        if (_thread_count == 0)
        {
            return _baseloop;
        }
        // 更新索引，使用取模运算确保索引在有效范围内
        _next_idx = (_next_idx + 1) % _thread_count;
        // 返回下一个 EventLoop 指针
        return _loops[_next_idx];
    }
};