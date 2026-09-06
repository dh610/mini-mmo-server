#include <algorithm>

#include "doctest.h"
#include "world/World.h"

namespace {

// tick 스레드가 하는 일을 테스트에서 직접 흉내낸다.
void spawn(World& w, uint32_t id, int32_t x, int32_t y) {
    w.pushCommand({CmdType::Spawn, id, x, y});
    w.applyCommands();
}

void move(World& w, uint32_t id, int32_t x, int32_t y) {
    w.pushCommand({CmdType::Move, id, x, y});
    w.applyCommands();
}

std::vector<uint32_t> sorted(std::vector<uint32_t> v) {
    std::sort(v.begin(), v.end());
    return v;
}

std::vector<uint32_t> naiveOf(const World& w, uint32_t id) {
    std::vector<uint32_t> out;
    uint64_t c = 0;
    w.collectNaive(w.entities()[id], out, c);
    return sorted(out);
}

std::vector<uint32_t> gridOf(const World& w, uint32_t id) {
    std::vector<uint32_t> out;
    uint64_t c = 0;
    w.collectGrid(w.entities()[id], out, c);
    return sorted(out);
}

}  // namespace

TEST_CASE("AOI 경계는 포함이다") {
    World w(3000, 500, 500, 16);
    spawn(w, 0, 1000, 1000);
    spawn(w, 1, 1500, 1000);  // dx == aoi
    spawn(w, 2, 1501, 1000);  // dx == aoi + 1
    spawn(w, 3, 1500, 1500);  // 모서리, dx == dy == aoi

    std::vector<uint32_t> v = naiveOf(w, 0);
    CHECK(std::find(v.begin(), v.end(), 1u) != v.end());
    CHECK(std::find(v.begin(), v.end(), 3u) != v.end());
    CHECK(std::find(v.begin(), v.end(), 2u) == v.end());
}

TEST_CASE("naive와 grid는 같은 결과를 낸다") {
    World w(3000, 500, 500, 64);
    // 셀 경계, 맵 끝, 같은 좌표를 일부러 섞는다.
    const int32_t xs[] = {0, 499, 500, 501, 1000, 1499, 1500, 2999, 1000, 250};
    const int32_t ys[] = {0, 500, 500, 999, 1000, 1000, 1501, 2999, 1000, 2750};
    for (uint32_t i = 0; i < 10; ++i) spawn(w, i, xs[i], ys[i]);

    for (uint32_t i = 0; i < 10; ++i) {
        CAPTURE(i);
        CHECK(naiveOf(w, i) == gridOf(w, i));
    }
}

TEST_CASE("셀을 옮겨도 grid가 어긋나지 않는다") {
    World w(3000, 500, 500, 64);
    spawn(w, 0, 100, 100);
    spawn(w, 1, 2900, 2900);
    CHECK(gridOf(w, 0).empty());

    // 1을 0 근처로 끌고 온다. 셀을 여러 번 건너뛴다.
    move(w, 1, 200, 200);
    CHECK(gridOf(w, 0) == std::vector<uint32_t>{1});
    CHECK(naiveOf(w, 0) == gridOf(w, 0));
    CHECK(w.cellTransitions() > 0);

    // 다시 멀리 보낸다.
    move(w, 1, 2900, 2900);
    CHECK(gridOf(w, 0).empty());
}

TEST_CASE("같은 좌표에 여러 명이 있어도 자기 자신은 빠진다") {
    World w(3000, 500, 500, 64);
    for (uint32_t i = 0; i < 5; ++i) spawn(w, i, 1000, 1000);

    for (uint32_t i = 0; i < 5; ++i) {
        std::vector<uint32_t> v = gridOf(w, i);
        CHECK(v.size() == 4);
        CHECK(std::find(v.begin(), v.end(), i) == v.end());
    }
}

TEST_CASE("despawn하면 grid에서도 빠진다") {
    World w(3000, 500, 500, 64);
    spawn(w, 0, 1000, 1000);
    spawn(w, 1, 1100, 1100);
    CHECK(gridOf(w, 0).size() == 1);

    w.pushCommand({CmdType::Despawn, 1, 0, 0});
    w.applyCommands();
    CHECK(gridOf(w, 0).empty());
    CHECK(naiveOf(w, 0).empty());

    // 슬롯을 재사용해도 유령이 남지 않는다.
    spawn(w, 1, 2900, 2900);
    CHECK(gridOf(w, 0).empty());
}

TEST_CASE("맵 밖 좌표는 잘린다") {
    World w(3000, 500, 500, 16);
    spawn(w, 0, 1000, 1000);
    move(w, 0, -5000, 99999);
    CHECK(w.entities()[0].x == 0);
    CHECK(w.entities()[0].y == 3000);

    // 잘린 뒤에도 grid 소속이 맞아야 한다.
    spawn(w, 1, 100, 2900);
    CHECK(naiveOf(w, 0) == gridOf(w, 0));
}
