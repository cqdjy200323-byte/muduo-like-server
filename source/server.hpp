#ifndef _M_SERVER_H_
#define _M_SERVER_H_
#define BUFFER_DEFAULT_SIZE 1024
#include <iostream>
#include <ctime>
#include <vector>
#include <cassert>
#include <string>
#include <functional>
#include <unistd.h>
#include <memory>
#include <cstdint>
#include <string.h>
#include <ctime>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <sys/eventfd.h>
#include <condition_variable>
#include <signal.h>
#include <sys/timerfd.h>

static void SetNonBlock(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG
#define LOG(level, format, ...)                                                                                       \
    do                                                                                                                \
    {                                                                                                                 \
        if (level < LOG_LEVEL)                                                                                        \
            break;                                                                                                    \
        time_t t = time(NULL);                                                                                        \
        struct tm *ltm = localtime(&t);                                                                               \
        char tmp[32] = {0};                                                                                           \
        strftime(tmp, 31, "%H:%M:%S", ltm);                                                                           \
        fprintf(stdout, "[%p %s %s:%d]" format "\n", (void *)pthread_self(), tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

class Any
{
private:
    class holder
    {
    public:
        virtual ~holder();
        virtual const std::type_info &tipe();
        virtual holder *clone();
    };

    template <class T>
    class placeholder : public holder
    {
    public:
        placeholder(const T val)
            : _val(val)
        {
        }

        virtual const std::type_info &type()
        {
            return typeid(T);
        }
        virtual holder *clone()
        {
            return new placeholder(_val);
        }

    public:
        T _val;
    };
    holder *_content;

public:
    Any()
        : _content(NULL)
    {
    }
    template <class T>
    Any(const T &val)
        : _content(new placeholder<T>(val))
    {
    }

    Any(const Any &other)
        : _content(other._content ? other._content->clone() : NULL)
    {
    }

    ~Any()
    {
        delete _content;
    }

    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }

    template <class T>
    T Swap(const T &val)
    {
        placeholder<T> a;
        std::swap(_content, a);
    }

    template <class T>
    Any &operator=(const T &val)
    {
        Any(val).swap(*this);
        return *this;
    }
    Any &operator=(const Any &other)
    {
        Any(other).swap(*this);
        return *this;
    }
};

class Buffer
{
private:
    std::vector<char> _buffer;
    uint64_t _reader_idx;
    uint64_t _writer_idx;

public:
    Buffer()
        : _reader_idx(0), _writer_idx(0), _buffer(BUFFER_DEFAULT_SIZE)
    {
    }
    // 获取写入的起始位置
    char *Begin()
    {
        return &*_buffer.begin();
    }
    char *WritePosition()
    {
        return Begin() + _writer_idx;
    }
    // 获取读取的起始位置
    char *ReadPosition()
    {
        return Begin() + _reader_idx;
    }
    // 获取写偏移之后的空间
    uint64_t WriteOffsetSpace()
    {
        return _buffer.size() - _writer_idx;
    }
    // 获取读偏移之前的空间
    uint64_t ReadOffsetSpace()
    {
        return _reader_idx;
    }
    // 获取可读空间大小
    uint64_t ReadableSpace()
    {
        return _writer_idx - _reader_idx;
    }

    // 检测空间是否足够（不够就扩容）
    bool SpaceEnough(int len)
    {
        if (WriteOffsetSpace() > len)
        {
            return false;
        }
        else if (WriteOffsetSpace() + ReadOffsetSpace() >= len)
        {
            uint64_t rez = ReadableSpace();
            std::copy(ReadPosition(), ReadPosition() + rez, Begin());
            _reader_idx = 0;
            _writer_idx = rez;
        }
        else
        {
            _buffer.resize(_writer_idx + len);
        }
        return true;
    }
    // 将读偏移向后移动
    void MoveReadOffset(int len)
    {
        assert(ReadableSpace() >= len);
        _reader_idx += len;
    }
    // 将写偏移向后移动
    void MoveWriteOffset(int len)
    {
        assert(WriteOffsetSpace() > len);
        _writer_idx += len;
    }

    // 读数据
    void Read(void *buf, uint64_t len)
    {
        assert(ReadableSpace() >= len);
        std::copy(ReadPosition(), ReadPosition() + len, (char *)buf);
    }
    // 读数据并且弹出
    void ReadPop(void *buf, uint64_t len)
    {
        Read(buf, len);
        MoveReadOffset(len);
    }

    // 读一个string并以string返回
    std::string ReadString(uint64_t len)
    {
        assert(len <= ReadableSpace());
        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }
    // 读string并弹出

    std::string ReadStringPop(uint64_t len)
    {

        std::string str = ReadString(len);
        MoveReadOffset(len);
        return str;
    }

    // 写数据
    void Write(const void *data, uint64_t len)
    {
        if (len == 0)
        {
            return;
        }
        SpaceEnough(len);
        char *d = (char *)data;
        std::copy(d, d + len, WritePosition());
    }
    // 写数据并且压入
    void WritePush(const void *data, uint64_t len)
    {
        Write(data, len);
        MoveWriteOffset(len);
    }
    // 写入一个string
    void WriteString(std::string data)
    {
        Write(&data[0], data.size());
    }

    void WriteStringAndPush(const std::string &data)
    {
        WriteString(data);
        MoveWriteOffset(data.size());
    }
    void WriteBuffer(Buffer &data)
    {
        return Write(data.ReadPosition(), data.ReadableSpace());
    }
    void WriteBufferAndPush(Buffer &data)
    {
        WriteBuffer(data);
        MoveWriteOffset(data.ReadableSpace());
    }

    char *FindCRLF()
    {
        void *res = memchr(ReadPosition(), '\n', ReadableSpace());
        return (char *)res;
    }

    std::string GetLine()
    {
        char *pos = FindCRLF();
        if (pos == NULL)
        {
            return " ";
        }
        return ReadString(pos - ReadPosition() + 1);
    }

    std::string GetLinePop()
    {
        std::string str = GetLine();
        MoveReadOffset(str.size());
        return str;
    }
    // 清空
    void Clear()
    {
        _reader_idx = 0;
        _writer_idx = 0;
    }
};

#define Max_LISTEN 1024
class Socket
{
private:
    int _sockfd;

public:
    Socket()
        : _sockfd(-1)
    {
    }

    Socket(int fd)
        : _sockfd(fd)
    {
    }

    bool Create()
    {
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sockfd < 0)
        {
            ERR_LOG("CREATE SOCKET FAILED!!");
            return false;
        }
        return true;
    }

    bool Bind(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("BIND ADDRESS FAILED!!");
            return false;
        }
        return true;
    }
    bool Listen(int backlog = Max_LISTEN)
    {
        int ret = listen(_sockfd, backlog);
        if (ret < 0)
        {
            ERR_LOG("SOCKET LISTEN FAILED!");
            return false;
        }
        return true;
    }

    bool Connect(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("SOCKET CONNECT FAILED!!");
            return false;
        }
        return true;
    }

    int Accept()
    {
        int newfd = accept(_sockfd, NULL, NULL);
        if (newfd <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            return -1;
        }
        return newfd;
    }

    ssize_t Recv(void *buf, size_t len, int flag = 0)
    {
        ssize_t ret = recv(_sockfd, buf, len, flag);
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("SOCKET RECV FAILED!!");
            return -1;
        }
        return ret;
    }

    ssize_t NonBlockReve(void *buf, size_t len)
    {
        return Recv(buf, len, MSG_DONTWAIT);
    }

    ssize_t Send(const void *buf, size_t len, int flag = 0)
    {
        ssize_t ret = send(_sockfd, buf, len, flag);
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("SOCKET SEND FAILED!!");
            return -1;
        }
        return ret;
    }
    ssize_t NonBlockSend(void *buf, size_t len)
    {
        return Send(buf, len, MSG_DONTWAIT);
    }

    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }

    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0")
    {
        // 1.创建套接字,设置非阻塞
        if (Create() == false)
            return false;
        NonBlock();

        // 4.启动地址重用
        ReuseAddress();
        // 2.绑定地址
        if (Bind(ip, port) == false)
            return false;
        // 3.开始监听
        if (Listen() == false)
            return false;

        return true;
    }
    bool CreateClient(uint16_t port, const std::string &ip)
    {
        // 1. 创建套接字，2.指向连接服务器
        if (Create() == false)
            return false;
        if (Connect(ip, port) == false)
            return false;
        return true;
    }

    // 设置套接字选项--开启地址端口重用
    void ReuseAddress()
    {
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
        val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
    }

    // 设置非阻塞
    void NonBlock()
    {
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }

    int Fd()
    {
        return _sockfd;
    }
};

class Poller;
class EventLoop;
class Channel
{
private:
    int _fd;
    uint32_t _events;  // 当前需要监控的事件
    uint32_t _revents; // 当前需要连续触发的事件
    EventLoop *_loop;
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;
    EventCallback _write_callback;
    EventCallback _error_callback;
    EventCallback _close_callback;
    EventCallback _event_callback;

public:
    Channel(EventLoop *loop, int fd)
        : _fd(fd), _events(0), _revents(0), _loop(loop)
    {
    }

    int Fd()
    {
        return _fd;
    }
    uint32_t Events()
    {
        return _events;
    }

    void SetREvents(uint32_t revents)
    {
        _revents = revents;
    }
    void SetReadCallback(const EventCallback &cb)
    {
        _read_callback = cb;
    }
    void SetWriteCallback(const EventCallback &cb)
    {
        _write_callback = cb;
    }
    void SetErrorCallback(const EventCallback &cb)
    {
        _error_callback = cb;
    }
    void SetCloseCallback(const EventCallback &cb)
    {
        _close_callback = cb;
    }
    void SetEventCallback(const EventCallback &cb)
    {
        _event_callback = cb;
    }
    bool ReadAble()
    {
        return (_events & EPOLLIN);
    }
    bool WriteAble()
    {
        return (_events & EPOLLOUT);
    }
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    }
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    }
    void DisableRead()
    {
        _events &= ~EPOLLIN;
        Update();
    }
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    }
    void DisableAll()
    {
        _events = 0;
        Update();
    }

    void Update();
    void Remove();
    // 事件处理，连续促发事件，就调用此函数，触发什么事件再去调用什么回调函数
    void HandleEvent()
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            if (_read_callback)
                _read_callback();
        }

        if (_revents & EPOLLOUT)
        {
            if (_write_callback)
                _write_callback();
        }
        else if (_revents & EPOLLERR)
        {
            if (_error_callback)
                _error_callback();
        }
        else if (_revents & EPOLLHUP)
        {
            if (_close_callback)
                _close_callback();
        }

        if (_event_callback)
            _event_callback();
    }
};

#define MAX_EPOLLEVENTS 1024
class Poller
{
private:
    int _epfd;
    struct epoll_event _evs[MAX_EPOLLEVENTS];
    std::unordered_map<int, Channel *> _channels;

public:
    void Update(Channel *channel, int op)
    {
        // int epoll_ctl(int epfd,int op,int fd,struct epoll_evevt* ev)
        int fd = channel->Fd();
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->Events();
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if (ret < 0)
        {
            ERR_LOG("EPOLLCTL FAILED");
            abort();
        }
    }

    // 判断Channel是否被监控
    bool HasChannel(Channel *channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it == _channels.end())
        {
            return false;
        }
        return true;
    }

public:
    Poller()
    {
        _epfd = epoll_create(MAX_EPOLLEVENTS);
        if (_epfd < 0)
        {
            ERR_LOG("EPOLL CREATE FAILED!!");
            abort(); // 退出程序
        }
    }
    // 添加或修改监控事件
    void UpdateEvent(Channel *channel)
    {
        bool ret = HasChannel(channel);
        if (ret == false)
        {
            _channels.insert(std::make_pair(channel->Fd(), channel));
            return Update(channel, EPOLL_CTL_ADD);
        }
        return Update(channel, EPOLL_CTL_MOD);
    }

    void RemoveEvent(Channel *channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it != _channels.end())
        {
            _channels.erase(it);
        }
        Update(channel, EPOLL_CTL_DEL);
    }

    // 开始监控
    void poll(std::vector<Channel *> *active)
    {
        // int epoll-wait(int epfd ,struct epoll_event*evs ,int maxevents ,int timeout)
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("EPOLL WAIT ERROR:%s\n", strerror(errno));
            abort();
        }
        for (int i = 0; i < nfds; i++)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());
            it->second->SetREvents(_evs[i].events); // 设置实际就绪的事件
            active->push_back(it->second);
        }
        return;
    }
};

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
        DBG_LOG("~TimerTask");
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
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(id);
        }
    }

    static int CreatTimeFd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            ERR_LOG("TIMERFD CREATE FAILED!!");
            abort();
        }
        struct itimerspec itime;
        // 第一次超时时间为一秒后
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0;
        // 第一次超时后每一次超时的间隔时间
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0;
        timerfd_settime(timerfd, 0, &itime, NULL);
        return timerfd;
    }

    void ReadHandle()
    {
        uint64_t timer;
        int ret = read(_timerfd, &timer, 8);
        if (ret < 0)
        {
            ERR_LOG("READ TIMERFD FAILED!!");
            abort();
        }
    }

    // 该函数每秒应该被执行一次
    void RunTimerTask()
    {
        _tick += 1;
        _tick %= _capacity;
        _wheel[_tick].clear();
    }

    void OnTime()
    {
        ReadHandle();
        RunTimerTask();
    }

public:
    TimerWheel(EventLoop *loop)
        : _tick(0), _capacity(60), _wheel(_capacity), _loop(loop), _timerfd(CreatTimeFd()), _time_channel(new Channel(loop, _timerfd))
    {
        _time_channel->SetReadCallback(std::bind(&TimerWheel::OnTime, this));
        _time_channel->EnableRead();
    }

    void TimerAddInLoop(uint64_t id, uint32_t timeout, const TaskFunc &cb)
    {
        auto task = std::make_shared<TimerTask>(id, timeout, cb);
        task->SetRelease([this, id]()
                         { _timers.erase(id); });
        int pos = (_tick + timeout) % _capacity;
        _wheel[pos].push_back(task);
        _timers[id] = WeakTask(task);
    }

    void TimerRefreshInLoop(uint64_t id)
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

    void TimerCancelInLoop(uint64_t id)
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

    void TimerRefresh(uint64_t id);

    void TimerAdd(uint64_t id, uint32_t timeout, const TaskFunc &cb);

    void TimerCancel(uint64_t id);

    bool HasTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return false;
        }
        return true;
    }

private:
    int _tick;
    int _capacity;
    std::vector<std::vector<PtrTask>> _wheel;
    std::unordered_map<uint64_t, WeakTask> _timers;
    EventLoop *_loop;
    int _timerfd;
    std::unique_ptr<Channel> _time_channel;
};

class EventLoop
{
private:
    // 事件触发的监控
    Poller _poller;
    // 首先得有个刷新事件的eventfd
    int _event_fd;
    // 管理event_fd的channel
    std::unique_ptr<Channel> _event_channel;
    // 任务队列
    using Functor = std::function<void()>;
    std::vector<Functor> _task;
    // 对任务队列操作时的锁
    std::mutex _mutex;
    // 线程id
    std::thread::id _thread_id;

    TimerWheel _timer_wheel;

    static int CreatEventFd()
    {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd < 0)
        {
            ERR_LOG("CREATE EVENTFD FAILED!!");
            abort();
        }
        return efd;
    }
    void ReadEventFd()
    {
        uint64_t res = 0;
        res = read(_event_fd, &res, sizeof(res));
        if (res < 0)
        {
            // EINTR是被信号打断，EAGAIN无数据可读
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!!");
            abort();
        }
        return;
    }

    void WeakUpEventFd()
    {
        uint64_t val = 1;
        int res = write(_event_fd, &val, sizeof(val));
        if (res < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("WEAKEUP EVENTFD FAILED!!");
            abort();
        }
        return;
    }

public:
    EventLoop()
        : _event_fd(CreatEventFd()), _thread_id(std::this_thread::get_id()), _event_channel(new Channel(this, _event_fd)), _timer_wheel(this)
    {
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventFd, this));
        _event_channel->EnableRead();
    }
    void RunAllTask()
    {
        std::vector<Functor> functor;
        {
            // 上锁
            std::unique_lock<std::mutex> lock(_mutex);
            _task.swap(functor);
        }
        for (auto &e : functor)
        {
            e();
        }
    }

    // 把任务压入任务队列
    void QueueInLoop(const Functor &cb)
    {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _task.push_back(cb);
        }
        // 唤醒可能因为没有事件就绪导致的堵塞

        WeakUpEventFd();
    }
    // 判断任务是否是当前线程
    bool IsInLoop()
    {
        return _thread_id == std::this_thread::get_id();
    }

    // 用来看任务是否在当前线程是就运行任务，不是就压入任务队列
    void RunInLoop(const Functor &cb)
    {
        if (IsInLoop())
        {
            cb();
        }
        else
        {
            QueueInLoop(cb);
        }
    }

    // 启动EventLoop
    void Start()
    {
        while (1)
        {
            // 1.启动事件监控
            std::vector<Channel *> actives;
            _poller.poll(&actives);
            // 2.执行就绪的事件
            for (auto e : actives)
            {
                e->HandleEvent();
            }
            // 3.执行任务
            RunAllTask();
        }
    }

    // 添加或修改描述符的事件监控
    void UpdateEvent(Channel *channel)
    {
        return _poller.UpdateEvent(channel);
    }

    void RemoveEvent(Channel *channel)
    {
        return _poller.RemoveEvent(channel);
    }

    // 添加定时任务
    void TimerAdd(uint64_t id, uint32_t dilay, const TaskFunc &cb)
    {
        return _timer_wheel.TimerAdd(id, dilay, cb);
    }

    // 刷新定时任务
    void TimerRefresh(uint64_t id)
    {
        return _timer_wheel.TimerRefresh(id);
    }
    // 删除定时任务
    void TimerCancel(uint64_t id)
    {
        return _timer_wheel.TimerCancel(id);
    }

    // 判断是否有这个定时任务
    bool HasTimer(uint64_t id)
    {
        return _timer_wheel.HasTimer(id);
    }
};

typedef enum
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
} ConnStatu;

class Connection;
using PtrConnection = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection>
{
private:
    // Connection唯一id
    uint64_t _conn_id;
    // 判断是否开启非活跃销毁默认false
    bool _enable_inactive_release;
    // 状态

    int _sockfd;
    EventLoop *_loop;
    ConnStatu _statu;
    // 连接所关联的loop
    Socket _socket;
    Channel _channel;
    Buffer _in_buffer;
    Buffer _out_buffer;
    Any _context; // 请求接收处理上下文

    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    using CloseCallback = std::function<void(const PtrConnection &)>;
    using AnyEventCallback = std::function<void(const PtrConnection &)>;

    ConnectedCallback _connected_callback;
    MessageCallback _message_callback;
    CloseCallback _close_callback;
    AnyEventCallback _event_callback;

    // 还有一个组件内连接关闭回调,服务器组件内会把所有的连接管理起来，一旦要关闭，就要从管理的地方删除
    CloseCallback _server_close_callback;

private:
    // 真正的删除
    void ReleaseInloop()
    {
        // 1.修改连接状态
        _statu = DISCONNECTED;
        // 2.移除事件监控
        _channel.Remove();
        // 3.移除描述符
        _socket.Close();
        // 4.如果定时器有超时销毁任务就要取消，不然等定时任务到了销毁的就是空指针
        if (_loop->HasTimer(_conn_id))
            CancelInactiveRelease();
        // 5.调用关闭回调
        // 正常关闭回调
        if (_close_callback)
        {
            _close_callback(shared_from_this());
        }
        // 组件关闭回调
        if (_server_close_callback)
        {
            _server_close_callback(shared_from_this());
        }
    }
    // 描述符可读后触发的函数，接收socket的数据放到接收缓冲区，然后调用_message_callback
    void HandleRead()
    {
        char buf[65536];
        ssize_t ret = _socket.NonBlockReve(buf, 65535);
        if (ret < 0)
        {
            // 不能直接关闭
            return ShutdownInloop();
        }
        _in_buffer.WritePush(buf, ret);

        if (_in_buffer.ReadableSpace() > 0)
        {
            return _message_callback(shared_from_this(), &_in_buffer);
        }
    }
    // 描述符可写后触发的函数，将发送缓冲区的数据进行发送
    void HandleWrite()
    {
        // 先发送数据
        ssize_t ret = _socket.NonBlockSend(_out_buffer.ReadPosition(), _out_buffer.ReadableSpace());
        if (ret < 0)
        {
            // 如果发送失败就要关闭连接了，但是关闭前要先看看接收缓冲区有没有待处理的消息，如果有就先处理
            if (_in_buffer.ReadableSpace() > 0)
            {
                return _message_callback(shared_from_this(), &_in_buffer);
            }
            return ReleaseInloop();
        }
        // 千万别忘记移动读偏移!!!!
        _out_buffer.MoveReadOffset(ret);
        // 如果发送缓冲区没有数据待发送，就关闭写事件监控
        if (_out_buffer.ReadableSpace() == 0)
        {
            _channel.DisableWrite();
        }
        // 如果状态是待关闭并且没有数据发送就直接关闭
        if (_statu == DISCONNECTING && _out_buffer.ReadableSpace() == 0)
        {
            return ReleaseInloop();
        }
        return;
    }
    void HandleClose()
    {
        // 一旦挂断，套接字就什么都干不了了，因此挂断前看看是否有数据待处理
        if (_in_buffer.ReadableSpace() > 0)
        {
            return _message_callback(shared_from_this(), &_in_buffer);
        }
        return ReleaseInloop();
    }
    void HandleError()
    {
        return HandleClose();
    }
    void HandleEvent()
    {
        // 任意事件处理前，先看是否开启了非活跃销毁，如果开启了就先刷新
        if (_enable_inactive_release == true)
        {
            _loop->TimerRefresh(_conn_id);
        }
        // 触发任意事件回调函数
        if (_event_callback)
        {
            _event_callback(shared_from_this());
        }
    }
    void SendInLoop(Buffer &buf)
    {
        if (_statu == DISCONNECTED)
            return;
        _out_buffer.WriteBufferAndPush(buf);
        if (_channel.WriteAble() == false)
        {
            _channel.EnableWrite();
        }
    }
    // 这个关闭操作并非实际的连接释放操作，需要判断还有没有数据待处理，待发送
    void ShutdownInloop()
    {
        _statu = DISCONNECTING; // 设置连接为半关闭状态
        if (_in_buffer.ReadableSpace() > 0)
        {
            if (_message_callback)
                _message_callback(shared_from_this(), &_in_buffer);
        }
        // 要么就是写入数据的时候出错关闭，要么就是没有待发送数据，直接关闭
        if (_out_buffer.ReadableSpace() > 0)
        {
            if (_channel.WriteAble() == false)
            {
                _channel.EnableWrite();
            }
        }
        if (_out_buffer.ReadableSpace() == 0)
        {
            ReleaseInloop();
        }
    }
    // 启动非活跃连接超时释放规则
    void EnableInactiveReleaseInLoop(int sec)
    {
        // 1. 将判断标志 _enable_inactive_release 置为true
        _enable_inactive_release = true;
        // 2. 如果当前定时销毁任务已经存在，那就刷新延迟一下即可
        if (_loop->HasTimer(_conn_id))
        {
            return _loop->TimerRefresh(_conn_id);
        }
        // 3. 如果不存在定时销毁任务，则新增
        _loop->TimerAdd(_conn_id, sec, std::bind(&Connection::ReleaseInloop, this));
    }
    void CancelInactiveReleaseInLoop()
    {
        _enable_inactive_release = false;
        if (_loop->HasTimer(_conn_id))
        {
            _loop->TimerCancel(_conn_id);
        }
    }
    void UpgradeInLoop(const Any &constext,
                       const ConnectedCallback &conn,
                       const MessageCallback &msg,
                       const CloseCallback &closed,
                       const AnyEventCallback &event)
    {
        _context = constext;
        _connected_callback = conn;
        _message_callback = msg;
        _close_callback = closed;
        _event_callback = event;
    }
    // 建立连接后要做的事
    void EstablishedInloop()
    {
        // 修改连接状态
        assert(_statu == CONNECTING);
        _statu = CONNECTED;
        // 启动读事件监控
        _channel.EnableRead();
        // 调用连接成功回调函数
        if (_connected_callback)
        {
            return _connected_callback(shared_from_this());
        }
    }

public:
    Connection(EventLoop *loop, int sockfd, uint64_t id)
        : _conn_id(id), _enable_inactive_release(false), _statu(CONNECTING), _sockfd(sockfd), _loop(loop), _socket(sockfd), _channel(loop, sockfd)
    {
        // 设置监控到各种事件后的回调函数
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
        _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
        _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
    }
    ~Connection()
    {
        DBG_LOG("Connection的地址是%p", this);
    }
    // 返回文件描述符
    int Fd()
    {
        return _sockfd;
    }
    uint64_t Id()
    {
        return _conn_id;
    }
    bool Connected()
    {
        return (_statu == CONNECTED);
    }
    void SetContext(const Any &context)
    {
        _context = context;
    }
    Any *GetContext()
    {
        return &_context;
    }
    void SetConnetedCallback(const ConnectedCallback &cb)
    {
        _connected_callback = cb;
    }
    void SetMessageCallback(const MessageCallback &cb)
    {
        _message_callback = cb;
    }
    void SetCloseCallback(const CloseCallback &cb)
    {
        _close_callback = cb;
    }
    void SetAnyEventCallback(const AnyEventCallback &cb)
    {
        _event_callback = cb;
    }

    void SetServerCloseCallback(const CloseCallback &cb)
    {
        _server_close_callback = cb;
    }
    // 将数据放到发送缓冲区，并开启写事件监控
    void Send(const char *date, size_t len)
    {
        Buffer buf;
        buf.WritePush(date, len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, buf));
    }
    // 提供给组件使用者的关闭接口，但并不实际关闭，要判断有没有数据待处理
    void ShutDown()
    {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInloop, this));
    }
    void Established()
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishedInloop, this));
    }
    // 启动非活跃销毁,并规定多久为非活跃
    void EnableInactiveRelease(int sec)
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
    }
    // 删除非活跃销毁
    void CancelInactiveRelease()
    {
        _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
    }
    // 切换协议重置上下文
    void Upgrade(const Any &context, const ConnectedCallback &conn, const MessageCallback &msg,
                 const CloseCallback &close, const AnyEventCallback &anyevent)
    {
    }
};

class Acceptor
{
private:
    Socket _socket;
    EventLoop *_loop;
    Channel _channel;
    using AcceptCallback = std::function<void(int)>;
    AcceptCallback _accept_callback;

    void HandleRead()
    {
        int newfd = _socket.Accept();
        if (newfd < 0)
        {
            return;
        }
        if (_accept_callback)
            _accept_callback(newfd);
    }

    int CreatServer(int port)
    {
        bool ret = _socket.CreateServer(port);
        assert(ret == true);
        return _socket.Fd();
    }

public:
    Acceptor(EventLoop *loop, int port)
        : _socket(CreatServer(port)), _loop(loop), _channel(loop, _socket.Fd())
    {
        _channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
    }
    void SetAcceptCallback(const AcceptCallback &cb)
    {
        _accept_callback = cb;
    }
    void Lisen()
    {
        _channel.EnableRead();
    }
};

class LoopThread
{

private:
    std::mutex _mutex;             // 互斥锁
    std::condition_variable _cond; // 条件变量
    EventLoop *_loop;              // Eventloop实例化对象，需要在线程内实例化
    std::thread _thread;           // Eventloop对应的线程

    void ThreadEntry()
    {
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_all();
        }
        loop.Start();
    }

public:
    // 创建线程，设定线程入口
    LoopThread()
        : _loop(NULL), _thread(std::thread(&LoopThread::ThreadEntry, this))
    {
    }
    EventLoop *GetLoop()
    {
        EventLoop *loop = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [&]()
                       { return _loop != NULL; });
            loop = _loop;
        }
        return loop;
    }
};

class LoopThreadPool
{
private:
    int _thread_count;
    int _next_idx;
    EventLoop *_baseloop;
    std::vector<LoopThread *> _threads;
    std::vector<EventLoop *> _loops;

public:
    LoopThreadPool(EventLoop *baseloop) : _thread_count(0), _next_idx(0), _baseloop(baseloop) {}
    void SetThreadCount(int count) { _thread_count = count; }
    void Create()
    {
        if (_thread_count > 0)
        {
            _threads.resize(_thread_count);
            _loops.resize(_thread_count);
            for (int i = 0; i < _thread_count; i++)
            {
                _threads[i] = new LoopThread();
                _loops[i] = _threads[i]->GetLoop();
            }
        }
        return;
    }
    EventLoop *NextLoop()
    {
        if (_thread_count == 0)
        {
            return _baseloop;
        }
        _next_idx = (_next_idx + 1) % _thread_count;
        return _loops[_next_idx];
    }
};

class TcpServer
{
private:
    int _port;
    uint64_t _next_id;
    int _timeout;
    bool _enable_inactive_release;
    EventLoop _baseloop;
    Acceptor _acceptor;
    LoopThreadPool _pool;
    std::unordered_map<uint64_t, PtrConnection> _conns;

    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    using CloseCallback = std::function<void(const PtrConnection &)>;
    using AnyEventCallback = std::function<void(const PtrConnection &)>;
    using Functor = std::function<void()>;
    ConnectedCallback _connected_callback;
    MessageCallback _message_callback;
    CloseCallback _close_callback;
    AnyEventCallback _event_callback;
    // 还有一个组件内连接关闭回调,服务器组件内会把所有的连接管理起来，一旦要关闭，就要从管理的地方删除
    CloseCallback _server_close_callback;

private:
    void RunAfterInLoop(const Functor &task, int delay)
    {
        _next_id++;
        _baseloop.TimerAdd(_next_id, delay, task);
    }

    void NewConnection(int fd) // 为新连接创建一个Connection
    {
        _next_id++;
        PtrConnection conn(new Connection(_pool.NextLoop(), fd, _next_id));
        conn->SetServerCloseCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
        conn->SetConnetedCallback(_connected_callback);
        conn->SetMessageCallback(_message_callback);
        conn->SetAnyEventCallback(_event_callback);
        conn->SetCloseCallback(_close_callback);
        conn->EnableInactiveRelease(10);
        conn->Established();
        _conns.insert(std::make_pair(_next_id, conn));
    }

    void RemoveConnectionInLoop(const PtrConnection &conn)
    {
        int id = conn->Id();
        auto it = _conns.find(id);
        if (it != _conns.end())
        {
            _conns.erase(it);
        }
    }
    void RemoveConnection(const PtrConnection &conn) // 从管理的conns中删除Connection
    {
        _baseloop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
    }

public:
    TcpServer(int port)
        : _port(port), _next_id(0), _enable_inactive_release(false), _acceptor(&_baseloop, port), _pool(&_baseloop)
    {
        _acceptor.SetAcceptCallback(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
        _acceptor.Lisen();
    }

    // 设置线程吃内线程数量
    void SetThreadCount(int count)
    {
        return _pool.SetThreadCount(count);
    }

    // 设置定时任务
    void RunAfter(const Functor &task, int delay)
    {
        _baseloop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop, this, task, delay));
    }
    // 设置是否开启非活跃超时销毁
    void EnableInacticeRelease(int timeout)
    {
        _timeout = timeout;
        _enable_inactive_release = true;
    }

    // 启动服务器
    void Start()
    {
        _pool.Create();
        return _baseloop.Start();
    }

    void SetConnetedCallback(const ConnectedCallback &cb)
    {
        _connected_callback = cb;
    }
    void SetMessageCallback(const MessageCallback &cb)
    {
        _message_callback = cb;
    }
    void SetCloseCallback(const CloseCallback &cb)
    {
        _close_callback = cb;
    }
    void SetAnyEventCallback(const AnyEventCallback &cb)
    {
        _event_callback = cb;
    }

    void SetServerCloseCallback(const CloseCallback &cb)
    {
        _server_close_callback = cb;
    }
};
void Channel::Update()
{
    _loop->UpdateEvent(this);
}
void Channel::Remove()
{
    _loop->RemoveEvent(this);
}

void TimerWheel::TimerRefresh(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}

void TimerWheel::TimerAdd(uint64_t id, uint32_t timeout, const TaskFunc &cb)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, timeout, cb));
}

void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}

class NetWork
{
public:
    NetWork()
    {
        signal(SIGPIPE, SIG_IGN);
    }
};

static NetWork nw;
#endif