#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "game/Server.h"

namespace {

void printUsage(const char* argv0) {
    printf(
        "usage: %s [options]\n"
        "  --port N        listen port (default 12345)\n"
        "  --map N         map side length (default 3500, keep it a multiple of --cell)\n"
        "  --aoi N         AOI half-extent (default 500)\n"
        "  --cell N        grid cell size (default 500)\n"
        "  --tick N        tick rate in Hz (default 20)\n"
        "  --mode S        naive | grid (default naive)\n"
        "  --max N         max concurrent entities (default 1024)\n"
        "  --queue N       send queue capacity (default 4)\n"
        "  --duration N    print summary and exit after N seconds (0 = run forever)\n",
        argv0);
}

// 파라미터를 같은 줄에 찍는다. 조건 없는 숫자는 나중에 대조할 수 없다.
void printSummary(const Config& cfg, const Metrics& m, int seconds) {
    uint64_t ticks = m.ticks.load();
    uint64_t snapshots = m.snapshotsSent.load();
    uint64_t bytes = m.bytesSent.load();
    uint64_t receivers = m.receiversSum.load();
    uint64_t candidates = m.candidatesSum.load();

    double K = snapshots ? static_cast<double>(receivers) / static_cast<double>(snapshots) : 0.0;
    double cand = snapshots ? static_cast<double>(candidates) / static_cast<double>(snapshots) : 0.0;

    printf("\n--- summary ---\n");
    printf("mode=%s map=%d aoi=%d cell=%d tick=%dHz duration=%ds\n", cfg.mode.c_str(), cfg.mapSize,
           cfg.aoi, cfg.cell, cfg.tickHz, seconds);
    printf("ticks              %llu\n", (unsigned long long)ticks);
    printf("tick_overruns      %llu\n", (unsigned long long)m.tickOverruns.load());
    printf("tick_busy_avg_us   %.1f\n",
           ticks ? static_cast<double>(m.tickBusyMicros.load()) / static_cast<double>(ticks) : 0.0);
    printf("snapshots_sent     %llu\n", (unsigned long long)snapshots);
    printf("snapshots_dropped  %llu\n", (unsigned long long)m.snapshotsDropped.load());
    printf("bytes_sent         %llu\n", (unsigned long long)bytes);
    printf("bytes_per_sec      %.0f\n", seconds ? static_cast<double>(bytes) / seconds : 0.0);
    printf("K_avg              %.2f\n", K);
    printf("candidates_avg     %.2f\n", cand);
    printf("precision          %.3f\n", cand > 0 ? K / cand : 0.0);
}

}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    int duration = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : "0"; };

        if (arg == "--port") cfg.port = static_cast<uint16_t>(atoi(next()));
        else if (arg == "--map") cfg.mapSize = atoi(next());
        else if (arg == "--aoi") cfg.aoi = atoi(next());
        else if (arg == "--cell") cfg.cell = atoi(next());
        else if (arg == "--tick") cfg.tickHz = atoi(next());
        else if (arg == "--mode") cfg.mode = next();
        else if (arg == "--max") cfg.maxEntities = static_cast<uint32_t>(atoi(next()));
        else if (arg == "--queue") cfg.sendQueueCapacity = static_cast<size_t>(atoi(next()));
        else if (arg == "--duration") duration = atoi(next());
        else { printUsage(argv[0]); return 1; }
    }

    if (cfg.tickHz <= 0) cfg.tickHz = 20;
    if (cfg.mode != "naive" && cfg.mode != "grid") {
        fprintf(stderr, "unknown mode: %s\n", cfg.mode.c_str());
        return 1;
    }

    Server server(cfg);

    if (duration > 0) {
        // 측정용. 지정 시간이 지나면 요약을 찍고 종료한다.
        std::thread([&server, &cfg, duration] {
            std::this_thread::sleep_for(std::chrono::seconds(duration));
            printSummary(cfg, server.metrics(), duration);
            fflush(stdout);
            _exit(0);
        }).detach();
    }

    return server.run();
}
