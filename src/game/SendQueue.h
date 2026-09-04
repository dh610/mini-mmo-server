#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

// tick 스레드가 push하고 연결당 송신 스레드가 pop한다.
//
// tick이 직접 send()를 부르지 않는 이유: 느린 클라이언트 하나가 send에서 막히면
// tick 전체가 밀린다. 게임 시간이 네트워크 사정에 묶이는 구조는 피해야 한다.
//
// 상한을 넘으면 "오래된" 스냅샷을 버린다. 스냅샷이 전체 상태라서 이전 것 없이도
// 해석이 되기 때문에 가능한 정책이다 — 델타 방식이었다면 중간을 버릴 수 없다.
class SendQueue {
public:
    explicit SendQueue(size_t capacity) : capacity_(capacity) {}

    // 버려진 개수를 반환한다.
    size_t push(std::vector<uint8_t>&& packet) {
        size_t dropped = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) return 0;
            while (queue_.size() >= capacity_) {
                queue_.pop_front();
                ++dropped;
            }
            queue_.push_back(std::move(packet));
        }
        cv_.notify_one();
        return dropped;
    }

    // 큐가 빌 때까지 대기. close되면 false를 돌려주고 송신 스레드가 빠져나간다.
    bool pop(std::vector<uint8_t>& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

private:
    size_t capacity_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::vector<uint8_t>> queue_;
    bool closed_ = false;
};
