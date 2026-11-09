#include "../source/http/http.hpp"
#include <cstdio>

const char *StateName(HttpRecvState s)
{
    switch (s)
    {
    case RECV_LINE:
        return "RECV_LINE";
    case RECV_HEAD:
        return "RECV_HEAD";
    case RECV_BODY:
        return "RECV_BODY";
    case RECV_OVER:
        return "RECV_OVER";
    default:
        return "RECV_ERROR";
    }
}

std::string Esc(const std::string &s)
{
    std::string out;
    for (char c : s.substr(0, 22))
        out += (c == '\r') ? "\\r" : (c == '\n') ? "\\n"
                                                 : std::string(1, c);
    return s.size() > 22 ? out + "..." : out;
}

void Feed(const char *title, std::initializer_list<std::string> pieces)
{
    printf("===== %s =====\n", title);
    HttpContext ctx;
    Buffer buf;
    for (auto &p : pieces)
    {
        buf.WriteStringAndPush(p);
        ctx.Parse(&buf);
        printf("  喂入 %-30s → %-10s", ("\"" + Esc(p) + "\"").c_str(), StateName(ctx.state));
        if (ctx.state == RECV_ERROR)
            printf(" 错误码 %d", ctx.error_status);
        printf("\n");
    }
    if (ctx.state == RECV_OVER)
    {
        printf("  请求：%s %s %s\n", ctx.req.method.c_str(), ctx.req.path.c_str(), ctx.req.version.c_str());
        for (auto &h : ctx.req.headers)
            printf("  头部：[%s] = [%s]\n", h.first.c_str(), h.second.c_str());
        printf("  正文：[%s]（%zu 字节）\n", Esc(ctx.req.body).c_str(), ctx.req.body.size());
    }
    printf("  解析完后 Buffer 里还剩 %lu 字节：\"%s\"\n", buf.ReadableSpace(),
           Esc(buf.ReadString(buf.ReadableSpace())).c_str());
}

int main()
{
    Feed("1. GET：头部分三次到",
         {"GET /hello HTTP/1.1\r\nHo", "st: test\r\nUser-Agent: cu", "rl\r\n\r\n"});
    Feed("2. POST：正文分两次到",
         {"POST /echo HTTP/1.1\r\nContent-Length: 6\r\n\r\nabc", "123"});
    Feed("3. 粘包：一个 POST 后面紧跟着下一个请求的开头",
         {"POST /echo HTTP/1.1\r\nContent-Length: 3\r\n\r\nabcGET /next HTTP/1.1\r\n"});
    Feed("4. 头部格式不对：没有冒号",
         {"GET / HTTP/1.1\r\nBadHeader\r\n\r\n"});
    return 0;
}
