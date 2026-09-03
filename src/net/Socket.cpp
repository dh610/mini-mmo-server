#include "Socket.h"

Socket::Socket(int fd) : fd_(fd) {}

Socket::~Socket() {
    closeIfValid();
}

Socket::Socket(Socket&& other) noexcept
    : fd_(other.fd_)
{
    other.fd_ = -1;  // 안 비우면 other 소멸 시 같은 fd가 이중 close됨
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        closeIfValid();  // 기존에 들고 있던 fd부터 닫아야 누수 없음
        fd_ = other.fd();
        other.fd_ = -1;
    }
    return *this;
}

int Socket::fd() const { return fd_; }
