# muduo-like-server

仿照 [muduo](https://github.com/chenshuo/muduo) 实现的 C++17 多线程 TCP 网络库，并在其上实现了一个支持 keep-alive 的 HTTP/1.1 服务器。

- **主从 Reactor（one loop per thread）**：主线程只负责 accept，N 个 IO 线程各运行一个 EventLoop，连接建立后的所有读写都在固定的 IO 线程中完成，除任务队列外无需加锁
- **epoll（LT）+ 非阻塞 IO**，用户态输入 / 输出缓冲区
- **跨线程任务**：`RunInLoop` / `QueueInLoop` + eventfd 唤醒
- **时间轮定时器**：timerfd 驱动，`shared_ptr` 析构触发任务，用于空闲连接超时释放
- **连接生命周期管理**：`shared_ptr` + Channel 的 `weak_ptr` tie、延迟释放、跨线程任务绑定 `shared_from_this()`
- **HTTP/1.1**：状态机增量解析（正确处理粘包、半包），keep-alive，路由，解析上下文通过 `std::any` 挂在连接上，网络层不依赖协议层

## 架构

```
            ┌──────────── 主线程 ────────────┐
 connect ─▶ │ EventLoop：listenfd → Acceptor  │ ── NewConnection：轮询选择 IO 线程
            └────────────────────────────────┘
                 │ QueueInLoop + eventfd
      ┌──────────┼──────────┐
      ▼          ▼          ▼
  IO 线程 1   IO 线程 2   IO 线程 3      每个线程：EventLoop = epoll + 时间轮 + 任务队列
  连接 A、D   连接 B、E   连接 C、F      一个连接只在一个线程中处理
```

| 模块 | 职责 |
|---|---|
| `Buffer` | 读写下标 + 自动腾挪 / 扩容的字节缓冲区 |
| `Socket` / `Channel` / `Poller` | fd 封装 / fd + 关心的事件 + 回调 / epoll 封装 |
| `EventLoop` | 一轮循环：`epoll_wait` → 分发事件 → 执行任务队列 |
| `TimerWheel` | 60 格时间轮，空闲连接超时释放 |
| `Connection` | 一条 TCP 连接：收发缓冲区、状态、回调、上下文 |
| `Acceptor` / `LoopThreadPool` / `TcpServer` | 监听 / IO 线程池 / 对外接口 |
| `source/http/http.hpp` | `HttpRequest`、`HttpResponse`、解析状态机 `HttpContext`、`HttpServer` |

## 构建与运行

环境：Linux（依赖 epoll、eventfd、timerfd），CMake ≥ 3.13，支持 C++17 的编译器。

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/http_server            # 监听 8080

curl http://127.0.0.1:8080/hello
curl -d "abc123" http://127.0.0.1:8080/echo
```

调试版本（AddressSanitizer）：

```bash
cmake -B build-asan -DASAN=ON && cmake --build build-asan
```

业务代码示例（`example/http_main.cc`）：

```cpp
HttpServer server(8080);
server.Get("/hello", [](const HttpRequest &req, HttpResponse *resp) {
    resp->SetContent("hello, " + req.GetHeader("User-Agent") + "\n");
});
server.Start();
```

## 性能测试

**环境**：腾讯云 2 vCPU，Ubuntu 24.04，g++ 13.3，Release（-O3）；**wrk 与服务器运行在同一台机器上**（会争抢 CPU，结果偏保守）；服务器 3 个 IO 线程，日志输出丢弃；请求 `GET /hello`，响应约 110 字节。

| 场景 | QPS | 平均延迟 | P99 |
|---|---|---|---|
| 长连接，100 并发 | **约 10.9 万** | 0.83 ms | 3.55 ms |
| 长连接，10 并发 | 约 8.5 万 | 0.51 ms | 4.13 ms |
| 长连接，1000 并发 | 约 10.4 万 | 7.00 ms | 16.25 ms |
| 短连接（`Connection: close`），100 并发 | 约 2.0 万 | 2.43 ms | 6.38 ms |
| Debug 构建（-O0），长连接 100 并发 | 约 6.4 万 | 1.51 ms | 4.45 ms |

```bash
wrk -t2 -c100 -d10s --latency http://127.0.0.1:8080/hello
wrk -t2 -c100 -d10s --latency -H "Connection: close" http://127.0.0.1:8080/hello
```

**分析**

- 吞吐量在约 100 并发时达到饱和（约 11 万 QPS，CPU 已用满），继续增加并发只会增加排队延迟
- P99 主要来自线程数多于核数造成的调度排队：Release 相比 Debug 的 P50 减半，P99 基本不变
- 短连接吞吐约为长连接的 1/5：每个请求都要建连、accept、创建连接对象、注册 / 注销 epoll 与定时器并两次跨线程，且 accept 与连接表维护都集中在主线程
- 同步日志直接输出到终端时，短连接场景吞吐降至约 580 QPS 并出现超时，说明 IO 线程中不能有慢操作（改进方向：异步日志）

## 调试与修复记录

使用 AddressSanitizer、ThreadSanitizer、gdb 定位并修复的主要问题：

| 问题 | 定位手段 | 修复 |
|---|---|---|
| 连接在自身的事件处理中被析构，返回后访问已释放内存（use-after-free） | ASan + 短连接风暴脚本 `tools/conn_storm.py` | Channel `tie`（weak_ptr）+ 延迟释放 + 跨线程任务绑定 `shared_from_this()` |
| 修复上一个问题后，多线程下偶发 abort（读回调与错误回调各释放一次，第二次对已关闭的 fd 执行 `EPOLL_CTL_DEL`） | 退出码 134 → gdb 调用栈 | 释放操作幂等；Poller 只删除已注册的 fd |
| 取消的定时任务仍然执行，访问已析构的连接 | 日志时间线 | 析构时检查取消标志 |
| Buffer 单次写入超过剩余空间时 assert 失败（差一错误） | 单元测试 `example/buffer_test.cc` | `>` 改为 `>=`，长度统一为 `uint64_t` |
| `recv` 返回 0 时误读残留的 `errno`，可能导致对端关闭后连接不释放（LT 下空转、CLOSE_WAIT 堆积） | 代码审查 | 先判断返回值再看 `errno` |
| 请求行解析后未从缓冲区移除，被当成头部 | gdb 断点 | 使用 `GetLinePop` |

## 目录

```
source/server.hpp        网络库
source/http/http.hpp     HTTP 层
source/echo/             echo 服务器示例
example/                 HTTP 服务器入口、单元测试与学习用示例
tools/                   压测 / 复现脚本（短连接风暴、粘包半包测试）
```

## 已知不足

- 短连接场景下 accept 集中在主线程，可考虑 `SO_REUSEPORT` 多 Acceptor
- 日志为同步输出，可改为异步日志
- HTTP 仅支持 `Content-Length` 定长正文，未支持 chunked 编码与 HTTPS