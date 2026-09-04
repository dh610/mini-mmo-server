#include "game/Server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdio>
#include <cstring>
#include <random>
#include <thread>

#include "net/Packet.h"

namespace {

// 헤더를 먼저 쓰고, 페이로드를 다 쓴 뒤 patchLength로 length를 메운다.
void beginPacket(std::vector<uint8_t>& buf, PacketType type, uint32_t sequence) {
    buf.clear();
    PacketHeader header{0, static_cast<uint16_t>(type), sequence};
    serializeHeader(header, buf);
}

}  // namespace

Server::Server(const Config& cfg)
    : cfg_(cfg), world_(cfg.mapSize, cfg.aoi, cfg.maxEntities) {}

void Server::removeSession(const std::shared_ptr<Session>& session) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    for (size_t i = 0; i < sessions_.size(); ++i) {
        if (sessions_[i] == session) {
            sessions_[i] = sessions_.back();
            sessions_.pop_back();
            return;
        }
    }
}

// 게임 상태를 직접 만지지 않고 명령 큐에만 넣는다.
void Server::receiveLoop(std::shared_ptr<Session> session) {
    // accumulator = 받았지만 아직 처리 못 한 바이트, offset = 그중 처리된 위치.
    std::vector<uint8_t> accumulator;
    size_t offset = 0;

    while (session->alive.load(std::memory_order_relaxed)) {
        size_t oldSize = accumulator.size();
        accumulator.resize(oldSize + 4096);
        ssize_t n = recv(session->sock.fd(), accumulator.data() + oldSize, 4096, 0);
        if (n <= 0) break;
        accumulator.resize(oldSize + static_cast<size_t>(n));

        size_t curSize = accumulator.size();
        while (offset + sizeof(PacketHeader) <= curSize) {
            PacketHeader header = deserializeHeader(accumulator, offset);
            if (header.length < sizeof(PacketHeader)) break;  // 잘못된 패킷
            if (offset + header.length > curSize) break;      // 아직 덜 왔다

            if (static_cast<PacketType>(header.type) == PacketType::Move) {
                int idx = static_cast<int>(offset + sizeof(PacketHeader));
                int32_t x = static_cast<int32_t>(readBigEndian(4, idx, accumulator));
                int32_t y = static_cast<int32_t>(readBigEndian(4, idx, accumulator));
                world_.pushCommand({CmdType::Move, session->id, x, y});
            }
            offset += header.length;
        }

        // 처리한 앞부분을 잘라내지 않으면 accumulator가 무한히 자란다.
        if (offset > 65536) {
            accumulator.erase(accumulator.begin(), accumulator.begin() + static_cast<long>(offset));
            offset = 0;
        }
    }

    session->alive.store(false, std::memory_order_relaxed);
    world_.pushCommand({CmdType::Despawn, session->id, 0, 0});
    session->out.close();
    removeSession(session);
    world_.releaseId(session->id);
}

void Server::sendLoop(std::shared_ptr<Session> session) {
    std::vector<uint8_t> packet;
    while (session->out.pop(packet)) {
        size_t sent = 0;
        while (sent < packet.size()) {
            // send는 한 번에 다 나가지 않을 수 있다.
            ssize_t n = send(session->sock.fd(), packet.data() + sent, packet.size() - sent, 0);
            if (n <= 0) {
                session->alive.store(false, std::memory_order_relaxed);
                return;
            }
            sent += static_cast<size_t>(n);
        }
        metrics_.add(metrics_.bytesSent, packet.size());
        metrics_.add(metrics_.snapshotsSent, 1);
    }
}

void Server::broadcast() {
    std::vector<std::shared_ptr<Session>> snapshot;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        snapshot = sessions_;
    }

    const std::vector<Entity>& entities = world_.entities();
    std::vector<uint32_t> visible;
    std::vector<uint8_t> buf;
    uint64_t candidates = 0;
    uint64_t receivers = 0;

    for (const std::shared_ptr<Session>& session : snapshot) {
        const Entity& me = entities[session->id];
        if (!me.active) continue;

        visible.clear();
        world_.collectNaive(me, visible, candidates);
        receivers += visible.size();

        beginPacket(buf, PacketType::Snapshot,
                    session->sequence.fetch_add(1, std::memory_order_relaxed));
        appendBigEndian(2, static_cast<uint32_t>(visible.size()), buf);
        for (uint32_t id : visible) {
            const Entity& e = entities[id];
            appendBigEndian(4, e.id, buf);
            appendBigEndian(4, static_cast<uint32_t>(e.x), buf);
            appendBigEndian(4, static_cast<uint32_t>(e.y), buf);
        }
        patchLength(buf);

        size_t dropped = session->out.push(std::vector<uint8_t>(buf));
        if (dropped) metrics_.add(metrics_.snapshotsDropped, dropped);
    }

    metrics_.add(metrics_.candidatesSum, candidates);
    metrics_.add(metrics_.receiversSum, receivers);
}

void Server::tickLoop() {
    const auto interval = std::chrono::milliseconds(1000 / cfg_.tickHz);
    auto next = std::chrono::steady_clock::now() + interval;

    while (true) {
        auto begin = std::chrono::steady_clock::now();

        world_.applyCommands();
        broadcast();

        auto end = std::chrono::steady_clock::now();
        metrics_.add(metrics_.tickBusyMicros,
                     static_cast<uint64_t>(
                         std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()));
        metrics_.add(metrics_.ticks, 1);

        if (end > next) {
            metrics_.add(metrics_.tickOverruns, 1);
            // 밀린 만큼 따라잡으려 하면 이후 sleep_until이 계속 즉시 반환하며 폭주한다.
            next = end;
        }
        std::this_thread::sleep_until(next);
        next += interval;
    }
}

int Server::run() {
    Socket serv(socket(PF_INET, SOCK_STREAM, 0));
    if (serv.fd() < 0) {
        perror("socket");
        return 1;
    }

    // 재시작 시 TIME_WAIT 때문에 bind가 실패하는 것을 막는다.
    int reuse = 1;
    setsockopt(serv.fd(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(cfg_.port);

    if (bind(serv.fd(), (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    // somaxconn이 128이라 동시 접속이 몰리면 일부가 거절된다. 봇은 램프업으로 붙는다.
    if (listen(serv.fd(), 128) < 0) {
        perror("listen");
        return 1;
    }

    printf("[server] mode=%s port=%u map=%d aoi=%d tick=%dHz\n", cfg_.mode.c_str(), cfg_.port,
           cfg_.mapSize, cfg_.aoi, cfg_.tickHz);

    std::thread(&Server::tickLoop, this).detach();

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int32_t> pos(0, cfg_.mapSize);

    while (true) {
        int clientFd = accept(serv.fd(), nullptr, nullptr);
        if (clientFd < 0) continue;

        uint32_t id = 0;
        if (!world_.reserveId(id)) {
            close(clientFd);
            continue;
        }

        auto session = std::make_shared<Session>(Socket(clientFd), id, cfg_.sendQueueCapacity);

        // 분포 제어는 봇의 --pattern이 하므로 스폰은 균등 랜덤.
        world_.pushCommand({CmdType::Spawn, id, pos(rng), pos(rng)});

        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_.push_back(session);
        }

        // 접속 직후 id를 알려준다. 이후 Move는 이 세션의 id로 처리된다.
        std::vector<uint8_t> hello;
        beginPacket(hello, PacketType::Hello, 0);
        appendBigEndian(4, id, hello);
        patchLength(hello);
        session->out.push(std::move(hello));

        std::thread(&Server::sendLoop, this, session).detach();
        std::thread(&Server::receiveLoop, this, session).detach();
    }
}
