#pragma once
#include <cstdint>
#include <mutex>
#include <vector>

// 좌표는 정수 단위. AOI가 정사각형이라 판정이 |dx|<=a && |dy|<=a 로 끝나고 곱셈이 없다
// (원이었다면 dx*dx+dy*dy<=R*R — 후보 수만큼 반복되는 코드라 이 차이가 곱해진다).
struct Entity {
    uint32_t id = 0;
    int32_t x = 0;
    int32_t y = 0;
    bool active = false;
};

enum class CmdType : uint8_t { Spawn, Move, Despawn };

// 수신 스레드가 큐에 넣고 tick 스레드만 꺼내 적용한다.
// 게임 상태를 수신 스레드가 직접 만지지 않는 것이 이 서버의 유일한 동시성 규약이다.
struct Command {
    CmdType type;
    uint32_t id;
    int32_t x;
    int32_t y;
};

class World {
public:
    // maxEntities만큼 슬롯을 미리 잡아둔다. 실행 중 vector가 커지지 않게 하려는 것 —
    // 커지면 재할당이 일어나고, tick 스레드가 순회하는 도중이면 그대로 터진다.
    // id를 곧 슬롯 인덱스로 쓰기 때문에, 나중에 grid가 id를 담아도 dangling이 생길 수 없다
    // (포인터를 담았다면 재할당 한 번에 전부 무효가 된다).
    World(int32_t mapSize, int32_t aoiHalfExtent, uint32_t maxEntities);

    int32_t mapSize() const { return mapSize_; }
    int32_t aoi() const { return aoi_; }

    // --- 수신 스레드에서 호출 ---
    // 슬롯을 예약하고 id를 즉시 돌려준다. 실제 배치는 tick이 Spawn 명령을 처리할 때.
    // 클라이언트에게 id를 바로 회신해야 해서 예약과 배치를 분리했다.
    bool reserveId(uint32_t& outId);
    void releaseId(uint32_t id);
    void pushCommand(const Command& cmd);

    // --- tick 스레드에서만 호출 ---
    void applyCommands();
    const std::vector<Entity>& entities() const { return entities_; }

    // me 주변 AOI 안에 있는 entity의 id를 out에 채운다.
    // candidates에는 broad phase가 넘긴 후보 수를 더한다 — naive는 "전원"이 후보다.
    void collectNaive(const Entity& me, std::vector<uint32_t>& out, uint64_t& candidates) const;

private:
    int32_t mapSize_;
    int32_t aoi_;

    std::vector<Entity> entities_;  // 크기 고정. erase하지 않는다

    std::mutex slotMutex_;  // 수신 스레드들끼리의 슬롯 경쟁만 보호
    std::vector<uint32_t> freeSlots_;

    std::mutex cmdMutex_;
    std::vector<Command> pending_;
};

// broad phase가 넘긴 후보를 실제 좌표로 거르는 narrow phase.
// naive든 grid든 이 판정은 동일해야 비교가 공정하다. 그래서 한 곳에만 둔다.
inline bool inAoi(const Entity& a, const Entity& b, int32_t aoi) {
    int32_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    int32_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx <= aoi && dy <= aoi;
}
