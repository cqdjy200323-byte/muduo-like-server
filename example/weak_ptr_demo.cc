#include <iostream>
#include <memory>

struct Conn
{
    ~Conn() { std::cout << "  >>> Conn 析构了" << std::endl; }
};

int main()
{
    std::shared_ptr<Conn> owner = std::make_shared<Conn>(); // 模拟 _conns 表里那一份
    std::weak_ptr<Conn> watcher = owner;                    // 观察者，不加计数
    std::cout << "1. 计数 = " << watcher.use_count() << std::endl;

    {
        std::shared_ptr<Conn> guard = watcher.lock(); // 从观察者手里临时"借"一份
        std::cout << "2. lock 之后计数 = " << watcher.use_count() << std::endl;

        owner.reset(); // 模拟 _conns.erase：表里那一份没了
        std::cout << "3. owner 释放后计数 = " << watcher.use_count() << std::endl;
        std::cout << "4. 对象还活着吗？" << (watcher.expired() ? "否" : "是") << std::endl;
    } // guard 在这里离开作用域

    std::cout << "5. guard 离开作用域后，对象还活着吗？" << (watcher.expired() ? "否" : "是") << std::endl;

    std::shared_ptr<Conn> again = watcher.lock();
    std::cout << "6. 再 lock 一次，拿到的是：" << (again ? "对象" : "空指针") << std::endl;
    return 0;
}