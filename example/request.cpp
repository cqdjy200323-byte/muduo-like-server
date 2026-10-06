#include<iostream>
#include<string>
#include<regex>

int main()
{
//http请求格式：GET /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1\r\n
    std::string str="GET /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1\r\n";
    std::regex r("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?");
    //先把最前面的请求方式提取，其中|表示或，代表可以提取其中任意一个
    //[^?]匹配非问号字符
    //([01])表示提取0或者1

    std::smatch matchs;
    bool ret=std::regex_search(str,matchs,r);
    if(ret==false)
    {
        return -1;
    }
    for(auto &e : matchs)
    {
        std::cout<<e<<std::endl;
    }


    return 0;
}