#include"../source/server.hpp"


void HandleClose(Channel* channel)
{
    std::cout<<"close:"<<channel->Fd()<<std::endl;
    channel->Remove();
    delete channel;
}

void HandleRead(Channel* channel)
{
    DBG_LOG("ceshi");
    int fd=channel->Fd();
    char buf[1024]={0};
    int ret=recv(fd,buf,1023,0);
    if(ret<=0)
    {
        return HandleClose(channel);
    }
    std::cout<<buf<<std::endl;
    channel->EnableWrite();
}

void HandleWrite(Channel* channel)
{
    int fd=channel->Fd();
    const char* data="hello codex";
    int ret = send(fd,data,strlen(data),0);
    if(ret<0)
    {
        return HandleClose(channel);
    }
    channel->DisableWrite();
    
}

void HandleErvent(Channel* channel)
{
    std::cout<<"任意事件被触发"<<std::endl;
}




void Acceptor(Poller* poller,Channel* lst_channel)
{
    int fd=lst_channel->Fd();
    int newfd=accept(fd,NULL,NULL);
    SetNonBlock(newfd);
    if(newfd<0)
    {
        return;
    }
    Channel* channel=new Channel(poller,newfd);
    channel->SetReadCallback(std::bind(HandleRead,channel));
    channel->SetWriteCallback(std::bind(HandleWrite,channel));
    channel->SetCloseCallback(std::bind(HandleClose,channel));
    channel->SetEventCallback(std::bind(HandleErvent,channel));
    channel->EnableRead();
}

int main()
{
    Socket lst_sock;
    Poller poller;
    lst_sock.CreateServer(8080);
    Channel channel(&poller,lst_sock.Fd());
    channel.SetReadCallback(std::bind(Acceptor,&poller,&channel));
    channel.EnableRead();
    while(1)
    {
        DBG_LOG("duandian");
        std::vector<Channel*> actives;
        DBG_LOG("duandian");

        poller.poll(&actives);
        DBG_LOG("duandian");

        for(auto e: actives)
        {
            e->HandleEvent();
        }
        DBG_LOG("duandian");

    }
    lst_sock.Close();
    return 0;
}