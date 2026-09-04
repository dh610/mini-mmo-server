#pragma once
#include <cstdint>
#include <mutex>
#include <vector>

struct Entity {
    uint32_t id = 0;
    int32_t x = 0;
    int32_t y = 0;
    bool active = false;
};

enum class CmdType : uint8_t { Spawn, Move, Despawn };

// 수신 스레드가 넣고 tick 스레드만 꺼낸다.
struct Command {
    CmdType type;
    uint32_t id;
    int32_t x;
    int32_t y;
};

// 맵을 정사각형 셀로 자르고, 각 셀이 그 안의 entity id를 들고 있다.
// 포인터가 아니라 id를 담는 이유는 World::entities_가 재할당되지 않더라도
// id 기반이 셀 이동 시 지우기가 간단하기 때문이다.
class Grid {
public:
    Grid(int32_t mapSize, int32_t cellSize);

    int32_t cellSize() const { return cellSize_; }
    int32_t dim() const { return dim_; }
    int32_t cellOf(int32_t coord) const;

    void insert(int32_t cx, int32_t cy, uint32_t id);
    void remove(int32_t cx, int32_t cy, uint32_t id);

    const std::vector<uint32_t>& at(int32_t cx, int32_t cy) const {
        return cells_[static_cast<size_t>(cy) * static_cast<size_t>(dim_) + static_cast<size_t>(cx)];
    }

private:
    int32_t cellSize_;
    int32_t dim_;
    std::vector<std::vector<uint32_t>> cells_;
};

class World {
public:
    // 슬롯은 maxEntities로 고정된다. 실행 중 entities_가 재할당되면
    // tick 스레드가 순회하는 도중에 무너진다.
    World(int32_t mapSize, int32_t aoiHalfExtent, int32_t cellSize, uint32_t maxEntities);

    int32_t mapSize() const { return mapSize_; }
    int32_t aoi() const { return aoi_; }

    // --- 수신 스레드에서 호출 ---
    // 슬롯만 예약한다. 실제 배치는 tick이 Spawn 명령을 처리할 때.
    bool reserveId(uint32_t& outId);
    void releaseId(uint32_t id);
    void pushCommand(const Command& cmd);

    // --- tick 스레드에서만 호출 ---
    void applyCommands();
    const std::vector<Entity>& entities() const { return entities_; }

    // 필터 없이 활성 entity 전부를 out에 넣는다. AOI 적용 전 기준선.
    void collectAll(const Entity& me, std::vector<uint32_t>& out, uint64_t& candidates) const;

    // me 주변 AOI 안 entity의 id를 out에 채우고, broad phase가 넘긴 후보 수를
    // candidates에 누적한다.
    void collectNaive(const Entity& me, std::vector<uint32_t>& out, uint64_t& candidates) const;

    // broad phase만 다르다. 후보를 AOI가 걸치는 셀 범위로 좁힌 뒤 같은 narrow phase를 태운다.
    void collectGrid(const Entity& me, std::vector<uint32_t>& out, uint64_t& candidates) const;

    // Move 적용 중 셀이 바뀐 횟수. border 패턴에서 bookkeeping 비용을 보기 위한 지표.
    uint64_t cellTransitions() const { return cellTransitions_; }

private:
    int32_t mapSize_;
    int32_t aoi_;

    Grid grid_;
    std::vector<Entity> entities_;  // 크기 고정. id == 인덱스, erase하지 않는다
    uint64_t cellTransitions_ = 0;

    std::mutex slotMutex_;
    std::vector<uint32_t> freeSlots_;

    std::mutex cmdMutex_;
    std::vector<Command> pending_;
};

// narrow phase. naive와 grid가 같은 판정을 쓰도록 여기 한 곳에만 둔다.
inline bool inAoi(const Entity& a, const Entity& b, int32_t aoi) {
    int32_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    int32_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx <= aoi && dy <= aoi;
}
