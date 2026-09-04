#pragma once
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "game/Metrics.h"
#include "game/SendQueue.h"
#include "net/Socket.h"
#include "world/World.h"

struct Config {
    uint16_t port = 12345;
    int32_t mapSize = 3500;
    int32_t aoi = 500;
    int32_t cell = 500;
    int tickHz = 20;
    uint32_t maxEntities = 1024;
    size_t sendQueueCapacity = 4;
    std::string mode = "naive";  // naive | grid
};

// 연결 하나. 수신 스레드가 소유하고, tick과 송신 스레드가 shared_ptr로 함께 본다.
struct Session {
    explicit Session(Socket&& s, uint32_t entityId, size_t queueCap)
        : sock(std::move(s)), id(entityId), out(queueCap) {}

    Socket sock;
    uint32_t id;
    SendQueue out;
    std::atomic<uint32_t> sequence{0};
    std::atomic<bool> alive{true};
};

class Server {
public:
    explicit Server(const Config& cfg);

    int run();  // accept 루프. 돌아오지 않는다
    const Metrics& metrics() const { return metrics_; }

private:
    void tickLoop();
    void receiveLoop(std::shared_ptr<Session> session);
    void sendLoop(std::shared_ptr<Session> session);
    void broadcast();
    void removeSession(const std::shared_ptr<Session>& session);

    Config cfg_;
    World world_;
    Metrics metrics_;

    std::mutex sessionsMutex_;
    std::vector<std::shared_ptr<Session>> sessions_;
};
