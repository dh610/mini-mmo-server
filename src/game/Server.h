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
    // broadcast : 필터 없이 전원에게. AOI 적용 전 기준선
    // naive     : 전원을 후보로 놓고 AOI 판정. 전송량은 줄지만 판정 비용이 O(N^2)
    // grid      : 공간 분할로 후보를 좁힌 뒤 같은 AOI 판정
    std::string mode = "naive";
};

// 수신/송신/tick 세 스레드가 shared_ptr로 함께 본다.
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
    const World& world() const { return world_; }

private:
    void tickLoop();
    void receiveLoop(std::shared_ptr<Session> session);
    void sendLoop(std::shared_ptr<Session> session);
    void broadcast();
    void removeSession(const std::shared_ptr<Session>& session);

    Config cfg_;
    World world_;
    enum class Mode { Broadcast, Naive, Grid };
    Mode mode_;
    Metrics metrics_;

    std::mutex sessionsMutex_;
    std::vector<std::shared_ptr<Session>> sessions_;
};
