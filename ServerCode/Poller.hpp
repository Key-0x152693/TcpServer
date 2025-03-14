#include "../Log.hpp"
#include "Channel.hpp"
#include <vector>
#include <cassert>
#include <unordered_map>

// 定义最大的 epoll 事件数量
#define MAX_EPOLLEVENTS 1024

using namespace log_ns;

// Poller 类用于封装 epoll 的操作，实现对文件描述符的事件监控
class Poller
{
private:
    // epoll 实例的文件描述符
    int _epfd;
    // 用于存储 epoll_wait 返回的就绪事件
    struct epoll_event _evs[MAX_EPOLLEVENTS];
    // 存储文件描述符和对应的 Channel 对象指针的映射，方便查找和管理
    std::unordered_map<int, Channel *> _channels;

private:
    // 对 epoll 进行直接操作，包括添加、修改或删除监控事件
    void Update(Channel *channel, int op)
    {
        // epoll_ctl 函数的原型：int epoll_ctl(int epfd, int op,  int fd,  struct epoll_event *ev);
        // 获取 Channel 对象对应的文件描述符
        int fd = channel->Fd();
        // 定义 epoll_event 结构体，用于设置事件信息
        struct epoll_event ev;
        // 设置事件关联的文件描述符
        ev.data.fd = fd;
        // 设置需要监控的事件类型，从 Channel 对象中获取
        ev.events = channel->Events();
        // 调用 epoll_ctl 函数进行操作
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        // 如果操作失败，记录错误日志
        if (ret < 0)
        {
            LOG(ERROR, "EPOLLCTL FAILED!\n");
        }
        return;
    }

    // 判断一个 Channel 是否已经添加了事件监控
    bool HasChannel(Channel *channel)
    {
        // 在 _channels 映射中查找该 Channel 对应的文件描述符
        auto it = _channels.find(channel->Fd());
        // 如果未找到，说明该 Channel 未添加监控
        if (it == _channels.end())
        {
            return false;
        }
        return true;
    }

public:
    // 构造函数，初始化 epoll 实例
    Poller()
    {
        // 创建一个 epoll 实例，参数为最大事件数量
        _epfd = epoll_create(MAX_EPOLLEVENTS);
        // 如果创建失败，记录错误日志并退出程序
        if (_epfd < 0)
        {
            LOG(ERROR, "EPOLL CREATE FAILED!!\n");
            abort(); // 退出程序
        }
    }

    // 添加或修改监控事件
    void UpdateEvent(Channel *channel)
    {
        // 检查该 Channel 是否已经添加了事件监控
        bool ret = HasChannel(channel);
        if (ret == false)
        {
            // 如果未添加，则将该 Channel 插入到 _channels 映射中
            _channels.insert(std::make_pair(channel->Fd(), channel));
            // 调用 Update 函数，使用 EPOLL_CTL_ADD 操作添加监控事件
            return Update(channel, EPOLL_CTL_ADD);
        }
        // 如果已经添加，则调用 Update 函数，使用 EPOLL_CTL_MOD 操作修改监控事件
        return Update(channel, EPOLL_CTL_MOD);
    }

    // 移除监控
    void RemoveEvent(Channel *channel)
    {
        // 在 _channels 映射中查找该 Channel 对应的文件描述符
        auto it = _channels.find(channel->Fd());
        // 如果找到，则从映射中移除该元素
        if (it != _channels.end())
        {
            _channels.erase(it);
        }
        // 调用 Update 函数，使用 EPOLL_CTL_DEL 操作移除监控事件
        Update(channel, EPOLL_CTL_DEL);
    }

    // 开始监控，返回活跃连接
    void Poll(std::vector<Channel *> *active)
    {
        // epoll_wait 函数的原型：int epoll_wait(int epfd, struct epoll_event *evs, int maxevents, int timeout)
        // 调用 epoll_wait 函数进行事件监控，-1 表示无限等待
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1);
        // 如果调用失败
        if (nfds < 0)
        {
            // 如果是被信号打断，直接返回
            if (errno == EINTR)
            {
                return;
            }
            // 记录错误日志并退出程序
            LOG(ERROR, "EPOLL WAIT ERROR:%s\n", strerror(errno));
            abort(); // 退出程序
        }
        // 遍历所有就绪的事件
        for (int i = 0; i < nfds; i++)
        {
            // 在 _channels 映射中查找该事件对应的 Channel 对象
            auto it = _channels.find(_evs[i].data.fd);
            // 确保该 Channel 对象存在
            assert(it != _channels.end());
            // 设置该 Channel 对象实际就绪的事件
            it->second->SetREvents(_evs[i].events);
            // 将该 Channel 对象添加到活跃连接列表中
            active->push_back(it->second);
        }
        return;
    }
};