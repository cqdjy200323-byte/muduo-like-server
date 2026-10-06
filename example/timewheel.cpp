#include <iostream>
#include <vector>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <memory>
#include <unistd.h>

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask
{
public:
    TimerTask(uint64_t id, uint32_t timeout, const TaskFunc &cb)
        : _id(id), _timeout(timeout), _task_cb(cb), _canceled(false)
    {
    }

    ~TimerTask()
    {
        _task_cb();
        _release();
    }

    void SetRelease(const ReleaseFunc &cb)
    {
        _release = cb;
    }

    uint32_t DelayTime()
    {
        return _timeout;
    }

    void Cancel() { _canceled = true; }

private:
    uint64_t _id;
    uint32_t _timeout;
    TaskFunc _task_cb;
    ReleaseFunc _release;
    bool _canceled;
};

using WeakTask = std::weak_ptr<TimerTask>;
using PtrTask = std::shared_ptr<TimerTask>;

class TimerWheel
{
public:
    TimerWheel()
        : _tick(0), _capacity(60), _wheel(_capacity)
    {
    }

    void TimerAdd(uint64_t id, uint32_t timeout, const TaskFunc &cb)
    {
        auto task = std::make_shared<TimerTask>(id, timeout, cb);
        task->SetRelease([this, id]()
                         { _timers.erase(id); });
        int pos = (_tick + timeout) % _capacity;
        _wheel[pos].push_back(task);
        _timers[id] = WeakTask(task);
    }

    void TimerRefresh(uint64_t id)
    {
        auto s = _timers.find(id);
        if (s == _timers.end())
        {
            return;
        }
        PtrTask pt = s->second.lock();
        int timeout = pt->DelayTime();
        int pos = (_tick + timeout) % _capacity;
        _wheel[pos].push_back(pt);
    }
    void TimerCancel(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 没找着定时任务，没法刷新，没法延迟
        }
        PtrTask pt = it->second.lock();
        if (pt)
            pt->Cancel();
    }
    void RunTimerTask()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear();
    }

private:
    int _tick;
    int _capacity;
    std::vector<std::vector<PtrTask>> _wheel;
    std::unordered_map<uint64_t, WeakTask> _timers;
};

class Test
{
public:
    Test()
    {
        std::cout << "测试构造函数" << std::endl;
    }
    ~Test()
    {
        std::cout << "测试析构函数" << std::endl;
    }
};

void TestDelete(Test* t)
{
    delete t;
}

int main()
{
    TimerWheel tw;
    Test *t = new Test();
    tw.TimerAdd(888,5,std::bind(TestDelete,t));

    for(int i = 0; i < 5; i++) {
        sleep(1);
        tw.TimerRefresh(888);//刷新定时任务
        tw.RunTimerTask();//向后移动秒针
        std::cout << "刷新了一下定时任务，重新需要5s钟后才会销毁\n";
    }

    while(1) {
        sleep(1);
        std::cout << "-------------------\n";
        tw.RunTimerTask();//向后移动秒针
    }

    return 0;
}