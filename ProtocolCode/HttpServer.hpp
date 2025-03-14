#include"Util.hpp"
#include"HttpRequest.hpp"
#include"HttpResponse.hpp"
#include"HttpContext.hpp"

// 定义HttpServer类，用于处理HTTP请求和响应
class HttpServer
{
private:
    // 定义Handler类型，它是一个函数对象，接受一个HttpRequest对象和一个HttpResponse对象的指针作为参数
    using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;
    // 定义Handlers类型，它是一个存储正则表达式和处理函数对的向量
    using Handlers = std::vector<std::pair<std::regex, Handler>>;
    // 存储GET请求的路由表，键为正则表达式，值为处理函数
    Handlers _get_route;
    // 存储POST请求的路由表，键为正则表达式，值为处理函数
    Handlers _post_route;
    // 存储PUT请求的路由表，键为正则表达式，值为处理函数
    Handlers _put_route;
    // 存储DELETE请求的路由表，键为正则表达式，值为处理函数
    Handlers _delete_route;
    // 静态资源的根目录，用于处理静态资源请求
    std::string _basedir; 
    // 底层的TCP服务器对象，用于处理网络连接
    TcpServer _server;

private:
    // 错误处理函数，当请求处理出错时调用
    void ErrorHandler(const HttpRequest &req, HttpResponse *rsp)
    {
        // 1. 组织一个错误展示页面
        std::string body;
        // 构建HTML页面的开头
        body += "<html>";
        body += "<head>";
        // 设置页面的字符编码为UTF-8
        body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
        body += "</head>";
        body += "<body>";
        body += "<h1>";
        // 添加响应状态码
        body += std::to_string(rsp->_statu);
        body += " ";
        // 添加状态码的描述信息
        body += Util::StatuDesc(rsp->_statu);
        body += "</h1>";
        body += "</body>";
        body += "</html>";
        // 2. 将页面数据，当作响应正文，放入rsp中
        // 设置响应的内容和内容类型
        rsp->SetContent(body, "text/html");
    }
    // 将HttpResponse中的要素按照http协议格式进行组织，发送
    void WriteReponse(const PtrConnection &conn, const HttpRequest &req, HttpResponse &rsp)
    {
        // 1. 先完善头部字段
        // 判断请求是否为短连接
        if (req.Close() == true)
        {
            // 短连接则设置Connection头部为close
            rsp.SetHeader("Connection", "close");
        }
        else
        {
            // 长连接则设置Connection头部为keep-alive
            rsp.SetHeader("Connection", "keep-alive");
        }
        // 如果响应正文不为空且没有设置Content-Length头部
        if (rsp._body.empty() == false && rsp.HasHeader("Content-Length") == false)
        {
            // 设置Content-Length头部为响应正文的长度
            rsp.SetHeader("Content-Length", std::to_string(rsp._body.size()));
        }
        // 如果响应正文不为空且没有设置Content-Type头部
        if (rsp._body.empty() == false && rsp.HasHeader("Content-Type") == false)
        {
            // 设置Content-Type头部为application/octet-stream
            rsp.SetHeader("Content-Type", "application/octet-stream");
        }
        // 如果响应需要重定向
        if (rsp._redirect_flag == true)
        {
            // 设置Location头部为重定向的URL
            rsp.SetHeader("Location", rsp._redirect_url);
        }
        // 2. 将rsp中的要素，按照http协议格式进行组织
        std::stringstream rsp_str;
        // 构建响应行，包含协议版本、状态码和状态描述
        rsp_str << req._version << " " << std::to_string(rsp._statu) << " " << Util::StatuDesc(rsp._statu) << "\r\n";
        // 遍历响应头部，将每个头部字段添加到响应字符串中
        for (auto &head : rsp._headers)
        {
            rsp_str << head.first << ": " << head.second << "\r\n";
        }
        // 头部和正文之间的空行
        rsp_str << "\r\n";
        // 添加响应正文
        rsp_str << rsp._body;
        // 3. 发送数据
        // 将组织好的响应字符串发送给客户端
        conn->Send(rsp_str.str().c_str(), rsp_str.str().size());
    }
    // 判断请求是否为静态资源请求
    bool IsFileHandler(const HttpRequest &req)
    {
        // 1. 必须设置了静态资源根目录
        if (_basedir.empty())
        {
            return false;
        }
        // 2. 请求方法，必须是GET / HEAD请求方法
        if (req._method != "GET" && req._method != "HEAD")
        {
            return false;
        }
        // 3. 请求的资源路径必须是一个合法路径
        if (Util::ValidPath(req._path) == false)
        {
            return false;
        }
        // 4. 请求的资源必须存在,且是一个普通文件
        //    有一种请求比较特殊 -- 目录：/, /image/， 这种情况给后边默认追加一个 index.html
        // index.html    /image/a.png
        // 不要忘了前缀的相对根目录,也就是将请求路径转换为实际存在的路径  /image/a.png  ->   ./wwwroot/image/a.png
        // 为了避免直接修改请求的资源路径，因此定义一个临时对象
        std::string req_path = _basedir + req._path; 
        // 如果请求路径以斜杠结尾
        if (req._path.back() == '/')
        {
            // 追加index.html
            req_path += "index.html";
        }
        // 判断请求的资源是否为普通文件
        if (Util::IsRegular(req_path) == false)
        {
            return false;
        }
        return true;
    }
    // 静态资源的请求处理 --- 将静态资源文件的数据读取出来，放到rsp的_body中, 并设置mime
    void FileHandler(const HttpRequest &req, HttpResponse *rsp)
    {
        // 拼接请求的实际路径
        std::string req_path = _basedir + req._path;
        // 如果请求路径以斜杠结尾
        if (req._path.back() == '/')
        {
            // 追加index.html
            req_path += "index.html";
        }
        // 读取文件内容到响应正文中
        bool ret = Util::ReadFile(req_path, &rsp->_body);
        if (ret == false)
        {
            return;
        }
        // 获取文件的MIME类型
        std::string mime = Util::ExtMime(req_path);
        // 设置响应的Content-Type头部为文件的MIME类型
        rsp->SetHeader("Content-Type", mime);
        return;
    }
    // 功能性请求的分类处理
    void Dispatcher(HttpRequest &req, HttpResponse *rsp, Handlers &handlers)
    {
        // 在对应请求方法的路由表中，查找是否含有对应资源请求的处理函数，有则调用，没有则返回404
        // 思想：路由表存储的时键值对 -- 正则表达式 & 处理函数
        // 使用正则表达式，对请求的资源路径进行正则匹配，匹配成功就使用对应函数进行处理
        //   /numbers/(\d+)       /numbers/12345
        // 遍历路由表
        for (auto &handler : handlers)
        {
            // 获取正则表达式
            const std::regex &re = handler.first;
            // 获取处理函数
            const Handler &functor = handler.second;
            // 使用正则表达式匹配请求路径
            bool ret = std::regex_match(req._path, req._matches, re);
            if (ret == false)
            {
                continue;
            }
            // 匹配成功则调用处理函数
            return functor(req, rsp); 
        }
        // 没有匹配到处理函数，设置响应状态码为404
        rsp->_statu = 404;
    }
    // 请求路由函数，根据请求类型和资源路径分发请求
    void Route(HttpRequest &req, HttpResponse *rsp)
    {
        // 1. 对请求进行分辨，是一个静态资源请求，还是一个功能性请求
        //    静态资源请求，则进行静态资源的处理
        //    功能性请求，则需要通过几个请求路由表来确定是否有处理函数
        //    既不是静态资源请求，也没有设置对应的功能性请求处理函数，就返回405
        // 判断是否为静态资源请求
        if (IsFileHandler(req) == true)
        {
            // 是一个静态资源请求, 则进行静态资源请求的处理
            return FileHandler(req, rsp);
        }
        // 如果是GET或HEAD请求
        if (req._method == "GET" || req._method == "HEAD")
        {
            // 调用GET请求的分发器
            return Dispatcher(req, rsp, _get_route);
        }
        // 如果是POST请求
        else if (req._method == "POST")
        {
            // 调用POST请求的分发器
            return Dispatcher(req, rsp, _post_route);
        }
        // 如果是PUT请求
        else if (req._method == "PUT")
        {
            // 调用PUT请求的分发器
            return Dispatcher(req, rsp, _put_route);
        }
        // 如果是DELETE请求
        else if (req._method == "DELETE")
        {
            // 调用DELETE请求的分发器
            return Dispatcher(req, rsp, _delete_route);
        }
        // 不支持的请求方法，设置响应状态码为405
        rsp->_statu = 405; 
        return;
    }
    // 当有新的连接建立时调用，设置连接的上下文
    void OnConnected(const PtrConnection &conn)
    {
        // 设置连接的上下文为HttpContext对象
        conn->SetContext(HttpContext());
        // 记录新连接的日志
        LOG(DEBUG, "NEW CONNECTION %p\n", conn.get());
    }
    // 当有数据到达缓冲区时调用，解析和处理缓冲区数据
    void OnMessage(const PtrConnection &conn, Buffer *buffer)
    {
        // 当缓冲区有可读数据时循环处理
        while (buffer->ReadAbleSize() > 0)
        {
            // 1. 获取上下文
            // 获取连接的上下文并转换为HttpContext指针
            HttpContext *context = conn->GetContext()->get<HttpContext>();
            // 2. 通过上下文对缓冲区数据进行解析，得到HttpRequest对象
            //   1. 如果缓冲区的数据解析出错，就直接回复出错响应
            //   2. 如果解析正常，且请求已经获取完毕，才开始去进行处理
            // 调用上下文的方法解析HTTP请求
            context->RecvHttpRequest(buffer);
            // 获取解析后的HttpRequest对象
            HttpRequest &req = context->Request();
            // 创建HttpResponse对象，初始状态码为上下文的响应状态码
            HttpResponse rsp(context->RespStatu());
            // 如果解析出错，状态码大于等于400
            if (context->RespStatu() >= 400)
            {
                // 进行错误响应，关闭连接
                // 调用错误处理函数填充响应内容
                ErrorHandler(req, &rsp);      
                // 组织并发送响应
                WriteReponse(conn, req, rsp); 
                // 重置上下文
                context->ReSet();
                // 清空缓冲区数据
                buffer->MoveReadOffset(buffer->ReadAbleSize()); 
                // 关闭连接
                conn->Shutdown();                               
                return;
            }
            // 如果请求还没有接收完整
            if (context->RecvStatu() != RECV_HTTP_OVER)
            {
                // 当前请求还没有接收完整,则退出，等新数据到来再重新继续处理
                return;
            }
            // 3. 请求路由 + 业务处理
            // 调用路由函数处理请求
            Route(req, &rsp);
            // 4. 对HttpResponse进行组织发送
            // 组织并发送响应
            WriteReponse(conn, req, rsp);
            // 5. 重置上下文
            // 重置上下文
            context->ReSet();
            // 6. 根据长短连接判断是否关闭连接或者继续处理
            // 如果是短连接
            if (rsp.Close() == true)
                // 关闭连接
                conn->Shutdown(); 
        }
        return;
    }

public:
    // 构造函数，初始化服务器
    HttpServer(int port, int timeout = DEFALT_TIMEOUT) : _server(port)
    {
        // 启用非活跃连接的释放功能
        _server.EnableInactiveRelease(timeout);
        // 设置连接建立时的回调函数
        _server.SetConnectedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
        // 设置有数据到达时的回调函数
        _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
    // 设置静态资源的根目录
    void SetBaseDir(const std::string &path)
    {
        // 确保路径是一个有效的目录
        assert(Util::IsDirectory(path) == true);
        // 设置静态资源根目录
        _basedir = path;
    }
    /*设置/添加，请求（请求的正则表达）与处理函数的映射关系*/
    // 添加GET请求的路由规则
    void Get(const std::string &pattern, const Handler &handler)
    {
        // 将正则表达式和处理函数添加到GET请求的路由表中
        _get_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    // 添加POST请求的路由规则
    void Post(const std::string &pattern, const Handler &handler)
    {
        // 将正则表达式和处理函数添加到POST请求的路由表中
        _post_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    // 添加PUT请求的路由规则
    void Put(const std::string &pattern, const Handler &handler)
    {
        // 将正则表达式和处理函数添加到PUT请求的路由表中
        _put_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    // 添加DELETE请求的路由规则
    void Delete(const std::string &pattern, const Handler &handler)
    {
        // 将正则表达式和处理函数添加到DELETE请求的路由表中
        _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    // 设置服务器的线程数量
    void SetThreadCount(int count)
    {
        // 设置底层TCP服务器的线程数量
        _server.SetThreadCount(count);
    }
    // 启动服务器监听
    void Listen()
    {
        // 启动底层TCP服务器
        _server.Start();
    }
};