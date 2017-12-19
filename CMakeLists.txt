cmake_minimum_required(VERSION 3.9)
project(SocketsCPP VERSION 0.5.0 DESCRIPTION "OOP wrapper around C socket API.")
set(CMAKE_CXX_STANDARD 14)

include(ExternalProject)
find_package(Threads)

#set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
#set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

if (${SOCKETS_EXTERNAL_LOGURU})
else ()
    add_definitions(-DCOMP_LOGURU)
endif ()

#set(COMPILE_PROTOBUF FALSE)
if (${COMPILE_PROTOBUF})
    add_definitions(-DPROTOBUF_SUPPORT)
    find_package(Protobuf REQUIRED)
endif ()

#set(COMPILE_LOGURU FALSE)
if (${COMPILE_LOGURU})
    # loguru
    add_definitions(-DLOGURU_SUPPORT)
    set(LOGURU_ROOT ${PROJECT_SOURCE_DIR}/include/loguru)
    ExternalProject_Add(loguru
            #PREFIX ${LOGURU_ROOT}
            GIT_REPOSITORY git@github.com:emilk/loguru.git
            #GIT_TAG origin/master
            GIT_PROGRESS TRUE
            SOURCE_DIR ${LOGURU_ROOT}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND "")
endif ()
include_directories(${PROJECT_SOURCE_DIR}/include)

set(SRC sockets.cpp sockets.h)

if (${STATIC_SOCKETSCPP})
    add_library(SocketsCPP STATIC ${SRC})
else ()
    add_library(SocketsCPP SHARED ${SRC})
endif ()

set_target_properties(SocketsCPP PROPERTIES
        VERSION ${PROJECT_VERSION}
        # SOVERSION 1
        PUBLIC_HEADER sockets.h)

if (${COMPILE_LOGURU})
    add_dependencies(SocketsCPP loguru)
endif ()

if (${COMPILE_PROTOBUF})
    if (${COMPILE_LOGURU})
        target_link_libraries(SocketsCPP dl ${CMAKE_THREADS_LIB_INIT} ${PROTOBUF_LIBRARY})
    else ()
        target_link_libraries(SocketsCPP dl ${PROTOBUF_LIBRARY})
    endif ()
endif ()

install(TARGETS SocketsCPP
        # EXPORT LibSocketsCPP
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/socketscpp)
# export(TARGETS SocketsCPP FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-config.cmake)
# export(PACKAGE ${PROJECT_NAME})
