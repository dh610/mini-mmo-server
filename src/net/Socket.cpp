#include "Socket.h"

Socket::Socket(int fd) : fd_(fd) {}

Socket::~Socket() {
    // fd_가 유효할 때만 close
    closeIfValid();
}

Socket::Socket(Socket&& other) noexcept
    : fd_(other.fd_)
{
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        closeIfValid();
        fd_ = other.fd();
        other.fd_ = -1;
    }
    return *this;
}

int Socket::fd() const { return fd_; }
