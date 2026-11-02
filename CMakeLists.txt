cmake_minimum_required(VERSION 3.10)   # 要求的最低 cmake 版本
project(muduo_like CXX)                # 项目名，CXX 表示是 C++ 项目
set(CMAKE_CXX_STANDARD 11)             # 用 C++11 标准

add_executable(echo source/main.cc)    # 把 source/main.cc 编译成名叫 echo 的程序
target_link_libraries(echo pthread)    # echo 需要链接线程库
add_executable(test_server test/server.cc)
target_link_libraries(test_server pthread)  