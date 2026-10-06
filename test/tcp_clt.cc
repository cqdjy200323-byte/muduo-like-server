#include"../source/server.hpp"

int main()
{
    Socket clt_sock;
    clt_sock.CreateClient(8080,"127.0.0.1");
    for(int i=0;i<5;i++)
    {
        std::string str="hello";
        clt_sock.Send(str.c_str(),str.size());
        //到此已近把消息发出去了，但未接受到消息
        char buf[1024]={0};
        clt_sock.Recv(buf,1023);

        DBG_LOG("%s",buf);
        sleep(1);
    }

    while(1)
    {
        sleep(1);
    }

    return 0;
}