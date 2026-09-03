#pragma once
#include <unistd.h>

class Socket {
public:
    explicit Socket(int fd);
    ~Socket();

    // 복사 금지 — 소멸자가 close(fd_)하므로 복사되면 같은 fd를 이중 close하게 된다.
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    int fd() const;

private:
    int fd_;

    // fd_가 -1(이동으로 비워진 상태)이면 close 생략 — 소멸자/이동 대입이 공유하는 경로.
    void closeIfValid() {
        if (fd_ != -1) close(fd_);
    }
};
