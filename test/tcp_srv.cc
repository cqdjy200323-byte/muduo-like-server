#include "../source/server.hpp"

std::unordered_map<uint64_t, PtrConnection> conns;
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

void Acceptor(EventLoop *loop, Channel *lst_channel)
{
    uint64_t conn_id = rand() % 10000;
    int fd = lst_channel->Fd();
    int newfd = accept(fd, NULL, NULL);
    SetNonBlock(newfd);
    if (newfd < 0)
    {
        return;
    }
    Buffer buf;
    PtrConnection conn(new Connection(loop, newfd, conn_id));
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
    Socket lst_sock;
    EventLoop loop;
    lst_sock.CreateServer(8080);
    Channel channel(&loop, lst_sock.Fd());
    channel.SetReadCallback(std::bind(Acceptor, &loop, &channel));
    channel.EnableRead();
    while (1)
    {
        loop.Start();
    }
    lst_sock.Close();
    return 0;
}