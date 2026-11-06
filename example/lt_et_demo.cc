#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cerrno>

// 用法：./lt_et_demo lt   或   ./lt_et_demo et
int main(int argc, char *argv[])
{
    bool et = (argc > 1 && strcmp(argv[1], "et") == 0);
    printf("模式：%s\n", et ? "ET（边沿触发）" : "LT（水平触发）");

    int fds[2];
    pipe(fds);                          // fds[0] 读端，fds[1] 写端
    fcntl(fds[0], F_SETFL, O_NONBLOCK); // 读端设为非阻塞

    int epfd = epoll_create(1);
    struct epoll_event ev;
    ev.events = EPOLLIN | (et ? EPOLLET : 0); // 关键区别就在这里
    ev.data.fd = fds[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, fds[0], &ev);

    write(fds[1], "hello world", 11); // 一次写进 11 个字节
    printf("写入了 11 个字节：hello world\n");

    bool wrote_again = false;
    for (int round = 1; round <= 6; round++)
    {
        struct epoll_event out;
        int n = epoll_wait(epfd, &out, 1, 1000); // 最多等 1 秒
        if (n == 0)
        {
            printf("第 %d 轮：等了 1 秒，没有事件\n", round);
            if (!wrote_again)
            {
                write(fds[1], "!", 1); // 第一次没等到事件时，再写 1 个字节
                printf("        又写入了 1 个字节：!\n");
                wrote_again = true;
            }
            continue;
        }
        if (et)
        {
            while (true)
            {
                char buf[5] = {0};
                int ret = read(fds[0], buf, 4);
                if (ret > 0)
                {
                    // 读到了数据：打印出来，然后继续读
                    printf("第 %d 轮：有事件，读到 %d 字节：\"%s\"\n", round, ret, buf);
                }
                else if (ret < 0 && errno == EAGAIN)
                {
                    // 读空了：打印一句"读空了"，跳出循环
                    printf("读空了");
                    break;
                }
                else
                {
                    if (ret == 0)
                        printf("        写端已关闭（EOF）\n");
                    else
                        printf("        出错了：%s\n", strerror(errno));
                    break;
                }
            }
        }
        else
        {
            char buf[5] = {0};
            int ret = read(fds[0], buf, 4); // 故意每次只读 4 个字节
            printf("第 %d 轮：有事件，读到 %d 字节：\"%s\"\n", round, ret, buf);
        }
    }
    return 0;
}