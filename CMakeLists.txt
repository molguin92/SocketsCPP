cmake_minimum_required(VERSION 3.9)
project(SocketsCPP)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

if(${SOCKETS_EXTERNAL_LOGURU})
else()
    add_definitions(-DCOMP_LOGURU)
endif()


set(COMPILE_PROTOBUF TRUE)
if (${COMPILE_PROTOBUF})
    add_definitions(-DPROTOBUF_SUPPORT)
    find_package(Protobuf REQUIRED)
endif ()

include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(Threads)

set(CMAKE_CXX_STANDARD 11)
set(SRC sockets.cpp sockets.h)

add_library(SocketsCPP ${SRC})

if (${COMPILE_PROTOBUF})
    target_link_libraries(SocketsCPP dl ${CMAKE_THREADS_LIB_INIT} ${PROTOBUF_LIBRARY})
else ()
    target_link_libraries(SocketsCPP dl ${CMAKE_THREADS_LIB_INIT})
endif ()
