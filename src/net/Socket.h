#pragma once
#include <unistd.h>

class Socket {
public:
    explicit Socket(int fd);
    ~Socket();

    // 복사하면 같은 fd를 이중 close하게 된다.
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    int fd() const;

private:
    int fd_;

    // 이동으로 비워진 상태(-1)면 close를 생략한다.
    void closeIfValid() {
        if (fd_ != -1) close(fd_);
    }
};
