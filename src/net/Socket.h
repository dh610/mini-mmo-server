#pragma once
#include <unistd.h>

class Socket {
public:
    explicit Socket(int fd);
    ~Socket();

    // 이중 참조 방지
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    int fd() const;

private:
    int fd_;

    void closeIfValid() {
        if (fd_ != -1) close(fd_);
    }
};
