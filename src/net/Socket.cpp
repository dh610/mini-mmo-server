#include "Socket.h"

Socket::Socket(int fd) : fd_(fd) {}

Socket::~Socket() {
    // fd_가 유효할 때만 close
    closeIfValid();
}

Socket::Socket(Socket&& other) noexcept
    : fd_(other.fd_)
{
    // other.fd_를 -1로 비워야 other가 소멸될 때 이미 옮겨간 fd를 같이 닫지 않는다.
    // 안 비우면 같은 fd에 대해 close()가 두 번 불려서 EBADF나 재사용된 fd 오폭이 난다.
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        closeIfValid();  // 자기 자신이 이미 들고 있던 fd부터 닫아야 누수 없음
        fd_ = other.fd();
        other.fd_ = -1;
    }
    return *this;
}

int Socket::fd() const { return fd_; }
