cmake_minimum_required(VERSION 3.9)
project(SocketsCPP)
set(CMAKE_CXX_STANDARD 11)

include(ExternalProject)
find_package(Threads)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

if(${SOCKETS_EXTERNAL_LOGURU})
else()
    add_definitions(-DCOMP_LOGURU)
endif()

set(COMPILE_PROTOBUF FALSE)
if (${COMPILE_PROTOBUF})
    add_definitions(-DPROTOBUF_SUPPORT)
    find_package(Protobuf REQUIRED)
endif ()

include_directories(${PROJECT_SOURCE_DIR}/include)

# loguru
set(LOGURU_ROOT ${PROJECT_SOURCE_DIR}/include/loguru)
ExternalProject_Add(loguru
        #PREFIX ${LOGURU_ROOT}
        GIT_REPOSITORY git@github.com:emilk/loguru.git
        #GIT_TAG origin/master
        GIT_PROGRESS TRUE
        SOURCE_DIR ${LOGURU_ROOT}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        )


set(SRC sockets.cpp sockets.h)
add_library(SocketsCPP ${SRC})
add_dependencies(SocketsCPP loguru)

if (${COMPILE_PROTOBUF})
    target_link_libraries(SocketsCPP dl ${CMAKE_THREADS_LIB_INIT} ${PROTOBUF_LIBRARY})
else ()
    target_link_libraries(SocketsCPP dl ${CMAKE_THREADS_LIB_INIT})
endif ()
