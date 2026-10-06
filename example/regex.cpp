#include <iostream>
#include <string>
#include <regex>

int main()
{
    std::string str="/number/1234";
    std::regex e("/number/(\\d+)");
    std::smatch matches;

    bool ret=std::regex_match(str,matches,e);
    if(ret==false)
    {
        return -1;
    }
    for(auto &s : matches)
    {
        std::cout<<s<<std::endl;
    }
    
    return 0;
}