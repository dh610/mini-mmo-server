#pragma once
#include <atomic>
#include <cstdint>

// 지표는 여러 스레드가 더하므로 atomic. relaxed면 충분하다 —
// 순서가 아니라 최종 합계만 필요하고, 읽는 시점은 측정이 끝난 뒤다.
struct Metrics {
    std::atomic<uint64_t> bytesSent{0};       // 주 지표. 배칭 때문에 패킷 "수"는 AOI를 켜도 안 줄어든다
    std::atomic<uint64_t> snapshotsSent{0};
    std::atomic<uint64_t> receiversSum{0};    // K의 분자 (narrow phase 통과 수의 총합)
    std::atomic<uint64_t> candidatesSum{0};   // broad phase가 넘긴 후보 수의 총합
    std::atomic<uint64_t> ticks{0};
    std::atomic<uint64_t> tickOverruns{0};    // 틱 예산 초과. I/O 모델의 한계 지점을 보는 값
    std::atomic<uint64_t> tickBusyMicros{0};
    std::atomic<uint64_t> snapshotsDropped{0};// 송신 큐가 넘쳐 버린 스냅샷

    void add(std::atomic<uint64_t>& c, uint64_t v) { c.fetch_add(v, std::memory_order_relaxed); }
};
