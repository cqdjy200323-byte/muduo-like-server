#include "../server.hpp"

class EchoServer
{
private:
    TcpServer _server;

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

public:
    EchoServer(int port)
        : _server(port)
    {
        _server.SetThreadCount(3);
        _server.SetConnetedCallback(std::bind(&EchoServer::OnConnected, this, std::placeholders::_1));
        _server.SetCloseCallback(std::bind(&EchoServer::OnClose, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&EchoServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }

    void Start()
    {
        return _server.Start();
    }
};
