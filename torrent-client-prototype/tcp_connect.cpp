#include "tcp_connect.h"
#include "byte_tools.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <chrono>
#include <utility>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>

TcpConnect::TcpConnect(std::string ip, int port, std::chrono::milliseconds connectTimeout,
                       std::chrono::milliseconds readTimeout)
        : ip_(std::move(ip)),
          port_(port),
          connectTimeout_(connectTimeout),
          readTimeout_(readTimeout),
          sock_(0) {}

TcpConnect::~TcpConnect() {
    CloseConnection();
}

void TcpConnect::EstablishConnection() {
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == -1)
        throw std::runtime_error("Can't create a socket!\n");

    struct sockaddr_in hint{};
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port_);
    if (inet_pton(AF_INET, ip_.c_str(), &hint.sin_addr) <= 0)
        throw std::runtime_error("Invalid IP address\n");
    if (fcntl(sock_, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("Cannot set to nonblocking mode\n");

    if (connect(sock_, (struct sockaddr *) &hint, sizeof(hint)) == -1) {
        if (errno != EINPROGRESS) {
            throw std::runtime_error("Can't connect to IP/port\n");
        }
    }

    fd_set fdSet;
    FD_ZERO(&fdSet);
    FD_SET(sock_, &fdSet);

    struct timeval tv{};
    tv.tv_sec = connectTimeout_.count() / 1000;
    tv.tv_usec = (connectTimeout_.count() % 1000) * 1000;
    std::cout << connectTimeout_.count() * 1000 << "\n";

    if (select(sock_ + 1, nullptr, &fdSet, nullptr, &tv) == 1) {
        int so_error;
        socklen_t length = sizeof(so_error);

        getsockopt(sock_, SOL_SOCKET, SO_ERROR, &so_error, &length);

        if (so_error == 0) {
            int fgs = fcntl(sock_, F_GETFL, 0);
            fgs = fgs & ~O_NONBLOCK;
            fcntl(sock_, F_SETFL, fgs);
            return;
        }
    }
    throw std::runtime_error("Timeout exceeded\n");
}

void TcpConnect::SendData(const std::string &data) const {
    if (send(sock_, data.c_str(), data.size(), 0) == -1)
        throw std::runtime_error("Can't send the data");
}

std::string TcpConnect::ReceiveData(size_t bufferSize) const {
    std::string response;

    struct pollfd fd{};
    fd.fd = sock_;
    fd.events = POLLIN;

    int ret = poll(&fd, 1, int(readTimeout_.count()));

    if (ret == -1)
        throw std::runtime_error("Cannot read\n");
    else if (ret == 0)
        throw std::runtime_error("Timeout for reading\n");

    if (bufferSize == 0) {
        char sz[4];
        long bytesRead = recv(sock_, &sz, 4, MSG_DONTWAIT);
        if (bytesRead < 0)
            throw std::runtime_error("Cannot read\n");
        long size = BytesToInt(sz);
        bufferSize = size;
    }

    if (bufferSize > std::numeric_limits<uint16_t>::max())
        throw std::runtime_error("Size is too large!");

    char buffer[bufferSize];
    long bytesRead;
    auto remaining = bufferSize;

    auto start = std::chrono::steady_clock::now();
    do {
        auto dif = std::chrono::steady_clock::now() - start;
        if (dif > readTimeout_)
            throw std::runtime_error("timeout for reading!");
        bytesRead = recv(sock_, buffer, bufferSize, 0);
        if (bytesRead <= 0)
            throw std::runtime_error("error while reading bytes from socket!");
        remaining -= bytesRead;
        for (int i = 0; i < bytesRead; ++i)
            response += buffer[i];
    } while (remaining > 0);

    return response;
}

void TcpConnect::CloseConnection() const {
    close(sock_);
}

const std::string &TcpConnect::GetIp() const {
    return ip_;
}

int TcpConnect::GetPort() const {
    return port_;
}