cmake_minimum_required(VERSION 3.9)
project(SocketsCPP)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(CMAKE_CXX_STANDARD 11)
set(SRC sockets.cpp sockets.h)

add_library(SocketsCPP ${SRC})
