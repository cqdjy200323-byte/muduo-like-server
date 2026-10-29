#include "../source/server.hpp"

void OnClose(const PtrConnection &conn)
{
    DBG_LOG("CLOSE SUCESS ADDRESS:%p", conn.get());
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
int main()
{
    TcpServer server(8080);
    server.SetThreadCount(2);
    server.EnableInacticeRelease(10);
    server.SetConnetedCallback(OnConnected);
    server.SetCloseCallback(OnClose);
    server.SetMessageCallback(OnMessage);
    server.Start();
    return 0;
}