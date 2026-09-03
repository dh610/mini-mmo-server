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

    // 이동 후 fd_가 -1(무효 sentinel)이면 close를 건너뛴다.
    // 소멸자와 이동 대입 연산자가 공유하는 경로라, 이동으로 비워진 객체가
    // 소멸될 때 아직 살아있는 새 소유자의 fd를 실수로 닫는 걸 막는다.
    void closeIfValid() {
        if (fd_ != -1) close(fd_);
    }
};
