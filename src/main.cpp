#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <cstdio>

#include "net/Socket.h"
#include "net/Packet.h"

// accumulator의 offset 이후에서 완성된 패킷을 있는 만큼 꺼내 echo하고 offset을 전진시킨다.
// 패킷이 덜 왔으면(헤더 또는 payload 부족) 조용히 리턴 — 정상적인 TCP 상황이라 에러가 아니다.
// 길이를 미리 확인하지 않고 deserializeHeader를 부르면 in.at()이 예외를 던지고,
// 그게 스레드 진입 함수 밖으로 새어나가면 std::terminate로 서버 전체가 죽는다.
void drainPackets(std::vector<uint8_t>& accumulator, size_t& offset, Socket& sock) {
    size_t cur_size = accumulator.size();
    while(offset < cur_size) {
        if (cur_size - offset < sizeof(PacketHeader))
            return;
        PacketHeader header = deserializeHeader(accumulator, offset);

        // length는 헤더 포함 전체 길이라 sizeof(PacketHeader)보다 작으면 잘못된 패킷.
        if (header.length < sizeof(PacketHeader))
            return;
        if (cur_size < offset + header.length)
            return;

        send(sock.fd(), accumulator.data() + offset, header.length, 0);  // offset 전진 전에 보내야 함
        offset += header.length;
    }
}

// Socket을 값으로 받아 스레드가 fd의 유일한 소유자가 되게 한다 (복사 =delete, 이동만 허용).
void handleClient(Socket sock) {
    // accumulator = 받았지만 아직 처리 못 한 바이트 전부, offset = 그중 처리된 위치.
    // 새 데이터는 항상 끝(size())에 이어붙이고, offset은 drainPackets만 전진시킨다.
    std::vector<uint8_t> accumulator;
    size_t offset = 0;

    while (true) {
        // old_size는 resize 전에 캡처해야 한다 — resize가 size()를 바꾸므로.
        size_t old_size = accumulator.size();
        accumulator.resize(old_size + 4096);
        ssize_t n = recv(sock.fd(), accumulator.data() + old_size, 4096, 0);
        if (n <= 0) return;
        accumulator.resize(old_size + n);

        drainPackets(accumulator, offset, sock);
    }
}


int main() {
    // IPv4 TCP 소켓 생성 후 0.0.0.0:12345로 바인드.
    struct sockaddr_in serv_addr;
    Socket serv(socket(PF_INET, SOCK_STREAM, 0));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi("12345"));

    // bind/listen 리턴값을 안 보면 실패가 조용히 묻힌다 (포트 충돌을 실제로 겪고 추가함).
    if (bind(serv.fd(), (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(serv.fd(), 10) < 0) {
        perror("listen");
        return 1;
    }

    while (true) {
        int client_fd = accept(serv.fd(), nullptr, nullptr);
        Socket clientSocket(client_fd);
        // std::thread는 joinable인 채로 소멸되면 std::terminate를 부르므로 detach 필요.
        std::thread(handleClient, std::move(clientSocket)).detach();
    }
}
