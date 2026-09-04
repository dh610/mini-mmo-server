#include "Packet.h"

constexpr int kBitsPerByte = 8;

// 상위 바이트부터 이어붙인다 (네트워크 바이트 오더).
// 0xFFu가 unsigned인 것은 의도적 - 부호 있는 int를 시프트하면 구현 정의 동작이 된다.
void appendBigEndian(size_t size, uint32_t var, std::vector<uint8_t>& out) {
    while(size) {
        size -= sizeof(uint8_t);
        uint8_t byte = (var & (0xFFu << size * kBitsPerByte)) >> (size * kBitsPerByte);
        out.push_back(byte);
    }
}

void serializeHeader (const PacketHeader& header, std::vector<uint8_t>& out) {
    appendBigEndian(sizeof(header.length), header.length, out);
    appendBigEndian(sizeof(header.type), header.type, out);
    appendBigEndian(sizeof(header.sequence), header.sequence, out);
}

// idx는 참조로 받아 호출마다 전진한다. 범위를 벗어나면 at()이 std::out_of_range를 던진다.
// static_cast<uint32_t>가 없으면 uint8_t가 int로 승격돼 24비트 시프트에서 부호 문제가 생긴다.
uint32_t readBigEndian(size_t size, int& idx, const std::vector<uint8_t>& in) {
    uint32_t ret = 0;
    while(size) {
        size -= sizeof(uint8_t);
        ret |= static_cast<uint32_t>(in.at(idx++)) << (size * kBitsPerByte);
    }
    return ret;
}

// 중괄호 초기화는 원소가 왼쪽에서 오른쪽으로 평가되는 것이 보장된다.
// idx가 참조로 전진하므로 이 순서에 의존한다 (함수 인자 목록이면 순서가 미보장이라 안 된다).
PacketHeader deserializeHeader(const std::vector<uint8_t>& in, size_t startOffset) {
    int idx = startOffset;
    return {
        static_cast<uint16_t>(readBigEndian(sizeof(PacketHeader::length), idx, in)),
        static_cast<uint16_t>(readBigEndian(sizeof(PacketHeader::type), idx, in)),
        readBigEndian(sizeof(PacketHeader::sequence), idx, in)
    };
}

void patchLength(std::vector<uint8_t>& out) {
    uint16_t total = static_cast<uint16_t>(out.size());
    out[0] = static_cast<uint8_t>((total >> kBitsPerByte) & 0xFFu);
    out[1] = static_cast<uint8_t>(total & 0xFFu);
}
