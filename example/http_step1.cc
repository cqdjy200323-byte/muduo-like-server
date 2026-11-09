#include "../source/http/http.hpp"
#include <cstdio>

// 把 \r\n 显示出来，方便看清报文的真实样子
void Show(const std::string &s)
{
    for (char c : s)
    {
        if (c == '\r')
            printf("\\r");
        else if (c == '\n')
            printf("\\n\n");
        else
            putchar(c);
    }
    printf("\n");
}

int main()
{
    HttpResponse resp;
    resp.SetContent("hello world!");
    printf("===== 1. 一个 200 响应 =====\n");
    Show(resp.ToString());

    HttpResponse r404;
    r404.status = 404;
    r404.close = true;
    r404.SetContent("<h1>not found</h1>", "text/html; charset=utf-8");
    printf("===== 2. 一个 404 响应，并且要求关闭连接 =====\n");
    Show(r404.ToString());

    HttpRequest req;
    req.version = "HTTP/1.1";
    printf("===== 3. 什么时候关闭连接 =====\n");
    printf("HTTP/1.1，没写 Connection         → Close() = %d\n", req.Close());
    req.headers["Connection"] = "close";
    printf("HTTP/1.1，Connection: close       → Close() = %d\n", req.Close());
    req.Reset();
    req.version = "HTTP/1.0";
    printf("HTTP/1.0，没写 Connection         → Close() = %d\n", req.Close());
    req.headers["Connection"] = "keep-alive";
    printf("HTTP/1.0，Connection: keep-alive  → Close() = %d\n", req.Close());
    return 0;
}