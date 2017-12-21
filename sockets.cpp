//
// Created by molguin on 2017-10-27.
//

#include <sys/un.h>
#include "sockets.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#ifdef LOGURU_SUPPORT
#ifdef COMP_LOGURU
#define LOGURU_IMPLEMENTATION 1
#endif

#define LOGURU_WITH_STREAMS 1

#include <loguru/loguru.hpp>
#endif

#include <zconf.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

namespace socketscpp
{
    UnixSocket::UnixSocket(std::string path)
    : socket_path(std::move(path)),
      socket_fd(socket(AF_UNIX, SOCK_SEQPACKET, 0))
    {
#ifdef LOGURU_SUPPORT
        CHECK_NE_S(-1, socket_fd) << "Could not open socket file descriptor. errno: " << strerror(errno);
#else
        if (-1 == socket_fd) exit(errno);
#endif

        auto* _addr = new sockaddr_un{};
        _addr->sun_family = AF_UNIX;
        strncpy(_addr->sun_path, socket_path.c_str(), sizeof(_addr->sun_path) - 1);
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
        ISocket::setAddr((sockaddr*) _addr);
#pragma clang diagnostic pop

        socketAPI.accept = accept;
        socketAPI.connect = connect;
        socketAPI.bind = bind;
        socketAPI.listen = listen;

        socketAPI.send = send;
        socketAPI.recv = recv;

        socketAPI.error_code = -1;
    }

    UnixSocket::~UnixSocket()
    {
        close(socket_fd);
        if (ISocket::is_Bound())
            unlink(socket_path.c_str());
    }

    Connection UnixSocket::Connect()
    {
#ifdef LOGURU_SUPPORT
        CHECK_NE_S(socketAPI.connect(socket_fd, ISocket::getAddr(), sizeof(sockaddr_un)), socketAPI.error_code)
            << "Could not connect to socket: " << socket_path << ", errno: " << strerror(errno);
#else
        if (socketAPI.error_code == socketAPI.connect(socket_fd, ISocket::getAddr(), sizeof(sockaddr_un))) exit(errno);
#endif

        /*
         * We need to copy the socket address to a new structure to pass to the connection since the destructor
         * of the connection deletes its address - and we don't want to end up with a dangling pointer here,
         * do we now?
         */
        auto _addr = ISocket::getAddr();
        auto n_addr = (sockaddr*) malloc(sizeof(sockaddr));
        memcpy(n_addr, _addr, sizeof(sockaddr));

        return Connection(socket_fd, n_addr, socketAPI);
    }

    void UnixSocket::BindAndListen()
    {
#ifdef LOGURU_SUPPORT
        CHECK_NE_S(socketAPI.bind(socket_fd, ISocket::getAddr(), sizeof(sockaddr_un)), socketAPI.error_code)
            << "Could not bind to socket: " << socket_path << ", errno: " << strerror(errno);

        CHECK_NE_S(socketAPI.listen(socket_fd, MAX_CONNECTION_BACKLOG), socketAPI.error_code)
            << "Could not listen on socket: " << socket_path << ", errno: " << strerror(errno);
#else
        if ((socketAPI.error_code == socketAPI.bind(socket_fd, ISocket::getAddr(), sizeof(sockaddr_un)))
            || (socketAPI.error_code == socketAPI.listen(socket_fd, MAX_CONNECTION_BACKLOG)))
            exit(errno);
#endif
        ISocket::setBound();
    }

    Connection UnixSocket::AcceptConnection()
    {
        auto* peer_addr = (sockaddr*) (new sockaddr_un{});
        socklen_t len = sizeof(sockaddr_un);
        int connection_fd = socketAPI.accept(socket_fd, peer_addr, &len);
#ifdef LOGURU_SUPPORT
        CHECK_NE_S(connection_fd, socketAPI.error_code)
        << "Could not accept incoming connection on socket: "
        << socket_path << ", errno: " << strerror(errno);
#else
        if (connection_fd == socketAPI.error_code) exit(errno);
#endif

        return Connection(connection_fd, peer_addr, socketAPI);
    }


    Connection::~Connection()
    {
        this->Close();
        delete addr;
    }


    template<typename PrimType>
    int Connection::sendPrimitive(const PrimType var)
    {
        if (!open)
        {
#ifdef LOGURU_SUPPORT
            LOG_S(WARNING) << "Closed connection.";
#endif
            return 0;
        }

        PrimType data;
        uint32_t total_sent = 0;
        ssize_t sent;

        size_t len = sizeof(PrimType);
        if (len > sizeof(uint8_t))
        {
            // if sending more than 1 byte
            // reverse endianness
            if (len == sizeof(uint16_t))
                data = (PrimType) htobe16((uint16_t) var);
            else if (len == sizeof(uint32_t))
                data = (PrimType) htobe32((uint32_t) var);
            else if (len == sizeof(uint64_t))
                data = (PrimType) htobe64((uint64_t) var);
        }
        else
            data = var;


        while (total_sent < sizeof(PrimType))
        {
            sent = socketAPI.send(fd, ((const char*) &data) + total_sent, sizeof(PrimType) - total_sent, 0);
#ifdef LOGURU_SUPPORT
            CHECK_NE_S(sent, -1) << "Error when trying to send primitive type, errno: " << strerror(errno);
#else
            if (-1 == sent) exit(errno);
#endif

            if (sent == 0)
            {
#ifdef LOGURU_SUPPORT
                LOG_S(INFO) << "Peer closed connection. Closing on this end.";
#endif
                this->Close();
                return 0;
            }

            total_sent += sent;
        }

        return total_sent;
    }

    template<typename PrimType>
    int Connection::recvPrimitive(PrimType* var)
    {
        if (!open)
        {
#ifdef LOGURU_SUPPORT
            LOG_S(WARNING) << "Closed connection.";
#endif
            return 0;
        }

        uint32_t total_received = 0;
        ssize_t received;
        PrimType data;

        while (total_received < sizeof(PrimType))
        {
            received = socketAPI.recv(fd, ((char*) &data) + total_received, sizeof(PrimType) - total_received, 0);
#ifdef LOGURU_SUPPORT
            CHECK_NE_S(received, -1) << "Error when trying to receive primitive type, errno: " << strerror(errno);
#else
            if (-1 == received) exit(errno);
#endif

            if (received == 0)
            {
#ifdef LOGURU_SUPPORT
                LOG_S(INFO) << "Peer closed connection. Closing on this end.";
#endif
                this->Close();
                return 0;
            }

            total_received += received;
        }

        size_t len = sizeof(PrimType);
        if (len > sizeof(uint8_t))
        {
            // if receiving more than 1 byte
            // reverse endianness
            if (len == sizeof(uint16_t))
                *var = (PrimType) be16toh((uint16_t) data);
            else if (len == sizeof(uint32_t))
                *var = (PrimType) be32toh((uint32_t) data);
            else if (len == sizeof(uint64_t))
                *var = (PrimType) be64toh((uint64_t) data);
        }
        else
            *var = data;

        return total_received;
    }


    /* unsigned integers */
    template int Connection::sendPrimitive<uint8_t>(uint8_t var);
    template int Connection::recvPrimitive<uint8_t>(uint8_t* var);
    template int Connection::sendPrimitive<uint16_t>(uint16_t var);
    template int Connection::recvPrimitive<uint16_t>(uint16_t* var);
    template int Connection::sendPrimitive<uint32_t>(uint32_t var);
    template int Connection::recvPrimitive<uint32_t>(uint32_t* var);
    template int Connection::sendPrimitive<uint64_t>(uint64_t var);
    template int Connection::recvPrimitive<uint64_t>(uint64_t* var);

    /* signed integers */
    template int Connection::sendPrimitive<int8_t>(int8_t var);
    template int Connection::recvPrimitive<int8_t>(int8_t* var);
    template int Connection::sendPrimitive<int16_t>(int16_t var);
    template int Connection::recvPrimitive<int16_t>(int16_t* var);
    template int Connection::sendPrimitive<int32_t>(int32_t var);
    template int Connection::recvPrimitive<int32_t>(int32_t* var);
    template int Connection::sendPrimitive<int64_t>(int64_t var);
    template int Connection::recvPrimitive<int64_t>(int64_t* var);

    /* floating point numbers */
    template int Connection::sendPrimitive<float>(float var);
    template int Connection::recvPrimitive<float>(float* var);
    template int Connection::sendPrimitive<double>(double var);
    template int Connection::recvPrimitive<double>(double* var);


    size_t Connection::sendBuffer(char* buf, size_t len)
    {
        if (!open)
        {
#ifdef LOGURU_SUPPORT
            LOG_S(WARNING) << "Closed connection.";
#endif
            return 0;
        }

        size_t total_sent = 0;
        ssize_t sent;
        while (total_sent < len)
        {
            sent = socketAPI.send(fd, buf + total_sent, len - total_sent, 0);
#ifdef LOGURU_SUPPORT
            CHECK_NE_S(sent, -1) << "Error when trying to send buffer, errno : " << strerror(errno);
#else
            if (-1 == sent) exit(errno);
#endif

            if (sent == 0)
            {
#ifdef LOGURU_SUPPORT
                LOG_S(INFO) << "Peer closed connection. Closing on this end.";
#endif
                this->Close();
                return 0;
            }

            total_sent += sent;
        }

        return total_sent;
    }

    size_t Connection::recvBuffer(char* buf, size_t len)
    {
        if (!open)
        {
#ifdef LOGURU_SUPPORT
            LOG_S(WARNING) << "Closed connection.";
#endif
            return 0;
        }

        size_t total_rcvd = 0;
        ssize_t rcvd;
        while (total_rcvd < len)
        {
            rcvd = socketAPI.recv(fd, buf + total_rcvd, len - total_rcvd, 0);
#ifdef LOGURU_SUPPORT
            CHECK_NE_S(rcvd, -1) << "Error when trying to send buffer, errno : " << strerror(errno);
#else
            if (-1 == rcvd) exit(errno);
#endif

            if (rcvd == 0)
            {
#ifdef LOGURU_SUPPORT
                LOG_S(INFO) << "Peer closed connection. Closing on this end.";
#endif
                this->Close();
                return 0;
            }

            total_rcvd += rcvd;
        }

        return total_rcvd;
    }

#ifdef PROTOBUF_SUPPORT

    void Connection::sendMessage(const google::protobuf::Message& msg)
    {
        // dump message into a buffer and send it
        int len = msg.ByteSize();
        auto buf = new char[len];

        // TODO: error handling on the serialization
        // TODO: counting size twice... maybe switch to using strings->chararrays?
        msg.SerializeToArray(buf, len);

        // first send length, then serialized message
#ifdef LOGURU_SUPPORT
        CHECK_GE_S(sendInt32(len), 0) << "Peer closed connection unexpectedly.";
        CHECK_GE_S(sendBuffer(buf, static_cast<size_t>(len)), 0) << "Peer closed connection unexpectedly.";
#else
        if ((0 < sendInt32(len)) || (0 < sendBuffer(buf, static_cast<size_t>(len))))
        {
            std::cerr << "Peer closed connection unexpectedly." << std::endl;
            exit(-1);
        }
#endif
        delete buf;
    }

    void Connection::recvMessage(google::protobuf::Message& msg)
    {
        // first get length of message, then complete message
        int32_t len = 0;
        recvInt32(len);
        auto* buf = new char[len];
#ifdef LOGURU_SUPPORT
        CHECK_GE_S(recvBuffer(buf, static_cast<size_t>(len)), 0) << "Peer closed connection unexpectedly.";
#else
        if(0 < recvBuffer(buf, static_cast<size_t>(len)))
        {
            std::cerr << "Peer closed connection unexpectedly." << std::endl;
            exit(-1);
        }
#endif

        msg.ParseFromArray(buf, len);
        delete buf;
    }

#endif

    void Connection::Close()
    {
        if (!open) return;
        close(fd);
        open = false;
    }

    bool Connection::isOpen()
    {
        return open;
    }

    TCPCommonSocket::TCPCommonSocket(uint16_t port) :
    socket_fd(socket(AF_INET, SOCK_STREAM, 0)), port(port)
    {
#ifdef LOGURU_SUPPORT
        CHECK_NE_S(-1, socket_fd) << "Could not open socket file descriptor. errno: " << strerror(errno);
#else
        if (-1 == socket_fd) exit(errno);
#endif
        // socket options for reuse:
        int set_opt = 1;
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (char*) &set_opt, sizeof(int));

        auto _addr = new sockaddr_in{};
        _addr->sin_family = AF_INET;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
        ISocket::setAddr((sockaddr*) _addr);
#pragma clang diagnostic pop

        socketAPI.accept = accept;
        socketAPI.connect = connect;
        socketAPI.bind = bind;
        socketAPI.listen = listen;

        socketAPI.send = send;
        socketAPI.recv = recv;

        socketAPI.error_code = -1;
    }

    TCPCommonSocket::~TCPCommonSocket()
    {
        close(socket_fd);
    }

    TCPServerSocket::TCPServerSocket(uint16_t port)
    : TCPCommonSocket(port)
    {
        auto _addr = (sockaddr_in*) ISocket::getAddr();
        _addr->sin_addr.s_addr = INADDR_ANY;
        _addr->sin_port = htons(port);
    }

    void TCPServerSocket::BindAndListen()
    {
        auto addr = ISocket::getAddr();

#ifdef LOGURU_SUPPORT
        CHECK_NE_S(socketAPI.bind(socket_fd, addr, sizeof(sockaddr_in)), socketAPI.error_code)
        << "Could not bind socket to port " << port << ", errno: " << strerror(errno);
        CHECK_NE_S(socketAPI.listen(socket_fd, MAX_CONNECTION_BACKLOG), socketAPI.error_code)
        << "Error when trying to listen on socket, errno: " << strerror(errno);
#else
        if ((socketAPI.error_code == socketAPI.bind(socket_fd, addr, sizeof(sockaddr_in)))
            || (socketAPI.error_code == socketAPI.listen(socket_fd, MAX_CONNECTION_BACKLOG)))
            exit(errno);
#endif
        ISocket::setBound();
    }

    Connection TCPServerSocket::AcceptConnection()
    {
        auto* peer_addr = (sockaddr*) (new sockaddr_in{});
        socklen_t len = sizeof(sockaddr_in);
        int connection_fd = socketAPI.accept(socket_fd, peer_addr, &len);
#ifdef LOGURU_SUPPORT
        CHECK_NE_S(connection_fd, socketAPI.error_code)
        << "Could not accept incoming connection on port " << port << ", errno: " << strerror(errno);
#else
        if (socketAPI.error_code == connection_fd) exit(errno);
#endif

        return Connection(connection_fd, peer_addr, socketAPI);
    }

    Connection TCPServerSocket::Connect()
    {
#ifdef LOGURU_SUPPORT
        ABORT_S() << "Tried to call Connect() on a server socket. Use a client socket next time!";
#else
        std::cerr << "Tried to call Connect() on a server socket. Use a client socket next time!" << std::endl;
        exit(-1);
#endif
    }

    TCPClientSocket::TCPClientSocket(std::string address, uint16_t port)
    : TCPCommonSocket(port), address(std::move(address))
    {
        auto _addr = (sockaddr_in*) ISocket::getAddr();
        _addr->sin_addr.s_addr = inet_addr(this->address.c_str());
        _addr->sin_port = htons(port);
    }

    Connection TCPClientSocket::Connect()
    {
#ifdef LOGURU_SUPPORT
        CHECK_NE_S(socketAPI.connect(socket_fd, ISocket::getAddr(), sizeof(sockaddr_in)), socketAPI.error_code)
        << "Could not connect to host " << address << ":" << port << ", errno: " << strerror(errno);
#else
        if (socketAPI.error_code == socketAPI.connect(socket_fd, ISocket::getAddr(), sizeof(sockaddr_in))) exit(errno);
#endif

        /*
         * We need to copy the socket address to a new structure to pass to the connection since the destructor
         * of the connection deletes its address - and we don't want to end up with a dangling pointer here,
         * do we now?
         */
        auto _addr = ISocket::getAddr();
        auto n_addr = (sockaddr*) malloc(sizeof(sockaddr));
        memcpy(n_addr, _addr, sizeof(sockaddr));

        return Connection(socket_fd, n_addr, socketAPI);
    }

    void TCPClientSocket::BindAndListen()
    {
#ifdef LOGURU_SUPPORT
        ABORT_S() << "Tried to call BindAndListen() on a client socket. Use a server socket next time!";
#else
        std::cerr << "Tried to call BindAndListen() on a client socket. Use a server socket next time!" << std::endl;
        exit(-1);
#endif
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
    Connection TCPClientSocket::AcceptConnection()
    {
#ifdef LOGURU_SUPPORT
        ABORT_S() << "Tried to call AcceptConnection() on a client socket. Use a server socket next time!";
#else
        std::cerr << "Tried to call AcceptConnection() on a client socket. Use a server socket next time!" << std::endl;
        exit(-1);
        return Connection();
#endif
    }

#pragma clang diagnostic pop
}


#pragma clang diagnostic pop
