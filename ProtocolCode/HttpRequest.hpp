#pragma once
#include"statuANDmime.hpp"
#include <regex>

// 该类用于表示一个 HTTP 请求，封装了请求的各个部分，如方法、路径、版本、头部、参数和正文等
class HttpRequest
{
public:
    std::string _method;                                   // 存储 HTTP 请求的方法，如 GET、POST、PUT 等
    std::string _path;                                     // 存储请求的资源路径，例如 /index.html
    std::string _version;                                  // 存储 HTTP 协议的版本，如 HTTP/1.1
    std::string _body;                                     // 存储 HTTP 请求的正文内容
    std::smatch _matches;                                  // 用于存储资源路径的正则提取数据，方便后续对路径进行解析和处理
    std::unordered_map<std::string, std::string> _headers; // 存储 HTTP 请求的头部字段，键为头部字段名，值为对应的值
    std::unordered_map<std::string, std::string> _params;  // 存储 HTTP 请求的查询字符串，键为参数名，值为对应的值

public:
    // 构造函数，初始化协议版本为 HTTP/1.1
    HttpRequest() : _version("HTTP/1.1") {}

    // 重置请求对象的所有成员变量，将其恢复到初始状态
    void ReSet()
    {
        _method.clear();  // 清空请求方法
        _path.clear();    // 清空资源路径
        _version = "HTTP/1.1";  // 恢复协议版本为 HTTP/1.1
        _body.clear();    // 清空请求正文
        std::smatch match;
        _matches.swap(match);  // 清空正则匹配结果
        _headers.clear();  // 清空头部字段
        _params.clear();   // 清空查询字符串
    }

    // 插入一个头部字段到 _headers 中
    void SetHeader(const std::string &key, const std::string &val)
    {
        _headers.insert(std::make_pair(key, val));  // 使用 insert 方法将键值对插入到 _headers 中
    }

    // 判断请求头部中是否存在指定的头部字段
    bool HasHeader(const std::string &key) const
    {
        auto it = _headers.find(key);  // 在 _headers 中查找指定的键
        if (it == _headers.end())
        {
            return false;  // 如果未找到，返回 false
        }
        return true;  // 找到则返回 true
    }

    // 获取指定头部字段的值
    std::string GetHeader(const std::string &key) const
    {
        auto it = _headers.find(key);  // 在 _headers 中查找指定的键
        if (it == _headers.end())
        {
            return "";  // 如果未找到，返回空字符串
        }
        return it->second;  // 找到则返回对应的值
    }

    // 插入一个查询字符串到 _params 中
    void SetParam(const std::string &key, const std::string &val)
    {
        _params.insert(std::make_pair(key, val));  // 使用 insert 方法将键值对插入到 _params 中
    }

    // 判断请求的查询字符串中是否存在指定的参数
    bool HasParam(const std::string &key) const
    {
        auto it = _params.find(key);  // 在 _params 中查找指定的键
        if (it == _params.end())
        {
            return false;  // 如果未找到，返回 false
        }
        return true;  // 找到则返回 true
    }

    // 获取指定查询字符串的值
    std::string GetParam(const std::string &key) const
    {
        auto it = _params.find(key);  // 在 _params 中查找指定的键
        if (it == _params.end())
        {
            return "";  // 如果未找到，返回空字符串
        }
        return it->second;  // 找到则返回对应的值
    }

    // 获取 HTTP 请求正文的长度
    size_t ContentLength() const
    {
        // Content-Length: 1234\r\n
        bool ret = HasHeader("Content-Length");  // 检查头部中是否存在 Content-Length 字段
        if (ret == false)
        {
            return 0;  // 如果不存在，返回 0
        }
        std::string clen = GetHeader("Content-Length");  // 获取 Content-Length 字段的值
        return std::stol(clen);  // 将值转换为长整型并返回
    }

    // 判断该 HTTP 请求是否是短链接
    bool Close() const
    {
        // 没有 Connection 字段，或者有 Connection 但是值是 close，则都是短链接，否则就是长连接
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;  // 如果存在 Connection 字段且值为 keep-alive，返回 false，表示是长连接
        }
        return true;  // 否则返回 true，表示是短链接
    }
};