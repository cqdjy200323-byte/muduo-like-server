cmake_minimum_required(VERSION 3.13)   # 要求的最低 cmake 版本
project(muduo_like CXX)                # 项目名，CXX 表示是 C++ 项目
set(CMAKE_CXX_STANDARD 17)             # 用 C++11 标准
add_compile_options(-Wall -Wextra)
option(ASAN "开启 AddressSanitizer" OFF)   # 定义一个开关，默认关闭
if(ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address)
endif()
add_executable(echo source/main.cc)    # 把 source/main.cc 编译成名叫 echo 的程序
target_link_libraries(echo pthread)    # echo 需要链接线程库
add_executable(test_server test/server.cc)
target_link_libraries(test_server pthread) 
add_executable(asan_demo example/asan_demo.cc)
add_executable(weak_ptr_demo example/weak_ptr_demo.cc)
add_executable(ctor_demo example/ctor_demo.cc)
add_executable(buffer_test example/buffer_test.cc)
target_link_libraries(buffer_test pthread)  
add_executable(lt_et_demo example/lt_et_demo.cc)

add_executable(http_server example/http_main.cc)
target_link_libraries(http_server pthread)