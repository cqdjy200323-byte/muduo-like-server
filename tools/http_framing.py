import socket, time

s = socket.create_connection(("127.0.0.1", 8080))

# A：粘包 —— 两个完整的请求，用一次 send 发出去
s.sendall(b"GET /hello HTTP/1.1\r\nHost: t\r\n\r\nGET /nope HTTP/1.1\r\nHost: t\r\n\r\n")
time.sleep(0.3)
print("A 粘包:", s.recv(4096).decode().count("HTTP/1.1"), "个响应")

# B：半包 —— 一个 POST 请求切成 3 段发，连 Content-Length 这个单词都被切断
s.sendall(b"POST /echo HTTP/1.1\r\nHost: t\r\nContent-Le")
time.sleep(0.3)
s.sendall(b"ngth: 5\r\n\r\nhe")
time.sleep(0.3)
s.sendall(b"llo")
time.sleep(0.3)
print("B 半包:", s.recv(4096).decode().splitlines()[-1])

s.close()