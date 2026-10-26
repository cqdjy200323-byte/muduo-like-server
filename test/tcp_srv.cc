#include "../source/server.hpp"

EventLoop baseloop;
std::vector<LoopThread> Threads(2);
std::unordered_map<uint64_t, PtrConnection> conns;
int nextloop = 0;
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
    uint64_t conn_id = rand() % 10000;
    nextloop = (nextloop + 1) % 2;
    PtrConnection conn(new Connection(Threads[nextloop].GetLoop(), fd, conn_id));
    conn->SetServerCloseCallback(std::bind(ConnectionClose, std::placeholders::_1));
    conn->SetConnetedCallback(std::bind(OnConnected, std::placeholders::_1));
    conn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
    conn->EnableInactiveRelease(10);
    conn->Established();
    conns.insert(std::make_pair(conn_id, conn));
}

int main()
{
    srand(time(NULL));
    Acceptor acceptor(&baseloop, 8080);
    acceptor.SetAcceptCallback(std::bind(Event, std::placeholders::_1));
    acceptor.Lisen();
    while (1)
    {
        baseloop.Start();
    }
    return 0;
}