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
static void SetNonBlock(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}


#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG
#define LOG(level,format, ...)do{\
        if(level<LOG_LEVEL) break;\
        time_t t = time(NULL);\
        struct tm *ltm = localtime(&t);\
        char tmp[32] = {0};\
        strftime(tmp, 31, "%H:%M:%S", ltm);\
        fprintf(stdout, "[%s %s:%d]" format "\n", tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
        }while(0)

#define INF_LOG(format,...) LOG(INF,format,##__VA_ARGS__)
#define DBG_LOG(format,...) LOG(DBG,format,##__VA_ARGS__)
#define ERR_LOG(format,...) LOG(ERR,format,##__VA_ARGS__)


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
    :_sockfd(-1)
    {}

    Socket(int fd)
    :_sockfd(fd)
    {}

    bool Create()
    {
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(_sockfd<0)
        {
            ERR_LOG("CREATE SOCKET FAILED!!");
            return false;
        }
        return true;
    }

    bool Bind(const std::string& ip,uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
        if(ret<0)
        {
            ERR_LOG("BIND ADDRESS FAILED!!");
            return false;
        }
        return true;
    }
    bool Listen(int backlog=Max_LISTEN)
    {
         int ret = listen(_sockfd, backlog);
            if (ret < 0) {
                ERR_LOG("SOCKET LISTEN FAILED!");
                return false;
            }
            return true;
    }

    bool Connect(const std::string& ip,uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
        if(ret<0)
        {
            ERR_LOG("SOCKET CONNECT FAILED!!");
            return false;
        }
        return true;
    }

    int Accept()
    {
        int newfd = accept(_sockfd, NULL, NULL);
        if(newfd<=0)
        {
            if(errno==EAGAIN||errno==EINTR)
            {
                return 0;
            }
            return -1;
        }
        return newfd;
    }

    ssize_t Recv(void* buf,size_t len,int flag=0)
    {
        ssize_t ret = recv(_sockfd, buf, len, flag);
        if(ret<=0)
        {
            if(errno==EAGAIN||errno==EINTR)
            {
                return 0;
            }
            ERR_LOG("SOCKET RECV FAILED!!");
            return -1;
        }
        return ret;
    }

    ssize_t NonBlockReve(void* buf,size_t len)
    {
        return Recv(buf, len, MSG_DONTWAIT);
    }

    ssize_t Send(const void* buf,size_t len,int flag=0)
    {
        ssize_t ret = send(_sockfd, buf, len, flag);
        if(ret<=0)
        {
            if(errno==EAGAIN||errno==EINTR)
            {
                return 0;
            }
            ERR_LOG("SOCKET SEND FAILED!!");
            return -1;
        }
        return ret;
    }
    ssize_t NonBlockSend(void* buf,size_t len)
    {
        return Send(buf, len, MSG_DONTWAIT);
    }

    void Close()
    {
        if(_sockfd!=-1)
        {
            close(_sockfd);
            _sockfd = -1;  
        }
    }

    bool CreateServer(uint16_t port,const std::string &ip="0.0.0.0")
    {
        //1.创建套接字,设置非阻塞
        if(Create()==false)
            return false;
        NonBlock();

        // 4.启动地址重用
        ReuseAddress();
        // 2.绑定地址
        if(Bind(ip,port)==false)
            return false;
        // 3.开始监听
        if(Listen()==false)
            return false;
        
        return true;
    }
    bool CreateClient(uint16_t port, const std::string &ip) {
            //1. 创建套接字，2.指向连接服务器
            if (Create() == false) return false;
            if (Connect(ip, port) == false) return false;
            return true;
        }

    //设置套接字选项--开启地址端口重用
    void ReuseAddress()
    {
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
        val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
    }

    //设置非阻塞
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
class Channel
{
private:
    int _fd;
    uint32_t _events;  // 当前需要监控的事件
    uint32_t _revents; //当前需要连续触发的事件
    Poller *_poller;
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;
    EventCallback _write_callback;
    EventCallback _error_callback;
    EventCallback _close_callback;
    EventCallback _event_callback;

public:
    Channel(Poller* poller,int fd)
    :_fd(fd)
    ,_events(0)
    ,_revents(0)
    ,_poller(poller)
    {}

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
        if((_revents&EPOLLIN)||(_revents&EPOLLRDHUP)||(_revents&EPOLLPRI))
        {
            if(_event_callback)
                _event_callback();
            if(_read_callback)
                _read_callback();
            
        }

        if(_revents&EPOLLOUT)
        {
            if(_event_callback)
                _event_callback();
            if(_write_callback)
                _write_callback();
        }
        else if(_revents&EPOLLERR)
        {
            if(_error_callback)
                _error_callback();
        }
        else if(_revents&EPOLLHUP)
        {
            if(_close_callback)
            _close_callback();
        }
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
    void Update(Channel* channel,int op)
    {
        //int epoll_ctl(int epfd,int op,int fd,struct epoll_evevt* ev)
        int fd = channel->Fd();
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->Events();
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if(ret<0)
        {
            ERR_LOG("EPOLLCTL FAILED");
            abort();
        }
    }

    //判断Channel是否被监控
    bool HasChannel(Channel* channel)
    {
        auto it = _channels.find(channel->Fd());
        if(it==_channels.end())
        {
            return false;
        }
        return true;
    }

public:
    Poller() 
    {
            _epfd = epoll_create(MAX_EPOLLEVENTS);
            if (_epfd < 0) {
                ERR_LOG("EPOLL CREATE FAILED!!");
                abort();//退出程序
            }
        }
    //添加或修改监控事件
    void UpdateEvent(Channel* channel)
    {
        bool ret = HasChannel(channel);
        if(ret==false)
        {
            _channels.insert(std::make_pair(channel->Fd(),channel));
            return Update(channel, EPOLL_CTL_ADD);
        }
        return Update(channel, EPOLL_CTL_MOD);
    }

    void RemoveEvent(Channel* channel)
    {
        auto it = _channels.find(channel->Fd());
        if(it!=_channels.end())
        {
            _channels.erase(it);
        }
        Update(channel, EPOLL_CTL_DEL);
    }

    //开始监控
    void poll(std::vector<Channel*> *active)
    {
        //int epoll-wait(int epfd ,struct epoll_event*evs ,int maxevents ,int timeout)
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1);
        if(nfds<0)
        {
            if(errno==EINTR)
            {
                return;
            }
            ERR_LOG("EPOLL WAIT ERROR:%s\n", strerror(errno));
            abort();
        }
        for (int i = 0; i < nfds;i++)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());
            it->second->SetREvents(_evs[i].events);//设置实际就绪的事件
            active->push_back(it->second);
        }
        return;
    }
};


void Channel::Update()
{
    _poller->UpdateEvent(this);
}
void Channel::Remove()
{
    _poller->RemoveEvent(this);
}


class EventLoop
{
private:
    //首先得有个刷新事件的eventfd
    int _event_fd;
    //管理event_fd的channel
    std::unique_ptr<Channel*> _event_channel;
    //任务队列
    using Functor=std::function<void()>;
    std::vector<Functor> _task;
    //事件触发的监控
    Poller* poller;
    //对任务队列操作时的锁
    std::mutex _mutex;

};