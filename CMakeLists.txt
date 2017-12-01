cmake_minimum_required(VERSION 3.9)
project(SocketsCPP)

set(CMAKE_CXX_STANDARD 11)

add_executable(SocketsCPP
        include/loguru/glog_bench/glog_bench.cpp
        include/loguru/glog_example/glog_example.hpp
        include/loguru/glog_example/main.cpp
        include/loguru/loguru_bench/loguru_bench.cpp
        include/loguru/loguru_example/loguru_example.hpp
        include/loguru/loguru_example/main.cpp
        include/loguru/test/loguru_test.cpp
        include/loguru/loguru.hpp
        LICENSE
        README.md
        sockets.cpp
        sockets.h)
