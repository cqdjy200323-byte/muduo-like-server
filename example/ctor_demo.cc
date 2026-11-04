#include <iostream>
#include <memory>

struct Conn : public std::enable_shared_from_this<Conn>
{
    Conn()
    {
        std::cout << "构造函数开始" << std::endl;
        // std::shared_ptr<Conn> self = shared_from_this(); // 在构造函数里拿自己的 shared_ptr
        // std::cout << "拿到了，计数 = " << self.use_count() << std::endl;
    }
    void Established()
    {
        std::shared_ptr<Conn> self = shared_from_this();
        std::cout << "Established 里拿到了，计数 = " << self.use_count() << std::endl;
    }
};

int main()
{
    std::shared_ptr<Conn> conn(new Conn()); // 和你的 NewConnection 一样的写法
    conn->Established();
    return 0;
}