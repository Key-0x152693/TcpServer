#include"../Log.hpp"
#include<vector>
#include<cassert>
#include <stdint.h>

using namespace log_ns;

// 定义缓冲区的默认大小
#define BUFFER_DEFAULT_SIZE 1024

// 缓冲区类，用于管理数据的读写操作
class Buffer
{
private:
    // 使用 vector 进行内存空间管理，存储实际的数据
    std::vector<char> _buffer; 
    // 读偏移量，指示当前可读数据的起始位置
    uint64_t _reader_idx;      
    // 写偏移量，指示当前可写数据的起始位置
    uint64_t _writer_idx;      

public:
    // 构造函数，初始化读偏移和写偏移为 0，并分配默认大小的缓冲区
    Buffer() : _reader_idx(0), _writer_idx(0), _buffer(BUFFER_DEFAULT_SIZE) {}

    // 返回缓冲区的起始地址
    char *Begin() { return &*_buffer.begin(); }

    // 获取当前写入起始地址，即 _buffer 的空间起始地址加上写偏移量
    char *WritePosition() { return Begin() + _writer_idx; }

    // 获取当前读取起始地址，即 _buffer 的空间起始地址加上读偏移量
    char *ReadPosition() { return Begin() + _reader_idx; }

    // 获取缓冲区末尾空闲空间大小，即总体空间大小减去写偏移量
    uint64_t TailIdleSize() { return _buffer.size() - _writer_idx; }

    // 获取缓冲区起始空闲空间大小，即读偏移量
    uint64_t HeadIdleSize() { return _reader_idx; }

    // 获取可读数据大小，等于写偏移量减去读偏移量
    uint64_t ReadAbleSize() { return _writer_idx - _reader_idx; }

    // 将读偏移向后移动指定长度
    void MoveReadOffset(uint64_t len)
    {
        // 如果移动长度为 0，直接返回
        if (len == 0)
            return;
        // 确保向后移动的大小小于等于可读数据大小
        assert(len <= ReadAbleSize());
        // 更新读偏移量
        _reader_idx += len;
    }

    // 将写偏移向后移动指定长度
    void MoveWriteOffset(uint64_t len)
    {
        // 确保向后移动的大小小于等于当前后边的空闲空间大小
        assert(len <= TailIdleSize());
        // 更新写偏移量
        _writer_idx += len;
    }

    // 确保可写空间足够，整体空闲空间够了就移动数据，否则就扩容
    void EnsureWriteSpace(uint64_t len)
    {
        // 如果末尾空闲空间大小足够，直接返回
        if (TailIdleSize() >= len)
        {
            return;
        }
        // 末尾空闲空间不够，则判断加上起始位置的空闲空间大小是否足够
        if (len <= TailIdleSize() + HeadIdleSize())
        {
            // 把当前数据大小先保存起来
            uint64_t rsz = ReadAbleSize();
            // 把可读数据拷贝到起始位置
            std::copy(ReadPosition(), ReadPosition() + rsz, Begin());
            // 将读偏移归 0
            _reader_idx = 0;
            // 将写位置置为可读数据大小，因为当前的可读数据大小就是写偏移量
            _writer_idx = rsz;
        }
        else
        {
            // 总体空间不够，则需要扩容，不移动数据，直接给写偏移之后扩容足够空间
            LOG(DEBUG, "RESIZE %ld\n", _writer_idx + len);
            _buffer.resize(_writer_idx + len);
        }
    }

    // 写入数据
    void Write(const void *data, uint64_t len)
    {
        // 如果写入长度为 0，直接返回
        if (len == 0)
            return;
        // 确保有足够的空间来写入数据
        EnsureWriteSpace(len);
        // 将传入的数据指针转换为 char 类型指针
        const char *d = (const char *)data;
        // 将数据拷贝到写入位置
        std::copy(d, d + len, WritePosition());
    }

    // 写入数据并更新写偏移量
    void WriteAndPush(const void *data, uint64_t len)
    {
        // 写入数据
        Write(data, len);
        // 更新写偏移量
        MoveWriteOffset(len);
    }

    // 写入字符串数据
    void Write(const std::string &data)
    {
        // 调用 Write 函数，传入字符串的字符数组和长度
        return Write(data.c_str(), data.size());
    }

    // 写入字符串数据并更新写偏移量
    void WriteAndPush(const std::string &data)
    {
        // 写入字符串数据
        Write(data);
        // 更新写偏移量
        MoveWriteOffset(data.size());
    }

    // 写入另一个缓冲区的数据
    void Write(Buffer &data)
    {
        // 调用 Write 函数，传入另一个缓冲区的读取位置和可读数据大小
        return Write(data.ReadPosition(), data.ReadAbleSize());
    }

    // 写入另一个缓冲区的数据并更新写偏移量
    void WriteAndPush(Buffer &data)
    {
        // 写入另一个缓冲区的数据
        Write(data);
        // 更新写偏移量
        MoveWriteOffset(data.ReadAbleSize());
    }

    // 读取数据到指定缓冲区
    void Read(void *buf, uint64_t len)
    {
        // 确保要读取的数据大小小于等于可读数据大小
        assert(len <= ReadAbleSize());
        // 将数据从读取位置拷贝到指定缓冲区
        std::copy(ReadPosition(), ReadPosition() + len, (char *)buf);
    }

    // 读取数据到指定缓冲区并更新读偏移量
    void ReadAndPop(void *buf, uint64_t len)
    {
        // 读取数据
        Read(buf, len);
        // 更新读偏移量
        MoveReadOffset(len);
    }

    // 读取指定长度的数据并返回字符串
    std::string Read(uint64_t len)
    {
        // 确保要读取的数据大小小于等于可读数据大小
        assert(len <= ReadAbleSize());
        // 创建一个指定长度的字符串
        std::string str;
        str.resize(len);
        // 读取数据到字符串中
        Read(&str[0], len);
        // 返回读取的字符串
        return str;
    }

    // 读取指定长度的数据并返回字符串，同时更新读偏移量
    std::string ReadAndPop(uint64_t len)
    {
        // 确保要读取的数据大小小于等于可读数据大小
        assert(len <= ReadAbleSize());
        // 读取指定长度的数据
        std::string str = Read(len);
        // 更新读偏移量
        MoveReadOffset(len);
        // 返回读取的字符串
        return str;
    }

    // 在可读数据中查找换行符 '\n'，返回其位置指针
    char *FindCRLF()
    {
        // 使用 memchr 函数在可读数据中查找换行符
        char *res = (char *)memchr(ReadPosition(), '\n', ReadAbleSize());
        return res;
    }

    // 获取一行数据，包含换行符
    std::string GetLine()
    {
        // 查找换行符的位置
        char *pos = FindCRLF();
        // 如果未找到换行符，返回空字符串
        if (pos == NULL)
        {
            return "";
        }
        // +1 是为了把换行字符也取出来
        return Read(pos - ReadPosition() + 1);
    }

    // 获取一行数据并更新读偏移量
    std::string GetLineAndPop()
    {
        // 获取一行数据
        std::string str = GetLine();
        // 更新读偏移量
        MoveReadOffset(str.size());
        // 返回获取的一行数据
        return str;
    }

    // 清空缓冲区，将读偏移和写偏移都置为 0
    void Clear()
    {
        _reader_idx = 0;
        _writer_idx = 0;
    }
};

