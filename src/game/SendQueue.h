#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

// tick 스레드가 push, 연결당 송신 스레드가 pop.
// 상한을 넘으면 오래된 스냅샷부터 버린다. 스냅샷이 전체 상태라 이전 것 없이 해석된다.
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

    // close되면 false. 송신 스레드는 그걸 보고 빠져나간다.
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
