#include "Packet.h"

constexpr int kBitsPerByte = 8;

// 값에서 상위 바이트부터 하나씩 뽑아 out에 이어붙인다 (네트워크 바이트 오더).
// 0xFF가 아니라 0xFFu인 이유: 부호 있는 int를 시프트해 부호 비트를 침범하면
// 구현 정의 동작이 된다 — unsigned면 그 문제 자체가 없음.
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

// appendBigEndian의 역방향. idx는 참조로 받아 호출마다 전진시킨다.
// in.at()이 범위를 벗어나면 std::out_of_range를 던지고 호출자까지 그대로 전파된다.
// static_cast<uint32_t>가 시프트 전에 필요한 이유: uint8_t가 <<에서 int로 승격되는데,
// 승격된 채로 24비트 시프트하면 위 0xFFu와 같은 부호 문제가 생긴다.
uint32_t readBigEndian(size_t size, int& idx, const std::vector<uint8_t>& in) {
    uint32_t ret = 0;
    while(size) {
        size -= sizeof(uint8_t);
        ret |= static_cast<uint32_t>(in.at(idx++)) << (size * kBitsPerByte);
    }
    return ret;
}

// return {...} 중괄호 초기화의 원소는 왼쪽에서 오른쪽 순서 평가가 표준에 보장돼 있어,
// idx가 참조로 누적 전진하는 부작용이 순서대로 안전하게 반영된다 (일반 함수 인자 목록은 순서 미보장).
PacketHeader deserializeHeader(const std::vector<uint8_t>& in, size_t startOffset) {
    int idx = startOffset;
    return {
        static_cast<uint16_t>(readBigEndian(sizeof(PacketHeader::length), idx, in)),
        static_cast<uint16_t>(readBigEndian(sizeof(PacketHeader::type), idx, in)),
        readBigEndian(sizeof(PacketHeader::sequence), idx, in)
    };
}
