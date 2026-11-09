#include "../source/http/http.hpp"

int main()
{
    HttpServer server(8080);
    server.Get("/hello", [](const HttpRequest &req, HttpResponse *resp)
               { resp->SetContent("hello, " + req.GetHeader("User-Agent") + "\n"); });
    server.Get("/", [](const HttpRequest &, HttpResponse *resp)
               { resp->SetContent("<h1>Hello from my muduo-like server!</h1>\n", "text/html; charset=utf-8"); });
    server.Post("/echo", [](const HttpRequest &req, HttpResponse *resp)
                { resp->SetContent("you sent " + std::to_string(req.body.size()) + " bytes: " + req.body + "\n"); });
    server.Start();
    return 0;
}