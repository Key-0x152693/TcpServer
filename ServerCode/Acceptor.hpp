#include"Socket.hpp"
#include "EventLoop.hpp"

// Acceptor 类负责创建监听套接字，并处理新的连接请求
class Acceptor
{
private:
    // 用于创建监听套接字，封装了套接字的基本操作
    Socket _socket;   
    // 用于对监听套接字进行事件监控，EventLoop 负责事件的循环和处理
    EventLoop *_loop; 
    // 用于对监听套接字进行事件管理，将事件和回调函数关联起来
    Channel _channel; 

    // 定义一个回调函数类型，用于处理新连接。参数为新连接的文件描述符
    using AcceptCallback = std::function<void(int)>;
    // 存储处理新连接的回调函数
    AcceptCallback _accept_callback;

private:
    /*监听套接字的读事件回调处理函数---获取新连接，调用_accept_callback函数进行新连接处理*/
    void HandleRead()
    {
        // 调用 _socket 的 Accept 方法获取新连接的文件描述符
        int newfd = _socket.Accept();
        // 如果获取新连接失败，直接返回
        if (newfd < 0)
        {
            return;
        }
        // 如果设置了处理新连接的回调函数，则调用该回调函数处理新连接
        if (_accept_callback)
            _accept_callback(newfd);
    }

    // 创建服务器监听套接字的函数，接受端口号作为参数
    int CreateServer(int port)
    {
        // 调用 _socket 的 CreateServer 方法创建服务器监听套接字
        bool ret = _socket.CreateServer(port);
        // 断言创建服务器套接字成功，如果失败则程序终止
        assert(ret == true);
        // 返回监听套接字的文件描述符
        return _socket.Fd();
    }

public:
    /*不能将启动读事件监控，放到构造函数中，必须在设置回调函数后，再去启动*/
    /*否则有可能造成启动监控后，立即有事件，处理的时候，回调函数还没设置：新连接得不到处理，且资源泄漏*/
    // 构造函数，接受 EventLoop 指针和端口号作为参数
    Acceptor(EventLoop *loop, int port) : 
        // 调用 CreateServer 方法创建监听套接字
        _socket(CreateServer(port)), 
        // 保存传入的 EventLoop 指针
        _loop(loop), 
        // 创建一个 Channel 对象，用于管理监听套接字的事件
        _channel(loop, _socket.Fd())
    {
        // 为 Channel 对象设置读事件的回调函数，当监听套接字有读事件发生时，调用 HandleRead 方法
        _channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
    }

    // 设置处理新连接的回调函数
    void SetAcceptCallback(const AcceptCallback &cb) { _accept_callback = cb; }

    // 启动监听套接字的读事件监控
    void Listen() { _channel.EnableRead(); }
};