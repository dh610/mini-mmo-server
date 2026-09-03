#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "net/Packet.h"

namespace {

void checkRoundTrip(const PacketHeader& original) {
    std::vector<uint8_t> buf;
    serializeHeader(original, buf);

    PacketHeader restored = deserializeHeader(buf, 0);

    CHECK(restored.length == original.length);
    CHECK(restored.type == original.type);
    CHECK(restored.sequence == original.sequence);
}

}  // namespace

TEST_CASE("PacketHeader 왕복 - 전부 0 (최소값 스모크 테스트)") {
    checkRoundTrip({0, 0, 0});
}

TEST_CASE("PacketHeader 왕복 - 필드마다 다른 바이트 패턴 (바이트 순서 뒤바뀜 검출)") {
    checkRoundTrip({0x1234, 0xABCD, 0x12345678});
}

TEST_CASE("PacketHeader 왕복 - sequence 최상위 비트 켜짐 (부호 시프트 회귀 검증)") {
    checkRoundTrip({1, 1, 0x80000000});
}

TEST_CASE("PacketHeader 왕복 - 필드별 최대값 (마스킹/오버플로우 검증)") {
    checkRoundTrip({0xFFFF, 0xFFFF, 0xFFFFFFFF});
}
