#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <stdexcept>
#include <cstring>

#include "net/Socket.h"

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
        std::thread(handleClient, std::move(clientSocket)).detach();
    }
}
