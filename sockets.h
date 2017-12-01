//
// Created by molguin on 2017-10-27.
//

#ifndef MIGRATIONORCHESTRATOR_SOCKETS_H
#define MIGRATIONORCHESTRATOR_SOCKETS_H

#include <glob.h>
#include <string>

#ifdef PROTOBUF_SUPPORT
#include <google/protobuf/message.h>
#endif

#include <functional>
#include <utility>
#include <sys/socket.h>

#define MAX_CONNECTION_BACKLOG 128

namespace sockets
{
/**
 * @brief Defines an abstraction for the operations of sockets.
 * This is to allow dependency injection in connections.
 */
    struct SocketAPI
    {
        int error_code;

        std::function<int(int, const sockaddr*, socklen_t)> connect;
        std::function<int(int, const sockaddr*, socklen_t)> bind;
        std::function<int(int, sockaddr*, socklen_t*)> accept;
        std::function<int(int, int)> listen;

        std::function<ssize_t(int, const void*, size_t, int)> send;
        std::function<ssize_t(int, void*, size_t, int)> recv;
    };

/**
 * @brief Represents one end of a connection on top of a socket,
 * and provides helper methods to send data to and receive data
 * from the other end.
 */
    class Connection
    {
    private:
        sockaddr* addr;
        int fd;
        bool open;

        SocketAPI socketAPI;

    public:
        Connection() : open(false), fd(-1), addr(nullptr)
        {}

        Connection(const int fd, sockaddr* addr, SocketAPI api)
                : fd(fd), addr(addr), socketAPI(std::move(api)), open(true)
        {}

        ~Connection();

        void sendInt32(int32_t var);
        void recvInt32(int32_t& var);
        void sendBuffer(char* buf, size_t len);
        void recvBuffer(char* buf, size_t len);
        void Close();

#ifdef PROTOBUF_SUPPORT
        /**
         * @brief Sends a protobuf message through the connection.
         * @param msg A Protocol Buffers message object.
         */
        void sendMessage(const google::protobuf::Message& msg);

        /**
         * @brief Receives a protobuf message through the connection.
         * @param msg The Protocol Buffers message object where the incoming data should be stored.
         */
        void recvMessage(google::protobuf::Message& msg);
#endif

        /**
         * @brief Allows to override the socket input-output operations used by this
         * connection object, for testing and dependency injection purposes.
         * @param api
         */
        void setSocketAPI(SocketAPI api)
        { socketAPI = std::move(api); }
    };

/**
 * @brief Base class for all sockets. Defines a common interface for all sockets,
 * unix, tcp, udp or udt to adhere to.
 */
    class ISocket
    {
    private:
        bool is_bound;
        sockaddr* addr{};

    protected:

        ISocket() : is_bound(false), addr(nullptr)
        {}

        virtual ~ISocket()
        { delete addr; }

        void setAddr(sockaddr* addr)
        {
            this->addr = addr;
        }

        void setBound()
        {
            is_bound = true;
        }

    public:
        bool is_Bound() const
        {
            return is_bound;
        }

        const sockaddr* getAddr() const
        {
            return addr;
        }


        /**
         * @brief Connect to the provided socket address.
         * Calling this method implies using this socket as a client socket.
         * @return A Connection object to send and receive data.
         */
        virtual Connection Connect() = 0;

        /**
         * @brief Bind and listen on the provided socket address, previous
         * to an AcceptConnection call.
         * Calling this method implies using this socket as a server socket.
         */
        virtual void BindAndListen() = 0;

        /**
         * @brief Accept an incoming connection from a client on the socket.
         * Calling this method implies using this socket as a server socket.
         * @return A Connection object to send and receive data.
         */
        virtual Connection AcceptConnection() = 0;
    };

/**
 * @brief Abstraction of an Unix Domain Socket.
 */
    class UnixSocket : protected ISocket
    {
    private:
        int socket_fd;
        const std::string socket_path;
        SocketAPI socketAPI;
    public:

        explicit UnixSocket(std::string path);
        ~UnixSocket() override;

        Connection Connect() override;
        void BindAndListen() override;
        Connection AcceptConnection() override;
    };
};


#endif //MIGRATIONORCHESTRATOR_SOCKETS_H
