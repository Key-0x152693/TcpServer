#include<iostream>

// Any 类是一个通用容器，用于存储任意类型的值
class Any
{
private:
    // holder 是一个抽象基类，定义了两个纯虚函数，用于获取存储数据的类型信息和克隆对象
    class holder
    {
    public:
        // 虚析构函数，确保在删除基类指针时能正确调用派生类的析构函数
        virtual ~holder() {}
        // 纯虚函数，用于获取存储数据的类型信息
        virtual const std::type_info &type() = 0;
        // 纯虚函数，用于克隆当前对象
        virtual holder *clone() = 0;
    };

    // placeholder 是一个模板类，继承自 holder，用于实际存储具体类型的数据
    template <class T>
    class placeholder : public holder
    {
    public:
        // 构造函数，接受一个常量引用类型的参数 val，并将其赋值给成员变量 _val
        placeholder(const T &val) : _val(val) {}
        // 获取子类对象保存的数据类型
        virtual const std::type_info &type() { return typeid(T); }
        // 针对当前的对象自身，克隆出一个新的子类对象
        virtual holder *clone() { return new placeholder(_val); }

    public:
        // 存储具体类型的数据
        T _val;
    };

    // 指向 holder 基类的指针，用于存储实际的数据
    holder *_content;

public:
    // 默认构造函数，将 _content 初始化为 NULL
    Any() : _content(NULL) {}

    // 模板构造函数，接受一个常量引用类型的参数 val，创建一个 placeholder 对象并将其地址赋值给 _content
    template <class T>
    Any(const T &val) : _content(new placeholder<T>(val)) {}

    // 拷贝构造函数，接受一个常量引用类型的参数 other，根据 other 的 _content 是否为空，克隆或置为 NULL
    Any(const Any &other) : _content(other._content ? other._content->clone() : NULL) {}

    // 析构函数，释放 _content 指向的内存
    ~Any() { delete _content; }

    // 交换函数，交换当前对象和 other 对象的 _content 指针
    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }

    // 返回子类对象保存的数据的指针
    template <class T>
    T *get()
    {
        // 想要获取的数据类型，必须和保存的数据类型一致
        assert(typeid(T) == _content->type());
        // 将 _content 指针转换为 placeholder<T> 类型的指针，并返回其 _val 的地址
        return &((placeholder<T> *)_content)->_val;
    }

    // 赋值运算符的重载函数，接受一个常量引用类型的参数 val
    template <class T>
    Any &operator=(const T &val)
    {
        // 为 val 构造一个临时的通用容器，然后与当前容器自身进行指针交换，临时对象释放的时候，原先保存的数据也就被释放
        Any(val).swap(*this);
        return *this;
    }

    // 赋值运算符的重载函数，接受一个常量引用类型的参数 other
    Any &operator=(const Any &other)
    {
        // 为 other 构造一个临时的通用容器，然后与当前容器自身进行指针交换
        Any(other).swap(*this);
        return *this;
    }
};