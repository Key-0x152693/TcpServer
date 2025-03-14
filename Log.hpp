#pragma once
#include "LockGuard.hpp"
#include <iostream> 
#include <unistd.h>
#include <sys/types.h>
#include <ctime>
#include <stdarg.h>
#include <fstream>
#include <cstring>
#include <pthread.h>

namespace log_ns
{

    // 日志级别枚举，定义了5种日志级别
    enum
    {
        DEBUG = 1, // 调试信息
        INFO,      // 一般信息
        WARNING,   // 警告信息
        ERROR,     // 错误信息
        FATAL      // 严重错误信息
    };

    // 将日志级别枚举值转换为字符串
    std::string LevelToString(int level) // NOLINT
    {
        switch (level)
        {
        case DEBUG:
            return "DEBUG";
        case INFO:
            return "INFO";
        case WARNING:
            return "WARNING";
        case ERROR:
            return "ERROR";
        case FATAL:
            return "FATAL";
        default:
            return "UNKNOWN"; // 未知日志级别
        }
    }

    // 获取当前时间的字符串表示
    std::string GetCurrTime() // NOLINT
    {
        time_t now = time(nullptr);
        struct tm *curr_time = localtime(&now);
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%d-%02d-%02d %02d:%02d:%02d",
                 curr_time->tm_year + 1900, // 年份
                 curr_time->tm_mon + 1,     // 月份
                 curr_time->tm_mday,        // 日
                 curr_time->tm_hour,        // 小时
                 curr_time->tm_min,         // 分钟
                 curr_time->tm_sec);        // 秒
        return buffer;
    }

    // 日志消息类，用于存储日志的各个部分
    class LogMessage
    {
    public:
        std::string _level;        // 日志级别
        pid_t _id;                 // 进程ID
        std::string _filename;     // 文件名
        int _filenumber;           // 文件行号
        std::string _curr_time;    // 当前时间
        std::string _message_info; // 日志信息
    };

// 日志输出类型：屏幕或文件
#define SCREEN_TYPE 1
#define FILE_TYPE 2

    const std::string glogfile = "./log.txt";          // 默认日志文件路径
    pthread_mutex_t glock = PTHREAD_MUTEX_INITIALIZER; // NOLINT  // 全局锁，用于线程安全

    // 日志类，负责日志的输出
    class Log
    {
    public:
        // 构造函数，默认日志输出到屏幕
        Log(const std::string &logfile = glogfile) : _logfile(logfile), _type(SCREEN_TYPE)
        {
        }

        // 设置日志输出类型（屏幕或文件）
        void Enable(int type)
        {
            _type = type;
        }

        // 将日志输出到屏幕
        void FlushLogToScreen(const LogMessage &lg)
        {
            printf("[%s][%d][%s][%d][%s] %s",
                   lg._level.c_str(),         // 日志级别
                   lg._id,                    // 进程ID
                   lg._filename.c_str(),      // 文件名
                   lg._filenumber,            // 文件行号
                   lg._curr_time.c_str(),     // 当前时间
                   lg._message_info.c_str()); // 日志信息
        }

        // 将日志输出到文件
        void FlushLogToFile(const LogMessage &lg)
        {
            std::ofstream out(_logfile, std::ios::app); // 以追加模式打开日志文件
            if (!out.is_open())
                return;

            char logtxt[2048];
            snprintf(logtxt, sizeof(logtxt), "[%s][%d][%s][%d][%s] %s",
                     lg._level.c_str(),         // 日志级别
                     lg._id,                    // 进程ID
                     lg._filename.c_str(),      // 文件名
                     lg._filenumber,            // 文件行号
                     lg._curr_time.c_str(),     // 当前时间
                     lg._message_info.c_str()); // 日志信息
            out.write(logtxt, strlen(logtxt));  // 写入日志信息
            out.close();                        // 关闭文件
        }

        // 根据日志输出类型，将日志输出到屏幕或文件
        void FlushLog(const LogMessage &lg)
        {
            // 以后可以加过滤--TODO

            LockGuard lockguard(&glock); // 加锁，确保线程安全
            switch (_type)
            {
            case SCREEN_TYPE:
                FlushLogToScreen(lg); // 输出到屏幕
                break;
            case FILE_TYPE:
                FlushLogToFile(lg); // 输出到文件
                break;
            default:
                break;
            }
        }

        // 记录日志消息
        void logMessage(std::string filename, int filenumber, int level, const char *format, ...)
        {
            LogMessage lg;
            lg._level = LevelToString(level); // 设置日志级别
            lg._id = getpid();                // 获取当前进程ID
            lg._filename = filename;          // 设置文件名
            lg._filenumber = filenumber;      // 设置文件行号
            lg._curr_time = GetCurrTime();    // 获取当前时间

            va_list ap;
            va_start(ap, format);
            char log_info[1024];
            vsnprintf(log_info, sizeof(log_info), format, ap); // 格式化日志信息
            va_end(ap);
            lg._message_info = log_info; // 设置日志信息

            // 将日志打印出来
            FlushLog(lg);
        }

        ~Log()
        {
        }

    private:
        int _type;            // 日志输出类型（屏幕或文件）
        std::string _logfile; // 日志文件路径
    };

    Log lg; // NOLINT  // 全局日志对象

// 宏定义，简化日志记录调用
#define LOG(Level, Format, ...)                                          \
    do                                                                   \
    {                                                                    \
        lg.logMessage(__FILE__, __LINE__, Level, Format, ##__VA_ARGS__); \
    } while (0)
#define EnableScreen()          \
    do                          \
    {                           \
        lg.Enable(SCREEN_TYPE); \
    } while (0) // 启用屏幕输出
#define EnableFile()          \
    do                        \
    {                         \
        lg.Enable(FILE_TYPE); \
    } while (0) // 启用文件输出

};