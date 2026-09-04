#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "net/Packet.h"
#include "net/Socket.h"

namespace {

// uniform  : 맵 전체를 랜덤 워크. AOI가 제대로 동작하는 기준 조건
// hotspot  : 다수가 좁은 구역에 몰린다. AOI가 무력해지는 지점을 본다
// border   : 셀 경계를 사이에 두고 진동한다. 격자 AOI 고유의 약점
// idle     : 접속만 하고 움직이지 않는다. 스레드 비용과 브로드캐스트 비용을 분리하는 대조군
enum class Pattern { Uniform, Hotspot, Border, Idle };

struct Options {
    std::string host = "127.0.0.1";
    uint16_t port = 12345;
    int count = 200;
    int duration = 30;
    int tickHz = 20;
    int32_t mapSize = 3500;
    int32_t cell = 500;
    uint32_t seed = 42;
    Pattern pattern = Pattern::Uniform;
    int rampUpMs = 15;
};

std::atomic<uint64_t> g_bytesReceived{0};
std::atomic<uint64_t> g_snapshots{0};
std::atomic<uint64_t> g_visibleSum{0};
std::atomic<uint64_t> g_seqGaps{0};   // 서버 송신 큐에서 버려진 스냅샷 수
std::atomic<int> g_connected{0};
std::atomic<bool> g_stop{false};

Socket connectTo(const std::string& host, uint16_t port) {
    Socket s(socket(PF_INET, SOCK_STREAM, 0));
    if (s.fd() < 0) return s;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (connect(s.fd(), (struct sockaddr*)&addr, sizeof(addr)) < 0) return Socket(-1);
    return s;
}

// 패턴별 다음 좌표. 봇마다 고정 시드를 주므로 같은 조건이 그대로 재현된다.
void nextPosition(Pattern p, const Options& opt, std::mt19937& rng, int index, int32_t& x,
                  int32_t& y) {
    std::uniform_int_distribution<int32_t> step(-40, 40);
    switch (p) {
        case Pattern::Idle:
            break;
        case Pattern::Uniform: {
            x += step(rng);
            y += step(rng);
            break;
        }
        case Pattern::Hotspot: {
            // 60%는 맵 중앙의 좁은 구역 안에서만 움직인다.
            if (index % 10 < 6) {
                int32_t half = opt.mapSize / 8;
                int32_t c = opt.mapSize / 2;
                x += step(rng);
                y += step(rng);
                if (x < c - half) x = c - half;
                if (x > c + half) x = c + half;
                if (y < c - half) y = c - half;
                if (y > c + half) y = c + half;
            } else {
                x += step(rng);
                y += step(rng);
            }
            break;
        }
        case Pattern::Border: {
            // 셀 경계를 매 tick 넘나들게 해 cell transition을 최대로 만든다.
            int32_t line = opt.cell * (1 + (index % 5));
            x = (x > line) ? line - 1 : line + 1;
            y += step(rng);
            break;
        }
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > opt.mapSize) x = opt.mapSize;
    if (y > opt.mapSize) y = opt.mapSize;
}

void botMain(int index, Options opt) {
    Socket sock = connectTo(opt.host, opt.port);
    if (sock.fd() < 0) return;
    g_connected.fetch_add(1, std::memory_order_relaxed);

    std::mt19937 rng(opt.seed + static_cast<uint32_t>(index));
    std::uniform_int_distribution<int32_t> pos(0, opt.mapSize);
    int32_t x = pos(rng), y = pos(rng);

    // 수신은 별도 스레드. 봇이 recv를 늦게 하면 서버 송신 큐가 쌓여 측정이 오염된다.
    std::thread reader([&sock] {
        std::vector<uint8_t> acc;
        size_t offset = 0;
        uint32_t lastSeq = 0;
        bool first = true;

        while (!g_stop.load(std::memory_order_relaxed)) {
            size_t oldSize = acc.size();
            acc.resize(oldSize + 8192);
            ssize_t n = recv(sock.fd(), acc.data() + oldSize, 8192, 0);
            if (n <= 0) break;
            acc.resize(oldSize + static_cast<size_t>(n));
            g_bytesReceived.fetch_add(static_cast<uint64_t>(n), std::memory_order_relaxed);

            size_t curSize = acc.size();
            while (offset + sizeof(PacketHeader) <= curSize) {
                PacketHeader h = deserializeHeader(acc, offset);
                if (h.length < sizeof(PacketHeader) || offset + h.length > curSize) break;

                if (static_cast<PacketType>(h.type) == PacketType::Snapshot) {
                    int idx = static_cast<int>(offset + sizeof(PacketHeader));
                    uint32_t visible = readBigEndian(2, idx, acc);
                    g_snapshots.fetch_add(1, std::memory_order_relaxed);
                    g_visibleSum.fetch_add(visible, std::memory_order_relaxed);

                    // sequence에 구멍이 있으면 서버 송신 큐에서 버려진 것이다.
                    if (!first && h.sequence > lastSeq + 1)
                        g_seqGaps.fetch_add(h.sequence - lastSeq - 1, std::memory_order_relaxed);
                    lastSeq = h.sequence;
                    first = false;
                }
                offset += h.length;
            }
            if (offset > 65536) {
                acc.erase(acc.begin(), acc.begin() + static_cast<long>(offset));
                offset = 0;
            }
        }
    });

    const auto interval = std::chrono::milliseconds(1000 / opt.tickHz);
    auto next = std::chrono::steady_clock::now() + interval;
    uint32_t seq = 0;

    while (!g_stop.load(std::memory_order_relaxed)) {
        nextPosition(opt.pattern, opt, rng, index, x, y);

        std::vector<uint8_t> buf;
        PacketHeader h{0, static_cast<uint16_t>(PacketType::Move), ++seq};
        serializeHeader(h, buf);
        appendBigEndian(4, static_cast<uint32_t>(x), buf);
        appendBigEndian(4, static_cast<uint32_t>(y), buf);
        patchLength(buf);

        size_t sent = 0;
        while (sent < buf.size()) {
            ssize_t n = send(sock.fd(), buf.data() + sent, buf.size() - sent, 0);
            if (n <= 0) goto done;
            sent += static_cast<size_t>(n);
        }

        std::this_thread::sleep_until(next);
        next += interval;
    }
done:
    shutdown(sock.fd(), SHUT_RDWR);
    reader.join();
}

const char* patternName(Pattern p) {
    switch (p) {
        case Pattern::Uniform: return "uniform";
        case Pattern::Hotspot: return "hotspot";
        case Pattern::Border: return "border";
        case Pattern::Idle: return "idle";
    }
    return "?";
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : "0"; };

        if (arg == "--host") opt.host = next();
        else if (arg == "--port") opt.port = static_cast<uint16_t>(atoi(next()));
        else if (arg == "--count") opt.count = atoi(next());
        else if (arg == "--duration") opt.duration = atoi(next());
        else if (arg == "--tick") opt.tickHz = atoi(next());
        else if (arg == "--map") opt.mapSize = atoi(next());
        else if (arg == "--cell") opt.cell = atoi(next());
        else if (arg == "--seed") opt.seed = static_cast<uint32_t>(atoi(next()));
        else if (arg == "--ramp") opt.rampUpMs = atoi(next());
        else if (arg == "--pattern") {
            std::string p = next();
            if (p == "uniform") opt.pattern = Pattern::Uniform;
            else if (p == "hotspot") opt.pattern = Pattern::Hotspot;
            else if (p == "border") opt.pattern = Pattern::Border;
            else if (p == "idle") opt.pattern = Pattern::Idle;
            else { fprintf(stderr, "unknown pattern: %s\n", p.c_str()); return 1; }
        } else {
            fprintf(stderr,
                    "usage: %s [--host H] [--port N] [--count N] [--duration N] [--tick N]\n"
                    "          [--map N] [--cell N] [--seed N] [--ramp MS]\n"
                    "          [--pattern uniform|hotspot|border|idle]\n",
                    argv[0]);
            return 1;
        }
    }

    printf("[bot] count=%d pattern=%s seed=%u map=%d duration=%ds\n", opt.count,
           patternName(opt.pattern), opt.seed, opt.mapSize, opt.duration);

    std::vector<std::thread> bots;
    bots.reserve(static_cast<size_t>(opt.count));
    for (int i = 0; i < opt.count; ++i) {
        bots.emplace_back(botMain, i, opt);
        // somaxconn이 128이라 동시에 붙이면 일부가 거절된다.
        std::this_thread::sleep_for(std::chrono::milliseconds(opt.rampUpMs));
    }

    std::this_thread::sleep_for(std::chrono::seconds(opt.duration));
    g_stop.store(true, std::memory_order_relaxed);
    for (std::thread& t : bots) t.join();

    uint64_t snaps = g_snapshots.load();
    printf("\n--- bot summary ---\n");
    printf("connected          %d / %d\n", g_connected.load(), opt.count);
    printf("bytes_received     %llu\n", (unsigned long long)g_bytesReceived.load());
    printf("snapshots_received %llu\n", (unsigned long long)snaps);
    printf("snapshot_gaps      %llu\n", (unsigned long long)g_seqGaps.load());
    printf("visible_avg        %.2f\n",
           snaps ? static_cast<double>(g_visibleSum.load()) / static_cast<double>(snaps) : 0.0);
    return 0;
}
