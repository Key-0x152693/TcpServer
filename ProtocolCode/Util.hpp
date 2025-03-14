#pragma once
#include"statuANDmime.hpp"
#include<sys/stat.h>

// 定义一个工具类 Util，其中的方法都是静态的，提供一些常用的工具函数
class Util
{
public:
    // 字符串分割函数，将 src 字符串按照 sep 字符进行分割，得到的各个子串放到 arry 中，最终返回子串的数量
    static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *arry)
    {
        // 初始化偏移量为 0，用于记录查找的起始位置
        size_t offset = 0;
        // 注释说明偏移量的范围，offset 等于 src 的长度时表示已经越界
        // 有 10 个字符，offset 是查找的起始位置，范围应该是 0~9，offset==10 就代表已经越界了
        while (offset < src.size())
        {
            // 在 src 字符串偏移量 offset 处，开始向后查找 sep 字符/子串，返回查找到的位置
            size_t pos = src.find(sep, offset); 
            if (pos == std::string::npos)
            { // 没有找到特定的字符
                // 将剩余的部分当作一个子串，放入 arry 中
                if (pos == src.size())
                    break;
                arry->push_back(src.substr(offset));
                return arry->size();
            }
            if (pos == offset)
            {
                // 如果当前位置就是分隔符，将偏移量后移分隔符的长度
                offset = pos + sep.size();
                // 当前子串是一个空的，没有内容，跳过本次循环
                continue; 
            }
            // 将从 offset 到 pos 之间的子串添加到 arry 中
            arry->push_back(src.substr(offset, pos - offset));
            // 更新偏移量，跳过已经处理的子串和分隔符
            offset = pos + sep.size();
        }
        // 返回分割后子串的数量
        return arry->size();
    }

    // 读取文件的所有内容，将读取的内容放到一个 std::string 中
    static bool ReadFile(const std::string &filename, std::string *buf)
    {
        // 以二进制模式打开文件
        std::ifstream ifs(filename, std::ios::binary);
        if (ifs.is_open() == false)
        {
            // 打印打开文件失败的信息
            printf("OPEN %s FILE FAILED!!", filename.c_str());
            return false;
        }
        // 用于存储文件大小
        size_t fsize = 0;
        // 跳转读写位置到文件末尾
        ifs.seekg(0, ifs.end); 
        // 获取当前读写位置相对于起始位置的偏移量，从末尾偏移刚好就是文件大小
        fsize = ifs.tellg();   
        // 跳转到文件起始位置
        ifs.seekg(0, ifs.beg); 
        // 为 buf 开辟文件大小的空间
        buf->resize(fsize);    
        // 从文件中读取数据到 buf 中
        ifs.read(&(*buf)[0], fsize);
        if (ifs.good() == false)
        {
            // 打印读取文件失败的信息
            printf("READ %s FILE FAILED!!", filename.c_str());
            // 关闭文件
            ifs.close();
            return false;
        }
        // 关闭文件
        ifs.close();
        return true;
    }

    // 向文件写入数据
    static bool WriteFile(const std::string &filename, const std::string &buf)
    {
        // 以二进制模式打开文件，并清空文件原有内容
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        if (ofs.is_open() == false)
        {
            // 打印打开文件失败的信息
            printf("OPEN %s FILE FAILED!!", filename.c_str());
            return false;
        }
        // 将 buf 中的数据写入文件
        ofs.write(buf.c_str(), buf.size());
        if (ofs.good() == false)
        {
            // 记录写入文件失败的日志
            LOG(ERROR, "WRITE %s FILE FAILED!\n", filename.c_str());
            // 关闭文件
            ofs.close();
            return false;
        }
        // 关闭文件
        ofs.close();
        return true;
    }

    // URL 编码，避免 URL 中资源路径与查询字符串中的特殊字符与 HTTP 请求中特殊字符产生歧义
    // 编码格式：将特殊字符的 ascii 值，转换为两个 16 进制字符，前缀%   C++ -> C%2B%2B
    // 不编码的特殊字符：RFC3986 文档规定 . - _ ~ 字母，数字属于绝对不编码字符
    // RFC3986 文档规定，编码格式 %HH
    // W3C 标准中规定，查询字符串中的空格，需要编码为+， 解码则是+转空格
    static std::string UrlEncode(const std::string url, bool convert_space_to_plus)
    {
        // 用于存储编码后的字符串
        std::string res;
        for (auto &c : url)
        {
            // 如果字符是 . - _ ~ 或者是字母、数字，则直接添加到结果字符串中
            if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c))
            {
                res += c;
                continue;
            }
            // 如果字符是空格且需要将空格转换为 +，则添加 + 到结果字符串中
            if (c == ' ' && convert_space_to_plus == true)
            {
                res += '+';
                continue;
            }
            // 剩下的字符都是需要编码成为 %HH 格式
            char tmp[4] = {0};
            // snprintf 与 printf 比较类似，都是格式化字符串，只不过一个是打印，一个是放到一块空间中
            snprintf(tmp, 4, "%%%02X", c);
            res += tmp;
        }
        return res;
    }

    // 将十六进制字符转换为对应的整数值
    static char HEXTOI(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        else if (c >= 'a' && c <= 'z')
        {
            return c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'Z')
        {
            return c - 'A' + 10;
        }
        // 如果字符不是有效的十六进制字符，返回 -1
        return -1;
    }

    // URL 解码，将编码后的 URL 字符串解码为原始字符串
    static std::string UrlDecode(const std::string url, bool convert_plus_to_space)
    {
        // 遇到了%，则将紧随其后的 2 个字符，转换为数字，第一个数字左移 4 位，然后加上第二个数字  + -> 2b  %2b->2 << 4 + 11
        std::string res;
        for (int i = 0; i < url.size(); i++)
        {
            // 如果字符是 + 且需要将 + 转换为空格，则添加空格到结果字符串中
            if (url[i] == '+' && convert_plus_to_space == true)
            {
                res += ' ';
                continue;
            }
            // 如果字符是 % 且后面还有两个字符，则进行解码操作
            if (url[i] == '%' && (i + 2) < url.size())
            {
                // 将 % 后面的第一个字符转换为整数值
                char v1 = HEXTOI(url[i + 1]);
                // 将 % 后面的第二个字符转换为整数值
                char v2 = HEXTOI(url[i + 2]);
                // 计算解码后的字符值
                char v = v1 * 16 + v2;
                res += v;
                // 跳过已经处理的两个字符
                i += 2;
                continue;
            }
            // 如果字符不需要解码，直接添加到结果字符串中
            res += url[i];
        }
        return res;
    }

    // 响应状态码的描述信息获取
    static std::string StatuDesc(int statu)
    {
        // 在状态码映射表中查找对应的描述信息
        auto it = _statu_msg.find(statu);
        if (it != _statu_msg.end())
        {
            return it->second;
        }
        // 如果未找到，返回 "Unknow"
        return "Unknow";
    }

    // 根据文件后缀名获取文件 MIME 类型
    static std::string ExtMime(const std::string &filename)
    {
        // 先获取文件扩展名
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos)
        {
            // 如果没有扩展名，返回默认的 MIME 类型
            return "application/octet-stream";
        }
        // 根据扩展名，获取 MIME 类型
        std::string ext = filename.substr(pos);
        auto it = _mime_msg.find(ext);
        if (it == _mime_msg.end())
        {
            // 如果未找到对应的 MIME 类型，返回默认的 MIME 类型
            return "application/octet-stream";
        }
        return it->second;
    }

    // 判断一个文件是否是一个目录
    static bool IsDirectory(const std::string &filename)
    {
        // 用于存储文件状态信息的结构体
        struct stat st;
        // 获取文件的状态信息
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            // 如果获取状态信息失败，返回 false
            return false;
        }
        // 判断文件是否是目录
        return S_ISDIR(st.st_mode);
    }

    // 判断一个文件是否是一个普通文件
    static bool IsRegular(const std::string &filename)
    {
        // 用于存储文件状态信息的结构体
        struct stat st;
        // 获取文件的状态信息
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            // 如果获取状态信息失败，返回 false
            return false;
        }
        // 判断文件是否是普通文件
        return S_ISREG(st.st_mode);
    }

    // http 请求的资源路径有效性判断
    //  /index.html  --- 前边的/叫做相对根目录  映射的是某个服务器上的子目录
    //  想表达的意思就是，客户端只能请求相对根目录中的资源，其他地方的资源都不予理会
    //  /../login, 这个路径中的..会让路径的查找跑到相对根目录之外，这是不合理的，不安全的
    static bool ValidPath(const std::string &path)
    {
        // 思想：按照/进行路径分割，根据有多少子目录，计算目录深度，有多少层，深度不能小于 0
        std::vector<std::string> subdir;
        // 调用 Split 函数对路径进行分割
        Split(path, "/", &subdir);
        // 初始化目录深度为 0
        int level = 0;
        for (auto &dir : subdir)
        {
            if (dir == "..")
            {
                // 任意一层走出相对根目录，就认为有问题
                level--; 
                if (level < 0)
                    return false;
                continue;
            }
            // 目录深度加 1
            level++;
        }
        return true;
    }
};