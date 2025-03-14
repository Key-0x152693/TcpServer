#include"Buffer.hpp"
#include "Socket.hpp"
#include"EventLoop.hpp"
#include"Any.hpp"

// 前向声明Connection类
class Connection;

// 定义连接状态的枚举类型
// DISCONNECTED -- 连接关闭状态
// CONNECTING -- 连接建立成功但处于待处理状态
// CONNECTED -- 连接建立完成，各项设置就绪，可以进行通信
// DISCONNECTING -- 连接处于待关闭状态
typedef enum
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
} ConnStatu;

// 定义一个智能指针类型，用于管理Connection对象
using PtrConnection = std::shared_ptr<Connection>;

// 定义Connection类，继承自std::enable_shared_from_this，方便获取自身的智能指针
class Connection : public std::enable_shared_from_this<Connection>
{
private:
    // 连接的唯一ID，用于连接的管理和查找
    uint64_t _conn_id; 
    // 注释掉的定时器ID，为简化操作，使用_conn_id作为定时器ID
    // uint64_t _timer_id;   
    // 连接关联的文件描述符
    int _sockfd;                   
    // 连接是否启动非活跃销毁的判断标志，默认为false
    bool _enable_inactive_release; 
    // 连接所关联的一个EventLoop对象
    EventLoop *_loop;              
    // 连接的当前状态
    ConnStatu _statu;              
    // 套接字操作管理对象
    Socket _socket;                
    // 连接的事件管理对象
    Channel _channel;              
    // 输入缓冲区，用于存放从socket中读取到的数据
    Buffer _in_buffer;             
    // 输出缓冲区，用于存放要发送给对端的数据
    Buffer _out_buffer;            
    // 请求的接收处理上下文，可存储任意类型的数据
    Any _context;                  

    // 定义四个回调函数类型，这些回调函数由服务器模块设置，最终由组件使用者使用
    // 连接建立成功时调用的回调函数
    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    // 接收到消息时调用的回调函数
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    // 连接关闭时调用的回调函数
    using ClosedCallback = std::function<void(const PtrConnection &)>;
    // 发生任意事件时调用的回调函数
    using AnyEventCallback = std::function<void(const PtrConnection &)>;

    // 连接建立成功时调用的回调函数对象
    ConnectedCallback _connected_callback;
    // 接收到消息时调用的回调函数对象
    MessageCallback _message_callback;
    // 连接关闭时调用的回调函数对象
    ClosedCallback _closed_callback;
    // 发生任意事件时调用的回调函数对象
    AnyEventCallback _event_callback;

    // 组件内的连接关闭回调，由组件内部设置，用于在连接关闭时从服务器管理中移除该连接信息
    ClosedCallback _server_closed_callback;

private:
    // 描述符可读事件触发后调用的函数，接收socket数据放到接收缓冲区中，然后调用_message_callback
    void HandleRead()
    {
        // 1. 接收socket的数据，放到缓冲区
        // 定义一个缓冲区，用于临时存储从socket读取的数据
        char buf[65536];
        // 以非阻塞方式从socket读取数据
        ssize_t ret = _socket.NonBlockRecv(buf, 65535);
        if (ret < 0)
        {
            // 出错了，不能直接关闭连接，调用ShutdownInLoop函数进行处理
            return ShutdownInLoop();
        }
        // 这里的等于0表示的是没有读取到数据，而并不是连接断开了，连接断开返回的是-1
        // 将数据放入输入缓冲区，写入之后顺便将写偏移向后移动
        _in_buffer.WriteAndPush(buf, ret);
        // 2. 调用message_callback进行业务处理
        if (_in_buffer.ReadAbleSize() > 0)
        {
            // shared_from_this -- 从当前对象自身获取自身的shared_ptr管理对象
            // 调用_message_callback进行业务处理
            return _message_callback(shared_from_this(), &_in_buffer);
        }
    }

    // 描述符可写事件触发后调用的函数，将发送缓冲区中的数据进行发送
    void HandleWrite()
    {
        // _out_buffer中保存的数据就是要发送的数据
        // 以非阻塞方式将输出缓冲区的数据发送出去
        ssize_t ret = _socket.NonBlockSend(_out_buffer.ReadPosition(), _out_buffer.ReadAbleSize());
        if (ret < 0)
        {
            // 发送错误就该关闭连接了
            if (_in_buffer.ReadAbleSize() > 0)
            {
                // 若输入缓冲区还有数据，调用_message_callback进行处理
                _message_callback(shared_from_this(), &_in_buffer);
            }
            // 调用Release函数进行实际的关闭释放操作
            return Release(); 
        }
        // 千万不要忘了，将读偏移向后移动
        _out_buffer.MoveReadOffset(ret); 
        if (_out_buffer.ReadAbleSize() == 0)
        {
            // 没有数据待发送了，关闭写事件监控
            _channel.DisableWrite(); 
            // 如果当前是连接待关闭状态，则有数据，发送完数据释放连接，没有数据则直接释放
            if (_statu == DISCONNECTING)
            {
                return Release();
            }
        }
        return;
    }

    // 描述符触发挂断事件
    void HandleClose()
    {
        // 一旦连接挂断了，套接字就什么都干不了了，因此有数据待处理就处理一下，完毕关闭连接
        if (_in_buffer.ReadAbleSize() > 0)
        {
            // 若输入缓冲区还有数据，调用_message_callback进行处理
            _message_callback(shared_from_this(), &_in_buffer);
        }
        // 调用Release函数进行实际的关闭释放操作
        return Release();
    }

    // 描述符触发出错事件
    void HandleError()
    {
        // 出错事件处理与挂断事件处理相同，调用HandleClose函数
        return HandleClose();
    }

    // 描述符触发任意事件: 1. 刷新连接的活跃度 -- 延迟定时销毁任务；  2. 调用组件使用者的任意事件回调
    void HandleEvent()
    {
        if (_enable_inactive_release == true)
        {
            // 若启用了非活跃销毁，刷新连接的定时器
            _loop->TimerRefresh(_conn_id);
        }
        if (_event_callback)
        {
            // 若设置了任意事件回调函数，调用该函数
            _event_callback(shared_from_this());
        }
    }

    // 连接获取之后，所处的状态下要进行各种设置（启动读监控，调用回调函数）
    void EstablishedInLoop()
    {
        // 1. 修改连接状态；  2. 启动读事件监控；  3. 调用回调函数
        // 当前的状态必须一定是上层的半连接状态
        assert(_statu == CONNECTING); 
        // 当前函数执行完毕，则连接进入已完成连接状态
        _statu = CONNECTED;           
        // 一旦启动读事件监控就有可能会立即触发读事件，如果这时候启动了非活跃连接销毁
        _channel.EnableRead();
        if (_connected_callback)
        {
            // 若设置了连接建立成功回调函数，调用该函数
            _connected_callback(shared_from_this());
        }
    }

    // 这个接口才是实际的释放接口
    void ReleaseInLoop()
    {
        // 1. 修改连接状态，将其置为DISCONNECTED
        _statu = DISCONNECTED;
        // 2. 移除连接的事件监控
        _channel.Remove();
        // 3. 关闭描述符
        _socket.Close();
        // 4. 如果当前定时器队列中还有定时销毁任务，则取消任务
        if (_loop->HasTimer(_conn_id))
        {
            // 取消非活跃销毁任务
            CancelInactiveReleaseInLoop();
        }
        // 5. 调用关闭回调函数，避免先移除服务器管理的连接信息导致Connection被释放，再去处理会出错，因此先调用用户的回调函数
        if (_closed_callback)
        {
            // 调用连接关闭回调函数
            _closed_callback(shared_from_this());
        }
        // 移除服务器内部管理的连接信息
        if (_server_closed_callback)
        {
            // 调用服务器内部的连接关闭回调函数
            _server_closed_callback(shared_from_this());
        }
    }

    // 这个接口并不是实际的发送接口，而只是把数据放到了发送缓冲区，启动了可写事件监控
    void SendInLoop(Buffer &buf)
    {
        if (_statu == DISCONNECTED)
        {
            // 若连接已关闭，直接返回
            return;
        }
        // 将数据写入输出缓冲区
        _out_buffer.WriteAndPush(buf);
        if (_channel.WriteAble() == false)
        {
            // 若写事件未启用，启用写事件监控
            _channel.EnableWrite();
        }
    }

    // 这个关闭操作并非实际的连接释放操作，需要判断还有没有数据待处理，待发送
    void ShutdownInLoop()
    {
        // 设置连接为半关闭状态
        _statu = DISCONNECTING; 
        if (_in_buffer.ReadAbleSize() > 0)
        {
            if (_message_callback)
            {
                // 若输入缓冲区还有数据，调用_message_callback进行处理
                _message_callback(shared_from_this(), &_in_buffer);
            }
        }
        // 要么就是写入数据的时候出错关闭，要么就是没有待发送数据，直接关闭
        if (_out_buffer.ReadAbleSize() > 0)
        {
            if (_channel.WriteAble() == false)
            {
                // 若写事件未启用，启用写事件监控
                _channel.EnableWrite();
            }
        }
        if (_out_buffer.ReadAbleSize() == 0)
        {
            // 若输出缓冲区没有数据，调用Release函数进行释放
            Release();
        }
    }

    // 启动非活跃连接超时释放规则
    void EnableInactiveReleaseInLoop(int sec)
    {
        // 1. 将判断标志 _enable_inactive_release 置为true
        _enable_inactive_release = true;
        // 2. 如果当前定时销毁任务已经存在，那就刷新延迟一下即可
        if (_loop->HasTimer(_conn_id))
        {
            // 刷新连接的定时器
            return _loop->TimerRefresh(_conn_id);
        }
        // 3. 如果不存在定时销毁任务，则新增
        _loop->TimerAdd(_conn_id, sec, std::bind(&Connection::Release, this));
    }

    // 取消非活跃销毁
    void CancelInactiveReleaseInLoop()
    {
        // 禁用非活跃销毁
        _enable_inactive_release = false;
        if (_loop->HasTimer(_conn_id))
        {
            // 取消连接的定时器
            _loop->TimerCancel(_conn_id);
        }
    }

    // 切换协议 --- 重置上下文以及阶段性回调处理函数 -- 而是这个接口必须在EventLoop线程中立即执行
    // 防备新的事件触发后，处理的时候，切换任务还没有被执行 -- 会导致数据使用原协议处理了。
    void UpgradeInLoop(const Any &context,
                       const ConnectedCallback &conn,
                       const MessageCallback &msg,
                       const ClosedCallback &closed,
                       const AnyEventCallback &event)
    {
        // 更新上下文
        _context = context;
        // 更新连接建立成功回调函数
        _connected_callback = conn;
        // 更新消息处理回调函数
        _message_callback = msg;
        // 更新连接关闭回调函数
        _closed_callback = closed;
        // 更新任意事件回调函数
        _event_callback = event;
    }

public:
    // 构造函数，初始化连接对象
    Connection(EventLoop *loop, uint64_t conn_id, int sockfd) : _conn_id(conn_id), _sockfd(sockfd),
                                                                _enable_inactive_release(false), _loop(loop), _statu(CONNECTING), _socket(_sockfd),
                                                                _channel(loop, _sockfd)
    {
        // 设置关闭事件回调函数
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
        // 设置任意事件回调函数
        _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
        // 设置可读事件回调函数
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        // 设置可写事件回调函数
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        // 设置出错事件回调函数
        _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
    }

    // 析构函数，打印连接释放信息
    ~Connection() { LOG(DEBUG,"RELEASE CONNECTION:%p\n", this); }

    // 获取管理的文件描述符
    int Fd() { return _sockfd; }

    // 获取连接ID
    int Id() { return _conn_id; }

    // 判断连接是否处于CONNECTED状态
    bool Connected() { return (_statu == CONNECTED); }

    // 设置上下文 -- 连接建立完成时进行调用
    void SetContext(const Any &context) { _context = context; }

    // 获取上下文，返回的是指针
    Any *GetContext() { return &_context; }

    // 设置连接建立成功回调函数
    void SetConnectedCallback(const ConnectedCallback &cb) { _connected_callback = cb; }

    // 设置消息处理回调函数
    void SetMessageCallback(const MessageCallback &cb) { _message_callback = cb; }

    // 设置连接关闭回调函数
    void SetClosedCallback(const ClosedCallback &cb) { _closed_callback = cb; }

    // 设置任意事件回调函数
    void SetAnyEventCallback(const AnyEventCallback &cb) { _event_callback = cb; }

    // 设置服务器内部的连接关闭回调函数
    void SetSrvClosedCallback(const ClosedCallback &cb) { _server_closed_callback = cb; }

    // 连接建立就绪后，进行channel回调设置，启动读监控，调用_connected_callback
    void Established()
    {
        // 将EstablishedInLoop函数放入EventLoop的任务队列中执行
        _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
    }

    // 发送数据，将数据放到发送缓冲区，启动写事件监控
    void Send(const char *data, size_t len)
    {
        // 外界传入的data，可能是个临时的空间，我们现在只是把发送操作压入了任务池，有可能并没有被立即执行
        // 因此有可能执行的时候，data指向的空间有可能已经被释放了。
        // 创建一个临时缓冲区
        Buffer buf;
        // 将数据写入临时缓冲区
        buf.WriteAndPush(data, len);
        // 将SendInLoop函数放入EventLoop的任务队列中执行
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, std::move(buf)));
    }

    // 提供给组件使用者的关闭接口 -- 并不实际关闭，需要判断有没有数据待处理
    void Shutdown()
    {
        // 将ShutdownInLoop函数放入EventLoop的任务队列中执行
        _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
    }

    // 实际的释放连接操作
    void Release()
    {
        // 将ReleaseInLoop函数放入EventLoop的任务队列中执行
        _loop->QueueInLoop(std::bind(&Connection::ReleaseInLoop, this));
    }

    // 启动非活跃销毁，并定义多长时间无通信就是非活跃，添加定时任务
    void EnableInactiveRelease(int sec)
    {
        // 将EnableInactiveReleaseInLoop函数放入EventLoop的任务队列中执行
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
    }

    // 取消非活跃销毁
    void CancelInactiveRelease()
    {
        // 将CancelInactiveReleaseInLoop函数放入EventLoop的任务队列中执行
        _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
    }

    // 切换协议 --- 重置上下文以及阶段性回调处理函数 -- 而是这个接口必须在EventLoop线程中立即执行
    // 防备新的事件触发后，处理的时候，切换任务还没有被执行 -- 会导致数据使用原协议处理了。
    void Upgrade(const Any &context, const ConnectedCallback &conn, const MessageCallback &msg,
                 const ClosedCallback &closed, const AnyEventCallback &event)
    {
        // 确保在EventLoop线程中执行
        _loop->AssertInLoop();
        // 将UpgradeInLoop函数放入EventLoop的任务队列中执行
        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, conn, msg, closed, event));
    }
};