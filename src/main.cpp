#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <stdexcept>
#include <cstring>

#include "net/Socket.h"

// Socket을 값으로 받는 이유: 복사가 =delete라, 호출부에서 std::move로 넘기면
// 스레드가 이 fd의 유일한 소유자가 된다. 스레드 함수가 끝나면 sock이 스코프를 벗어나며
// 소멸자가 close를 호출하므로 연결 종료 시 자원 해제를 잊을 수 없다.
void handleClient(Socket sock) {
    char buf[1024];
    while (true) {
        ssize_t n = recv(sock.fd(), buf, sizeof(buf), 0);
        if(n <= 0) break;
        send(sock.fd(), buf, n, 0);
    }
}


int main() {
    struct sockaddr_in serv_addr;
    Socket serv(socket(PF_INET, SOCK_STREAM, 0));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi("12345"));

    bind(serv.fd(), (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(serv.fd(), 10);

    while (true) {
        int client_fd = accept(serv.fd(), nullptr, nullptr);
        Socket clientSocket(client_fd);
        // detach하는 이유: std::thread는 pthread와 달리 joinable인 채로 소멸되면
        // std::terminate를 부른다. 여기서 만든 thread 객체는 반복문이 다음 accept로
        // 넘어가며 곧바로 스코프를 벗어나므로, join할 대상이 없어 detach가 맞다.
        std::thread(handleClient, std::move(clientSocket)).detach();
    }
}
