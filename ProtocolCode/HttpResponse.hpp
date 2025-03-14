#pragma once
#include"statuANDmime.hpp"

// 该类用于表示一个 HTTP 响应，封装了响应的各个部分，如状态码、头部、正文和重定向信息等
class HttpResponse
{
public:
    int _statu;  // 存储 HTTP 响应的状态码，例如 200 表示成功，404 表示未找到资源等
    bool _redirect_flag;  // 表示该响应是否为重定向响应，若为 true 则表示需要重定向
    std::string _body;  // 存储 HTTP 响应的正文内容，例如 HTML 页面、JSON 数据等
    std::string _redirect_url;  // 若为重定向响应，该字段存储重定向的目标 URL
    std::unordered_map<std::string, std::string> _headers;  // 存储 HTTP 响应的头部字段，键为头部字段名，值为对应的值

public:
    // 默认构造函数，初始化重定向标志为 false，状态码为 200（OK）
    HttpResponse() : _redirect_flag(false), _statu(200) {}

    // 带参数的构造函数，允许用户指定响应的状态码，重定向标志初始化为 false
    HttpResponse(int statu) : _redirect_flag(false), _statu(statu) {}

    // 重置响应对象的所有成员变量，将其恢复到初始状态
    void ReSet()
    {
        _statu = 200;  // 恢复状态码为 200
        _redirect_flag = false;  // 重置重定向标志为 false
        _body.clear();  // 清空响应正文
        _redirect_url.clear();  // 清空重定向 URL
        _headers.clear();  // 清空头部字段
    }

    // 插入一个头部字段到 _headers 中
    void SetHeader(const std::string &key, const std::string &val)
    {
        _headers.insert(std::make_pair(key, val));  // 使用 insert 方法将键值对插入到 _headers 中
    }

    // 判断响应头部中是否存在指定的头部字段
    bool HasHeader(const std::string &key)
    {
        auto it = _headers.find(key);  // 在 _headers 中查找指定的键
        if (it == _headers.end())
        {
            return false;  // 如果未找到，返回 false
        }
        return true;  // 找到则返回 true
    }

    // 获取指定头部字段的值
    std::string GetHeader(const std::string &key)
    {
        auto it = _headers.find(key);  // 在 _headers 中查找指定的键
        if (it == _headers.end())
        {
            return "";  // 如果未找到，返回空字符串
        }
        return it->second;  // 找到则返回对应的值
    }

    // 设置响应的正文内容，并指定内容类型，默认为 text/html
    void SetContent(const std::string &body, const std::string &type = "text/html")
    {
        _body = body;  // 设置响应正文
        SetHeader("Content-Type", type);  // 设置 Content-Type 头部字段
    }

    // 设置重定向信息，指定重定向的目标 URL 和状态码，默认状态码为 302（临时重定向）
    void SetRedirect(const std::string &url, int statu = 302)
    {
        _statu = statu;  // 设置响应状态码
        _redirect_flag = true;  // 设置重定向标志为 true
        _redirect_url = url;  // 设置重定向的目标 URL
    }

    // 判断该 HTTP 响应是否是短链接
    bool Close()
    {
        // 没有 Connection 字段，或者有 Connection 但是值是 close，则都是短链接，否则就是长连接
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;  // 如果存在 Connection 字段且值为 keep-alive，返回 false，表示是长连接
        }
        return true;  // 否则返回 true，表示是短链接
    }
};