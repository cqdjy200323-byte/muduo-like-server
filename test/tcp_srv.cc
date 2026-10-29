#include "../source/server.hpp"

EventLoop baseloop;
int conn_id = 1;
std::unordered_map<uint64_t, PtrConnection> conns;
LoopThreadPool *pool;
void ConnectionClose(const PtrConnection &conn)
{
    conns.erase(conn->Fd());
}

void OnConnected(const PtrConnection &conn)
{
    DBG_LOG("CONNECTED SUCESS ADDRESS:%p", conn.get());
}

void OnMessage(const PtrConnection &conn, Buffer *buf)
{
    DBG_LOG("%s", buf->ReadPosition());
    buf->MoveReadOffset(buf->ReadableSpace());
    std::string str = "hello world!";
    conn->Send(str.c_str(), str.size());
}

void Event(int fd)
{
    conn_id++;
    PtrConnection conn(new Connection(pool->NextLoop(), fd, conn_id));
    conn->SetServerCloseCallback(std::bind(ConnectionClose, std::placeholders::_1));
    conn->SetConnetedCallback(std::bind(OnConnected, std::placeholders::_1));
    conn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
    conn->EnableInactiveRelease(10);
    conn->Established();
    conns.insert(std::make_pair(conn_id, conn));
}

int main()
{
    pool = new LoopThreadPool(&baseloop);
    pool->SetThreadCount(2);
    pool->Create();
    Acceptor acceptor(&baseloop, 8080);
    acceptor.SetAcceptCallback(std::bind(Event, std::placeholders::_1));
    acceptor.Lisen();
    baseloop.Start();
    return 0;
}