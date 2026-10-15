#include "server.hpp"

int main()
{
    Buffer buf;
    for (int i = 0; i < 100; i++)
    {
        std::string str = "hello!!" + std::to_string(i) + '\n';
        buf.WriteStringAndPush(str);
    }

    while (buf.ReadableSpace() > 0)
    {
        std::string line = buf.GetLinePop();
        DBG_LOG("%s", line.c_str());
    }
    return 0;
}