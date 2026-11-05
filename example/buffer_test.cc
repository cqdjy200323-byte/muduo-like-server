#include "../source/server.hpp"

int main()
{
    Buffer buf;
    std::string big(5000, 'a');   // 5000 个字符 'a'
    DBG_LOG("准备写入 %zu 字节", big.size());
    buf.WriteStringAndPush(big);
    DBG_LOG("写入成功，可读 %lu 字节", buf.ReadableSpace());
    return 0;
}