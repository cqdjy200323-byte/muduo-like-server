#pragma once
#include "../server.hpp"
#include <functional>
#include <string>
#include <unordered_map>

// ======================== HttpRequest：解析出来的一个请求 ========================
struct HttpRequest
{
    std::string method;                                   // GET / POST ...
    std::string path;                                     // /hello
    std::string version;                                  // HTTP/1.1
    std::unordered_map<std::string, std::string> headers; // Host -> 127.0.0.1:8080
    std::string body;

    bool HasHeader(const std::string &key) const { return headers.count(key) > 0; }

    std::string GetHeader(const std::string &key) const
    {
        auto it = headers.find(key);
        return it == headers.end() ? "" : it->second; // 找不到就返回空字符串
    }

    size_t ContentLength() const
    {
        return HasHeader("Content-Length") ? std::stoul(GetHeader("Content-Length")) : 0;
    }

    bool Close() const
    {
        if (GetHeader("Connection") == "close")
            return true; // 客户端明确要求关闭
        if (version == "HTTP/1.0" && GetHeader("Connection") != "keep-alive")
            return true; // 1.0 默认是短连接
        return false;    // 1.1 默认是长连接
    }

    void Reset() { *this = HttpRequest(); }
};

struct HttpResponse
{
    int status = 200; // 默认就是 200
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    bool close = false; // 发完之后要不要关闭连接

    void SetContent(const std::string &content, const std::string &type = "text/plain; charset=utf-8")
    {
        body = content;
        headers["Content-Type"] = type; // 这里是要写入，用 [] 就是对的
    }

    static std::string StatusDesc(int status)
    {
        switch (status)
        {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 413:
            return "Payload Too Large";
        case 414:
            return "URI Too Long";
        default:
            return "Unknown";
        }
    }

    std::string ToString() const
    {
        std::string s = "HTTP/1.1 " + std::to_string(status) + " " + StatusDesc(status) + "\r\n"; // 状态行
        for (auto &h : headers)
            s += h.first + ": " + h.second + "\r\n";                    // 每个头部一行
        s += "Content-Length: " + std::to_string(body.size()) + "\r\n"; // 正文长度由程序自动计算
        s += std::string("Connection: ") + (close ? "close" : "keep-alive") + "\r\n";
        s += "\r\n"; // 空行：头部结束
        s += body;   // 正文
        return s;
    }
};

enum HttpRecvState
{
    RECV_LINE, // 正在等 / 解析请求行
    RECV_HEAD, // 正在解析头部
    RECV_BODY, // 正在接收正文
    RECV_OVER, // 一个完整的请求已经解析完
    RECV_ERROR // 请求格式错误
};

#define MAX_LINE 8192 // 一行最长 8KB

class HttpContext
{
public:
    HttpRecvState state = RECV_LINE; // 当前解析到哪一步了
    int error_status = 200;          // 出错时要回的状态码
    HttpRequest req;                 // 已经解析出来的内容，存在这里

    void Reset()
    {
        state = RECV_LINE;
        error_status = 200;
        req.Reset();
    }

    void Parse(Buffer *buf)
    {
        if (state == RECV_LINE)
            ParseLine(buf);
        if (state == RECV_HEAD)
            ParseHead(buf);
        if (state == RECV_BODY)
            ParseBody(buf);
    }

private:
    // 去掉行尾的 \r\n（或者只有 \n）
    static void TrimCRLF(std::string &line)
    {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
    }

    void SetError(int status)
    {
        state = RECV_ERROR;
        error_status = status;
    }
    void ParseLine(Buffer *buf)
    {
        if (buf->FindCRLF() == NULL)
        {
            if (buf->ReadableSpace() > MAX_LINE)
            {
                SetError(414);
            }
            return;
        }
        std::string line = buf->GetLinePop();
        TrimCRLF(line);

        // ③ 按两个空格切成三段
        size_t p1 = line.find(' ');                                                         // 第一个空格的位置
        size_t p2 = (p1 == std::string::npos) ? std::string::npos : line.find(' ', p1 + 1); // 第二个空格的位置
        if (p2 == std::string::npos)
            return SetError(400); // 不够两个空格：格式不对

        req.method = line.substr(0, p1);             // 从开头到第一个空格之前
        req.path = line.substr(p1 + 1, p2 - p1 - 1); // 两个空格中间的部分
        req.version = line.substr(p2 + 1);           // 第二个空格之后的全部

        // ④ 只支持 HTTP/1.0 和 HTTP/1.1
        if (req.version != "HTTP/1.1" && req.version != "HTTP/1.0")
            return SetError(400);

        state = RECV_HEAD; // 请求行解析完了，进入下一个状态
    }

    void ParseHead(Buffer *buf)
    {
        while (true)
        {
            // ① 这一行还没收全：先等（同样要防止超长行）
            if (buf->FindCRLF() == NULL)
            {
                if (buf->ReadableSpace() > MAX_LINE)
                    SetError(400);
                return;
            }

            // ② 取出一行，去掉行尾
            std::string line = buf->GetLinePop();
            TrimCRLF(line);

            // ③ 空行：头部结束了，进入下一个状态
            if (line.empty())
            {
                state = RECV_BODY;
                return;
            }

            // ④ 普通的头部行："Host: test" → 按 ": " 切成名字和值
            size_t pos = line.find(": ");
            if (pos == std::string::npos)
                return SetError(400);
            req.headers[line.substr(0, pos)] = line.substr(pos + 2); // pos + 2：跳过冒号和空格这两个字符
        }
    } // 第 3 步再写
    void ParseBody(Buffer *buf)
    {
        size_t need = req.ContentLength() - req.body.size(); // 还差多少字节

        if (buf->ReadableSpace() >= need) // ① 够了：只取需要的那一部分
        {
            req.body += buf->ReadStringPop(need);
            state = RECV_OVER;
        }
        else // ② 不够：先把现有的全部拿走，等下次
        {
            req.body += buf->ReadStringPop(buf->ReadableSpace());
        }
    }
};

class HttpServer
{
public:
    // 处理函数的类型：输入一个请求，往 resp 里填写响应
    using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;

    HttpServer(int port, int threads = 3) : _server(port)
    {
        _server.SetThreadCount(threads);
        _server.EnableInactiveRelease(30); // 空闲 30 秒的连接自动关闭
        _server.SetConnetedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }

    // 注册路由："GET /hello" → 处理函数
    void Get(const std::string &path, const Handler &h) { _routes["GET " + path] = h; }
    void Post(const std::string &path, const Handler &h) { _routes["POST " + path] = h; }
    void Start() { _server.Start(); }

private:
    void OnConnected(const PtrConnection &conn)
    {
        conn->SetContext(HttpContext()); // 每个连接都有一个独立的解析器，存在它的 _context 里
    }

    void OnMessage(const PtrConnection &conn, Buffer *buf)
    {
        while (buf->ReadableSpace() > 0) // 粘包：Buffer 里可能不止一个请求，循环处理
        {
            // ① 取出这个连接的解析器，接着上次的进度解析
            HttpContext *ctx = std::any_cast<HttpContext>(conn->GetContext());
            if (ctx == nullptr)
            {
                ERR_LOG("context is not HttpContext, OnConnected not called?");
                conn->ShutDown();
                return;
            }
            ctx->Parse(buf);

            // ② 格式错误：回一个错误响应，然后关闭连接
            if (ctx->state == RECV_ERROR)
            {
                HttpResponse resp;
                resp.status = ctx->error_status;
                resp.close = true;
                resp.SetContent(std::to_string(resp.status) + " " + HttpResponse::StatusDesc(resp.status) + "\n");
                SendResponse(conn, resp);
                buf->MoveReadOffset(buf->ReadableSpace()); // 剩下的数据都不要了
                conn->ShutDown();
                return;
            }

            // ③ 半包：还没解析完一个请求，等下次数据到达
            if (ctx->state != RECV_OVER)
                return;

            // ④ 解析出了一个完整的请求：路由 → 生成响应 → 发送
            HttpResponse resp;
            resp.close = ctx->req.Close();
            Route(ctx->req, &resp);
            SendResponse(conn, resp);

            // ⑤ keep-alive：重置解析器，准备解析下一个请求
            bool close = resp.close;
            ctx->Reset();
            if (close)
            {
                conn->ShutDown();
                return;
            }
            // 没有 return：回到 while 开头，如果 Buffer 里还有数据（粘包），就接着解析下一个请求
        }
    }
    void Route(const HttpRequest &req, HttpResponse *resp)
    {
        auto it = _routes.find(req.method + " " + req.path);
        if (it == _routes.end()) // 没有注册过这个路径
        {
            resp->status = 404;
            resp->SetContent("404 Not Found: " + req.path + "\n");
            return;
        }
        it->second(req, resp); // 调用业务层注册的处理函数
    }

    void SendResponse(const PtrConnection &conn, const HttpResponse &resp)
    {
        std::string s = resp.ToString();
        conn->Send(s.c_str(), s.size());
    }

    TcpServer _server;
    std::unordered_map<std::string, Handler> _routes;
};