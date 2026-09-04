#pragma once
#include <atomic>
#include <cstdint>

// 여러 스레드가 더한다. 순서는 필요 없고 합계만 쓰므로 relaxed.
struct Metrics {
    std::atomic<uint64_t> bytesSent{0};
    std::atomic<uint64_t> snapshotsSent{0};
    std::atomic<uint64_t> receiversSum{0};     // narrow phase 통과 수의 총합 (K의 분자)
    std::atomic<uint64_t> candidatesSum{0};    // broad phase가 넘긴 후보 수의 총합
    std::atomic<uint64_t> ticks{0};
    std::atomic<uint64_t> tickOverruns{0};
    std::atomic<uint64_t> tickBusyMicros{0};
    std::atomic<uint64_t> snapshotsDropped{0};

    void add(std::atomic<uint64_t>& c, uint64_t v) { c.fetch_add(v, std::memory_order_relaxed); }
};
