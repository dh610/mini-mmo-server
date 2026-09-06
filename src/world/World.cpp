#include "World.h"

Grid::Grid(int32_t mapSize, int32_t cellSize)
    : cellSize_(cellSize > 0 ? cellSize : 1) {
    dim_ = mapSize / cellSize_ + 1;
    cells_.resize(static_cast<size_t>(dim_) * static_cast<size_t>(dim_));
}

int32_t Grid::cellOf(int32_t coord) const {
    int32_t c = coord / cellSize_;
    if (c < 0) return 0;
    if (c >= dim_) return dim_ - 1;
    return c;
}

void Grid::insert(int32_t cx, int32_t cy, uint32_t id) {
    cells_[static_cast<size_t>(cy) * static_cast<size_t>(dim_) + static_cast<size_t>(cx)]
        .push_back(id);
}

void Grid::remove(int32_t cx, int32_t cy, uint32_t id) {
    std::vector<uint32_t>& v =
        cells_[static_cast<size_t>(cy) * static_cast<size_t>(dim_) + static_cast<size_t>(cx)];
    // 순서가 의미 없으므로 swap-and-pop. erase면 뒤를 전부 당겨야 한다.
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == id) {
            v[i] = v.back();
            v.pop_back();
            return;
        }
    }
}

World::World(int32_t mapSize, int32_t aoiHalfExtent, int32_t cellSize, uint32_t maxEntities)
    : mapSize_(mapSize), aoi_(aoiHalfExtent), grid_(mapSize, cellSize), entities_(maxEntities) {
    freeSlots_.reserve(maxEntities);
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
    // 큐를 바꿔치기하고 락을 즉시 놓는다. 쥔 채로 처리하면 수신 스레드가 전부 막힌다.
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
                grid_.insert(grid_.cellOf(e.x), grid_.cellOf(e.y), e.id);
                break;
            case CmdType::Move: {
                if (!e.active) break;
                // 클라이언트 좌표는 신뢰하지 않는다.
                int32_t nx = c.x < 0 ? 0 : (c.x > mapSize_ ? mapSize_ : c.x);
                int32_t ny = c.y < 0 ? 0 : (c.y > mapSize_ ? mapSize_ : c.y);

                int32_t oldCx = grid_.cellOf(e.x), oldCy = grid_.cellOf(e.y);
                int32_t newCx = grid_.cellOf(nx), newCy = grid_.cellOf(ny);
                if (oldCx != newCx || oldCy != newCy) {
                    grid_.remove(oldCx, oldCy, e.id);
                    grid_.insert(newCx, newCy, e.id);
                    ++cellTransitions_;
                }
                e.x = nx;
                e.y = ny;
                break;
            }
            case CmdType::Despawn:
                if (!e.active) break;
                grid_.remove(grid_.cellOf(e.x), grid_.cellOf(e.y), e.id);
                e.active = false;
                break;
        }
    }
}

void World::collectAll(const Entity& me, std::vector<uint32_t>& out, uint64_t& candidates) const {
    for (const Entity& other : entities_) {
        if (!other.active || other.id == me.id) continue;
        ++candidates;
        out.push_back(other.id);
    }
}

void World::collectNaive(const Entity& me, std::vector<uint32_t>& out, uint64_t& candidates) const {
    for (const Entity& other : entities_) {
        if (!other.active || other.id == me.id) continue;
        ++candidates;  // naive의 broad phase는 활성 entity 전부
        if (inAoi(me, other, aoi_)) out.push_back(other.id);
    }
}

void World::collectGrid(const Entity& me, std::vector<uint32_t>& out, uint64_t& candidates) const {
    // AOI 정사각형이 걸치는 셀 범위. cell == aoi면 3x3이 된다.
    int32_t minCx = grid_.cellOf(me.x - aoi_), maxCx = grid_.cellOf(me.x + aoi_);
    int32_t minCy = grid_.cellOf(me.y - aoi_), maxCy = grid_.cellOf(me.y + aoi_);

    for (int32_t cy = minCy; cy <= maxCy; ++cy) {
        for (int32_t cx = minCx; cx <= maxCx; ++cx) {
            for (uint32_t id : grid_.at(cx, cy)) {
                const Entity& other = entities_[id];
                if (!other.active || other.id == me.id) continue;
                ++candidates;
                if (inAoi(me, other, aoi_)) out.push_back(other.id);
            }
        }
    }
}
