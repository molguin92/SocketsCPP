//
// Created by molguin on 2017-10-27.
//

#include <sys/un.h>
#include "sockets.h"

#ifndef LOGURU_IMPLEMENTATION
#define LOGURU_IMPLEMENTATION 1
#endif

#define LOGURU_WITH_STREAMS 1

#include <loguru/loguru.hpp>
#include <zconf.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace sockets
{
    UnixSocket::UnixSocket(std::string path)
            : socket_path(std::move(path)),
              socket_fd(socket(AF_UNIX, SOCK_SEQPACKET, 0))
    {
        CHECK_NE_S(-1, socket_fd) << "Could not open socket file descriptor. errno: " << strerror(errno);

        auto* _addr = new sockaddr_un{};
        _addr->sun_family = AF_UNIX;
        strncpy(_addr->sun_path, socket_path.c_str(), sizeof(_addr->sun_path) - 1);
        ISocket::setAddr((sockaddr*) _addr);

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
        CHECK_NE_S(socketAPI.connect(socket_fd, ISocket::getAddr(), sizeof(sockaddr_un)), socketAPI.error_code)
            << "Could not connect to socket: " << socket_path << ", errno: " << strerror(errno);

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
        CHECK_NE_S(socketAPI.bind(socket_fd, ISocket::getAddr(), sizeof(sockaddr_un)), socketAPI.error_code)
            << "Could not bind to socket: " << socket_path << ", errno: " << strerror(errno);

        CHECK_NE_S(socketAPI.listen(socket_fd, MAX_CONNECTION_BACKLOG), socketAPI.error_code)
            << "Could not listen on socket: " << socket_path << ", errno: " << strerror(errno);
        ISocket::setBound();
    }

    Connection UnixSocket::AcceptConnection()
    {
        auto* peer_addr = (sockaddr*) (new sockaddr_un{});
        socklen_t len = sizeof(sockaddr_un);
        int connection_fd = socketAPI.accept(socket_fd, peer_addr, &len);
        CHECK_NE_S(connection_fd, socketAPI.error_code)
            << "Could not accept incoming connection on socket: "
            << socket_path << ", errno: " << strerror(errno);

        return Connection(connection_fd, peer_addr, socketAPI);
    }


    Connection::~Connection()
    {
        this->Close();
        delete addr;
    }

    void Connection::sendInt32(int32_t var)
    {
        CHECK_S(open) << "Closed connection.";

        uint32_t total_sent = 0;
        ssize_t sent;

        while (total_sent < sizeof(int32_t))
        {
            sent = socketAPI.send(fd, ((const char*) &var) + total_sent, sizeof(int32_t) - total_sent, 0);
            CHECK_NE_S(sent, -1) << "Error when trying to send int32, errno: " << strerror(errno);
            total_sent += sent;
        }
    }

    void Connection::recvInt32(int32_t& var)
    {
        CHECK_S(open) << "Closed connection";

        uint32_t total_received = 0;
        ssize_t received;
        while (total_received < sizeof(int32_t))
        {
            received = socketAPI.recv(fd, ((char*) &var) + total_received, sizeof(int32_t) - total_received, 0);
            CHECK_NE_S(received, -1) << "Error when trying to receive int32, errno: " << strerror(errno);
            total_received += received;
        }
    }

    void Connection::sendBuffer(char* buf, size_t len)
    {
        CHECK_S(open) << "Closed connection";

        size_t total_sent = 0;
        ssize_t sent;
        while (total_sent < len)
        {
            sent = socketAPI.send(fd, buf + total_sent, len - total_sent, 0);
            CHECK_NE_S(sent, -1) << "Error when trying to send buffer, errno : " << strerror(errno);
            total_sent += sent;
        }
    }

    void Connection::recvBuffer(char* buf, size_t len)
    {
        CHECK_S(open) << "Closed connection";

        size_t total_rcvd = 0;
        ssize_t rcvd;
        while (total_rcvd < len)
        {
            rcvd = socketAPI.recv(fd, buf + total_rcvd, len - total_rcvd, 0);
            CHECK_NE_S(rcvd, -1) << "Error when trying to send buffer, errno : " << strerror(errno);
            total_rcvd += rcvd;
        }
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
        sendInt32(len);
        sendBuffer(buf, static_cast<size_t>(len));
        delete buf;
    }

    void Connection::recvMessage(google::protobuf::Message& msg)
    {
        // first get length of message, then complete message
        int32_t len = 0;
        recvInt32(len);
        auto* buf = new char[len];
        recvBuffer(buf, static_cast<size_t>(len));

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
        CHECK_NE_S(-1, socket_fd) << "Could not open socket file descriptor. errno: " << strerror(errno);

        auto _addr = new sockaddr_in{};
        _addr->sin_family = AF_INET;
        ISocket::setAddr((sockaddr*) _addr);

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

        CHECK_NE_S(socketAPI.bind(socket_fd, addr, sizeof(sockaddr_in)), socketAPI.error_code)
            << "Could not bind socket to port " << port << ", errno: " << strerror(errno);
        CHECK_NE_S(socketAPI.listen(socket_fd, MAX_CONNECTION_BACKLOG), socketAPI.error_code)
            << "Error when trying to listen on socket, errno: " << strerror(errno);
        ISocket::setBound();
    }

    Connection TCPServerSocket::AcceptConnection()
    {
        auto* peer_addr = (sockaddr*) (new sockaddr_in{});
        socklen_t len = sizeof(sockaddr_in);
        int connection_fd = socketAPI.accept(socket_fd, peer_addr, &len);
        CHECK_NE_S(connection_fd, socketAPI.error_code)
            << "Could not accept incoming connection on port " << port << ", errno: " << strerror(errno);

        return Connection(connection_fd, peer_addr, socketAPI);
    }

    Connection TCPServerSocket::Connect()
    {
        ABORT_S() << "Tried to call Connect() on a server socket. Use a client socket next time!";
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
        CHECK_NE_S(socketAPI.connect(socket_fd, ISocket::getAddr(), sizeof(sockaddr_in)), socketAPI.error_code)
            << "Could not connect to host " << address << ":" << port << ", errno: " << strerror(errno);

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
        ABORT_S() << "Tried to call BindAndListen() on a client socket. Use a server socket next time!";
    }

    Connection TCPClientSocket::AcceptConnection()
    {
        ABORT_S() << "Tried to call AcceptConnection() on a client socket. Use a server socket next time!";
        return Connection();
    }


}

