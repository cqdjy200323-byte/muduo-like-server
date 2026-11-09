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

// 把 \r \n 换成看得见的文字，最多显示前 20 个字符
std::string Esc(const std::string &s)
{
    std::string out;
    for (char c : s.substr(0, 20))
        out += (c == '\r') ? "\\r" : (c == '\n') ? "\\n"
                                                 : std::string(1, c);
    return s.size() > 20 ? out + "..." : out;
}

// 往一个新的解析器里，分几次喂数据，每喂一次就解析一次，打印状态
void Feed(const char *title, std::initializer_list<std::string> pieces)
{
    printf("===== %s =====\n", title);
    HttpContext ctx;
    Buffer buf;
    for (auto &p : pieces)
    {
        buf.WriteStringAndPush(p);
        ctx.Parse(&buf);
        printf("  喂入 %-26s → 状态 %-10s", ("\"" + Esc(p) + "\"").c_str(), StateName(ctx.state));
        if (ctx.state == RECV_ERROR)
            printf(" 错误码 %d", ctx.error_status);
        printf("\n");
    }
    if (ctx.state != RECV_LINE && ctx.state != RECV_ERROR)
        printf("  解析结果：method=[%s] path=[%s] version=[%s]\n",
               ctx.req.method.c_str(), ctx.req.path.c_str(), ctx.req.version.c_str());
}

int main()
{
    Feed("1. 请求行一次到齐", {"GET /hello HTTP/1.1\r\n"});
    Feed("2. 请求行分两次到（半包）", {"GET /hel", "lo HTTP/1.1\r\n"});
    Feed("3. 格式不对：缺空格", {"HELLO\r\n"});
    Feed("4. 不支持的版本", {"GET / HTTP/2.0\r\n"});
    Feed("5. 超长且没有换行", {std::string(9000, 'a')});
    return 0;
}