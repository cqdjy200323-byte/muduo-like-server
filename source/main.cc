#include"server.hpp"
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG
#define LOG(level,format, ...)do{\
        if(level<LOG_LEVEL) break;\
        time_t t = time(NULL);\
        struct tm *ltm = localtime(&t);\
        char tmp[32] = {0};\
        strftime(tmp, 31, "%H:%M:%S", ltm);\
        fprintf(stdout, "[%s %s:%d]" format "\n", tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
        }while(0)

#define INF_LOG(format,...) LOG(INF,format,##__VA_ARGS__)
#define DBG_LOG(format,...) LOG(DBG,format,##__VA_ARGS__)
#define ERR_LOG(format,...) LOG(ERR,format,##__VA_ARGS__)


int main()
{
    Buffer buf;
    for (int i = 0; i < 100;i++)
    {
        std::string str = "hello!!" + std::to_string(i) + '\n';
        buf.WriteStringAndPush(str);
    }

    while(buf.ReadableSpace()>0)
    {
        std::string line = buf.GetLinePop();
        DBG_LOG("%s",line.c_str());
    }
    return 0;
}