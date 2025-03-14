#pragma once
#include "../Log.hpp"
#include <vector>
#include <cassert>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

using namespace log_ns;

// 定义最大监听队列长度
#define MAX_LISTEN 1024

// Socket类，封装了套接字的基本操作
class Socket
{
private:
    // 套接字文件描述符
    int _sockfd;

public:
    // 默认构造函数，初始化套接字文件描述符为 -1
    Socket() : _sockfd(-1) {}
    // 带参数的构造函数，使用传入的文件描述符初始化套接字
    Socket(int fd) : _sockfd(fd) {}
    // 析构函数，在对象销毁时关闭套接字
    ~Socket() { Close(); }
    // 获取套接字文件描述符
    int Fd() { return _sockfd; }

    // 创建套接字
    bool Create()
    {
        // 调用系统函数 socket 创建一个 TCP 套接字
        // int socket(int domain, int type, int protocol)
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        // 如果创建失败，记录错误日志并返回 false
        if (_sockfd < 0)
        {
            LOG(ERROR, "CREATE SOCKET FAILED!!\n");
            return false;
        }
        // 创建成功，返回 true
        return true;
    }

    // 绑定地址信息
    bool Bind(const std::string &ip, uint16_t port)
    {
        // 定义一个 IPv4 地址结构体
        struct sockaddr_in addr;
        // 设置地址族为 IPv4
        addr.sin_family = AF_INET;
        // 将端口号从主机字节序转换为网络字节序
        addr.sin_port = htons(port);
        // 将 IP 地址从点分十进制字符串转换为网络字节序的整数
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        // 获取地址结构体的长度
        socklen_t len = sizeof(struct sockaddr_in);
        // 调用系统函数 bind 将套接字与指定的地址和端口绑定
        // int bind(int sockfd, struct sockaddr*addr, socklen_t len);
        int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
        // 如果绑定失败，记录错误日志并返回 false
        if (ret < 0)
        {
            LOG(ERROR, "BIND ADDRESS FAILED!\n");
            return false;
        }
        // 绑定成功，返回 true
        return true;
    }

    // 开始监听
    bool Listen(int backlog = MAX_LISTEN)
    {
        // 调用系统函数 listen 开始监听连接请求
        // int listen(int backlog)
        int ret = listen(_sockfd, backlog);
        // 如果监听失败，记录错误日志并返回 false
        if (ret < 0)
        {
            LOG(ERROR, "SOCKET LISTEN FAILED!\n");
            return false;
        }
        // 监听成功，返回 true
        return true;
    }

    // 向服务器发起连接
    bool Connect(const std::string &ip, uint16_t port)
    {
        // 定义一个 IPv4 地址结构体
        struct sockaddr_in addr;
        // 设置地址族为 IPv4
        addr.sin_family = AF_INET;
        // 将端口号从主机字节序转换为网络字节序
        addr.sin_port = htons(port);
        // 将 IP 地址从点分十进制字符串转换为网络字节序的整数
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        // 获取地址结构体的长度
        socklen_t len = sizeof(struct sockaddr_in);
        // 调用系统函数 connect 向指定的服务器地址和端口发起连接
        // int connect(int sockfd, struct sockaddr*addr, socklen_t len);
        int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
        // 如果连接失败，记录错误日志并返回 false
        if (ret < 0)
        {
            LOG(ERROR, "CONNECT SERVER FAILED!\n");
            return false;
        }
        // 连接成功，返回 true
        return true;
    }

    // 获取新连接
    int Accept()
    {
        // 调用系统函数 accept 接受一个新的连接请求
        // int accept(int sockfd, struct sockaddr *addr, socklen_t *len);
        int newfd = accept(_sockfd, NULL, NULL);
        // 如果接受失败，记录错误日志并返回 -1
        if (newfd < 0)
        {
            LOG(ERROR, "SOCKET ACCEPT FAILED!\n");
            return -1;
        }
        // 接受成功，返回新的套接字文件描述符
        return newfd;
    }

    // 接收数据
    ssize_t Recv(void *buf, size_t len, int flag = 0)
    {
        // 调用系统函数 recv 从套接字接收数据
        // ssize_t recv(int sockfd, void *buf, size_t len, int flag);
        ssize_t ret = recv(_sockfd, buf, len, flag);
        // 如果接收失败或没有接收到数据
        if (ret <= 0)
        {
            // EAGAIN 当前 socket 的接收缓冲区中没有数据了，在非阻塞的情况下才会有这个错误
            // EINTR  表示当前 socket 的阻塞等待，被信号打断了
            if (errno == EAGAIN || errno == EINTR)
            {
                // 表示这次接收没有接收到数据，返回 0
                return 0;
            }
            // 其他错误情况，记录错误日志并返回 -1
            LOG(ERROR, "SOCKET RECV FAILED!!\n");
            return -1;
        }
        // 返回实际接收的数据长度
        return ret;
    }

    // 非阻塞接收数据
    ssize_t NonBlockRecv(void *buf, size_t len)
    {
        // 调用 Recv 函数并设置 MSG_DONTWAIT 标志，表示非阻塞接收
        return Recv(buf, len, MSG_DONTWAIT);
    }

    // 发送数据
    ssize_t Send(const void *buf, size_t len, int flag = 0)
    {
        // 调用系统函数 send 向套接字发送数据
        // ssize_t send(int sockfd, void *data, size_t len, int flag);
        ssize_t ret = send(_sockfd, buf, len, flag);
        // 如果发送失败
        if (ret < 0)
        {
            // EAGAIN 表示发送缓冲区已满，在非阻塞的情况下会出现
            // EINTR 表示发送操作被信号打断
            if (errno == EAGAIN || errno == EINTR)
            {
                // 表示这次发送没有成功，返回 0
                return 0;
            }
            // 其他错误情况，记录错误日志并返回 -1
            LOG(ERROR, "SOCKET SEND FAILED!!\n");
            return -1;
        }
        // 返回实际发送的数据长度
        return ret;
    }

    // 非阻塞发送数据
    ssize_t NonBlockSend(void *buf, size_t len)
    {
        // 如果要发送的数据长度为 0，直接返回 0
        if (len == 0)
            return 0;
        // 调用 Send 函数并设置 MSG_DONTWAIT 标志，表示非阻塞发送
        return Send(buf, len, MSG_DONTWAIT);
    }

    // 关闭套接字
    void Close()
    {
        // 如果套接字文件描述符不为 -1，表示套接字是打开的
        if (_sockfd != -1)
        {
            // 调用系统函数 close 关闭套接字
            close(_sockfd);
            // 将套接字文件描述符置为 -1，表示已关闭
            _sockfd = -1;
        }
    }

    // 创建一个服务端连接
    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = false)
    {
        // 1. 创建套接字
        if (Create() == false)
            return false;
        // 4. 如果需要，设置套接字为非阻塞模式
        if (block_flag)
            NonBlock();
        // 2. 绑定地址信息
        if (Bind(ip, port) == false)
            return false;
        // 3. 开始监听连接请求
        if (Listen() == false)
            return false;
        // 5. 启动地址端口重用
        ReuseAddress();
        // 所有步骤都成功，返回 true
        return true;
    }

    // 创建一个客户端连接
    bool CreateClient(uint16_t port, const std::string &ip)
    {
        // 1. 创建套接字
        if (Create() == false)
            return false;
        // 2. 向指定的服务器地址和端口发起连接
        if (Connect(ip, port) == false)
            return false;
        // 所有步骤都成功，返回 true
        return true;
    }

    // 设置套接字选项---开启地址端口重用
    void ReuseAddress()
    {
        // 定义一个整数变量，用于设置选项的值
        int val = 1;
        // 调用系统函数 setsockopt 开启地址重用选项
        // int setsockopt(int fd, int leve, int optname, void *val, int vallen)
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
        // 再次设置值为 1
        val = 1;
        // 调用系统函数 setsockopt 开启端口重用选项
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
    }

    // 设置套接字阻塞属性-- 设置为非阻塞
    void NonBlock()
    {
        // 调用系统函数 fcntl 获取当前套接字的文件状态标志
        // int fcntl(int fd, int cmd, ... /* arg */ );
        int flag = fcntl(_sockfd, F_GETFL, 0);
        // 将 O_NONBLOCK 标志与当前标志按位或，设置为非阻塞模式
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
};