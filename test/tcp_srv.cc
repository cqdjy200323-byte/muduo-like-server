#include"../source/server.hpp"


void HandleClose(Channel* channel)
{
    DBG_LOG("id:%d",channel->Fd());
    channel->Remove();
    delete channel;
}

void HandleRead(Channel* channel)
{
    int fd=channel->Fd();
    char buf[1024]={0};
    int ret=recv(fd,buf,1023,0);
    if(ret<=0)
    {
        return HandleClose(channel);
    }
    DBG_LOG("%s",buf);
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

void HandleErvent(EventLoop* loop,Channel* channel,uint64_t id)
{
   loop->TimerRefresh(id);
}




void Acceptor(EventLoop* loop,Channel* lst_channel)
{
    uint64_t id=rand()%10000;
    int fd=lst_channel->Fd();
    int newfd=accept(fd,NULL,NULL);
    SetNonBlock(newfd);
    if(newfd<0)
    {
        return;
    }
    Channel* channel=new Channel(loop,newfd);
    channel->SetReadCallback(std::bind(HandleRead,channel));
    channel->SetWriteCallback(std::bind(HandleWrite,channel));
    channel->SetCloseCallback(std::bind(HandleClose,channel));
    channel->SetEventCallback(std::bind(HandleErvent,loop,channel,id));
    loop->TimerAdd(id,10,std::bind(HandleClose,channel));

    channel->EnableRead();

    

}

int main()
{
    srand(time(NULL));
    Socket lst_sock;
    EventLoop loop;
    lst_sock.CreateServer(8080);
    Channel channel(&loop,lst_sock.Fd());
    channel.SetReadCallback(std::bind(Acceptor,&loop,&channel));
    channel.EnableRead();
    while(1)
    {
        loop.Start();

    }
    lst_sock.Close();
    return 0;
}