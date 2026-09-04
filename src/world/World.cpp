#include "World.h"

World::World(int32_t mapSize, int32_t aoiHalfExtent, uint32_t maxEntities)
    : mapSize_(mapSize), aoi_(aoiHalfExtent), entities_(maxEntities) {
    freeSlots_.reserve(maxEntities);
    // 뒤에서 pop_back으로 꺼내므로 역순으로 채워 0번부터 배정되게 한다.
    for (uint32_t i = maxEntities; i > 0; --i) {
        entities_[i - 1].id = i - 1;
        freeSlots_.push_back(i - 1);
    }
    pending_.reserve(maxEntities * 2);
}

bool World::reserveId(uint32_t& outId) {
    std::lock_guard<std::mutex> lock(slotMutex_);
    if (freeSlots_.empty()) return false;
    outId = freeSlots_.back();
    freeSlots_.pop_back();
    return true;
}

void World::releaseId(uint32_t id) {
    std::lock_guard<std::mutex> lock(slotMutex_);
    freeSlots_.push_back(id);
}

void World::pushCommand(const Command& cmd) {
    std::lock_guard<std::mutex> lock(cmdMutex_);
    pending_.push_back(cmd);
}

void World::applyCommands() {
    // 큐를 통째로 바꿔치기하고 락을 즉시 놓는다. 적용은 락 밖에서 —
    // 락을 쥔 채 처리하면 그동안 수신 스레드 전부가 pushCommand에서 막힌다.
    std::vector<Command> batch;
    {
        std::lock_guard<std::mutex> lock(cmdMutex_);
        batch.swap(pending_);
    }

    for (const Command& c : batch) {
        if (c.id >= entities_.size()) continue;
        Entity& e = entities_[c.id];
        switch (c.type) {
            case CmdType::Spawn:
                e.x = c.x;
                e.y = c.y;
                e.active = true;
                break;
            case CmdType::Move:
                if (!e.active) break;
                // 맵 밖으로 나가지 않게 자른다. 클라이언트를 믿지 않는다는 뜻이기도 하다.
                e.x = c.x < 0 ? 0 : (c.x > mapSize_ ? mapSize_ : c.x);
                e.y = c.y < 0 ? 0 : (c.y > mapSize_ ? mapSize_ : c.y);
                break;
            case CmdType::Despawn:
                e.active = false;
                break;
        }
    }
}

void World::collectNaive(const Entity& me, std::vector<uint32_t>& out, uint64_t& candidates) const {
    for (const Entity& other : entities_) {
        if (!other.active || other.id == me.id) continue;
        ++candidates;  // naive의 broad phase는 "전원" — 활성 entity 전부가 후보다
        if (inAoi(me, other, aoi_)) out.push_back(other.id);
    }
}
