#include"statuANDmime.hpp"
#include"Util.hpp"
#include"HttpRequest.hpp"

// 定义一个枚举类型，表示 HTTP 请求接收和解析的不同阶段状态
typedef enum
{
    RECV_HTTP_ERROR,  // 接收或解析过程中出现错误
    RECV_HTTP_LINE,   // 正在接收和解析 HTTP 请求的首行
    RECV_HTTP_HEAD,   // 正在接收和解析 HTTP 请求的头部
    RECV_HTTP_BODY,   // 正在接收和解析 HTTP 请求的正文
    RECV_HTTP_OVER    // HTTP 请求接收和解析完成
} HttpRecvStatu;

// 定义最大行长度，用于处理超长的 HTTP 请求行或头部行
#define MAX_LINE 8192

// HttpContext 类用于接收和解析 HTTP 请求
class HttpContext
{
private:
    int _resp_statu;           // 存储 HTTP 响应的状态码，用于在解析过程中出现错误时设置响应状态
    HttpRecvStatu _recv_statu; // 当前 HTTP 请求接收和解析所处的阶段状态
    HttpRequest _request;      // 存储已经解析得到的 HTTP 请求信息

private:
    // 解析 HTTP 请求的首行
    bool ParseHttpLine(const std::string &line)
    {
        std::smatch matches;
        // 定义一个正则表达式，用于匹配 HTTP 请求首行的格式
        std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        // 使用正则表达式匹配输入的首行
        bool ret = std::regex_match(line, matches, e);
        if (ret == false)
        {
            // 匹配失败，说明首行格式不正确，设置状态为错误，响应状态码为 400（错误请求）
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 400; // BAD REQUEST
            return false;
        }
        // 示例匹配结果：
        // 0 : GET /ahut/login?user=yhrg&pass=123123 HTTP/1.1
        // 1 : GET
        // 2 : /ahut/login
        // 3 : user=yhr&pass=123123
        // 4 : HTTP/1.1
        
        // 获取请求方法，并将其转换为大写
        _request._method = matches[1];
        std::transform(_request._method.begin(), _request._method.end(), _request._method.begin(), ::toupper);
        // 获取资源路径，并进行 URL 解码，不将 + 转换为空格
        _request._path = Util::UrlDecode(matches[2], false);
        // 获取协议版本
        _request._version = matches[4];
        // 获取查询字符串
        std::vector<std::string> query_string_arry;
        std::string query_string = matches[3];
        // 以 & 符号分割查询字符串，得到各个键值对
        Util::Split(query_string, "&", &query_string_arry);
        // 遍历每个键值对，以 = 符号分割，得到键和值，并进行 URL 解码
        for (auto &str : query_string_arry)
        {
            size_t pos = str.find("=");
            if (pos == std::string::npos)
            {
                // 键值对格式不正确，设置状态为错误，响应状态码为 400（错误请求）
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 400; // BAD REQUEST
                return false;
            }
            std::string key = Util::UrlDecode(str.substr(0, pos), true);
            std::string val = Util::UrlDecode(str.substr(pos + 1), true);
            // 将解码后的键值对添加到请求的参数中
            _request.SetParam(key, val);
        }
        return true;
    }

    // 接收并解析 HTTP 请求的首行
    bool RecvHttpLine(Buffer *buf)
    {
        if (_recv_statu != RECV_HTTP_LINE)
            return false;
        // 从缓冲区中获取一行数据
        std::string line = buf->GetLineAndPop();
        // 处理缓冲区数据不足一行或数据超长的情况
        if (line.size() == 0)
        {
            // 缓冲区数据不足一行，检查可读数据长度
            if (buf->ReadAbleSize() > MAX_LINE)
            {
                // 可读数据过长，设置状态为错误，响应状态码为 414（URI 过长）
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414; // URI TOO LONG
                return false;
            }
            // 数据不足但不多，等待新数据
            return true;
        }
        if (line.size() > MAX_LINE)
        {
            // 数据超长，设置状态为错误，响应状态码为 414（URI 过长）
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 414; // URI TOO LONG
            return false;
        }
        // 调用 ParseHttpLine 函数解析首行
        bool ret = ParseHttpLine(line);
        if (ret == false)
        {
            return false;
        }
        // 首行解析完毕，进入头部获取阶段
        _recv_statu = RECV_HTTP_HEAD;
        return true;
    }

    // 接收并解析 HTTP 请求的头部
    bool RecvHttpHead(Buffer *buf)
    {
        if (_recv_statu != RECV_HTTP_HEAD)
            return false;
        // 逐行读取头部数据，直到遇到空行
        while (1)
        {
            std::string line = buf->GetLineAndPop();
            // 处理缓冲区数据不足一行或数据超长的情况
            if (line.size() == 0)
            {
                // 缓冲区数据不足一行，检查可读数据长度
                if (buf->ReadAbleSize() > MAX_LINE)
                {
                    // 可读数据过长，设置状态为错误，响应状态码为 414（URI 过长）
                    _recv_statu = RECV_HTTP_ERROR;
                    _resp_statu = 414; // URI TOO LONG
                    return false;
                }
                // 数据不足但不多，等待新数据
                return true;
            }
            if (line.size() > MAX_LINE)
            {
                // 数据超长，设置状态为错误，响应状态码为 414（URI 过长）
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414; // URI TOO LONG
                return false;
            }
            if (line == "\n" || line == "\r\n")
            {
                // 遇到空行，头部解析完毕
                break;
            }
            // 调用 ParseHttpHead 函数解析当前行
            bool ret = ParseHttpHead(line);
            if (ret == false)
            {
                return false;
            }
        }
        // 头部解析完毕，进入正文获取阶段
        _recv_statu = RECV_HTTP_BODY;
        return true;
    }

    // 解析 HTTP 请求的头部行
    bool ParseHttpHead(std::string &line)
    {
        // 去除行末尾的换行符和回车符
        if (line.back() == '\n')
            line.pop_back();
        if (line.back() == '\r')
            line.pop_back();
        // 查找 : 分隔符
        size_t pos = line.find(": ");
        if (pos == std::string::npos)
        {
            // 格式不正确，设置状态为错误，响应状态码为 400（错误请求）
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 400; 
            return false;
        }
        // 提取头部字段的键和值
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 2);
        // 将键值对添加到请求的头部信息中
        _request.SetHeader(key, val);
        return true;
    }

    // 接收并解析 HTTP 请求的正文
    bool RecvHttpBody(Buffer *buf)
    {
        if (_recv_statu != RECV_HTTP_BODY)
            return false;
        // 获取正文长度
        size_t content_length = _request.ContentLength();
        if (content_length == 0)
        {
            // 没有正文，请求接收解析完毕
            _recv_statu = RECV_HTTP_OVER;
            return true;
        }
        // 计算还需要接收的正文长度
        size_t real_len = content_length - _request._body.size();
        // 处理缓冲区数据是否足够的情况
        if (buf->ReadAbleSize() >= real_len)
        {
            // 缓冲区数据足够，取出所需数据
            _request._body.append(buf->ReadPosition(), real_len);
            buf->MoveReadOffset(real_len);
            // 请求接收解析完毕
            _recv_statu = RECV_HTTP_OVER;
            return true;
        }
        // 缓冲区数据不足，取出现有数据，等待新数据
        _request._body.append(buf->ReadPosition(), buf->ReadAbleSize());
        buf->MoveReadOffset(buf->ReadAbleSize());
        return true;
    }

public:
    // 构造函数，初始化响应状态码为 200，接收状态为接收首行
    HttpContext() : _resp_statu(200), _recv_statu(RECV_HTTP_LINE) {}

    // 重置 HttpContext 对象的状态
    void ReSet()
    {
        _resp_statu = 200;
        _recv_statu = RECV_HTTP_LINE;
        _request.ReSet();
    }

    // 获取响应状态码
    int RespStatu() { return _resp_statu; }

    // 获取当前接收状态
    HttpRecvStatu RecvStatu() { return _recv_statu; }

    // 获取解析后的 HTTP 请求对象
    HttpRequest &Request() { return _request; }

    // 接收并解析 HTTP 请求
    void RecvHttpRequest(Buffer *buf)
    {
        // 根据当前接收状态进行相应的处理，处理完一个阶段后继续处理下一个阶段
        switch (_recv_statu)
        {
        case RECV_HTTP_LINE:
            RecvHttpLine(buf);
        case RECV_HTTP_HEAD:
            RecvHttpHead(buf);
        case RECV_HTTP_BODY:
            RecvHttpBody(buf);
        }
        return;
    }
};