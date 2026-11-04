import socket, random

# 反复建立短连接：连上 → 有一半概率发点数据 → 马上断开
for i in range(5000):
    try:
        s = socket.create_connection(("127.0.0.1", 8080))
        if random.random() < 0.5:
            s.send(b"hi")
        s.close()
    except Exception as e:
        print("第", i, "次连接失败：", e)
        break
print("结束")