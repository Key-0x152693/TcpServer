#include"../Log.hpp"
#include<signal.h>

using namespace log_ns;

// 定义一个名为 NetWork 的类，用于处理网络相关的初始化操作
class NetWork
{
public:
    // 类的构造函数，在创建 NetWork 对象时自动调用
    NetWork()
    {
        LOG(DEBUG,"SIGPIPE INIT\n");
        // 调用 signal 函数设置信号处理方式
        // SIGPIPE 是一个信号，表示向一个已经关闭的套接字写数据时产生的信号
        // SIG_IGN 是一个常量，表示忽略该信号
        // 这样设置后，当程序向一个已经关闭的套接字写数据时，不会触发默认的信号处理行为（通常是导致程序崩溃）
        signal(SIGPIPE, SIG_IGN);
    }
};

// 定义一个静态的 NetWork 对象 nw
// 静态对象在程序启动时就会被创建，并且只创建一次
// 由于构造函数中进行了 SIGPIPE 信号的初始化操作，所以程序启动时就会完成该信号的忽略设置
static NetWork nw;