#include "Packet.h"

constexpr int kBitsPerByte = 8;

// 값에서 상위 바이트부터 하나씩 뽑아 out에 이어붙인다 (네트워크 바이트 오더).
// 0xFF 대신 0xFFu를 쓰는 이유: 부호 있는 int를 시프트해 부호 비트를 침범하면
// 구현 정의 동작이 되고 -Wall 경고 대상이 된다. unsigned로 두면 그 문제 자체가 없다.
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

// appendBigEndian의 역방향: idx부터 바이트를 하나씩 읽어 값 하나로 합친다.
// idx는 참조로 받아 호출마다 전진시킨다. in.at()은 범위를 벗어나면
// std::out_of_range를 던지므로, 잘못된(너무 짧은) 패킷이 들어오면 이 함수가
// 즉시 중단되고 예외가 deserializeHeader를 거쳐 호출자까지 그대로 전파된다.
// static_cast<uint32_t>를 시프트 전에 거는 이유: in.at()의 uint8_t가 <<에서
// int로 정수 승격되는데, 승격 상태로 24비트 시프트하면 위 0xFFu와 같은 문제가 생긴다.
uint32_t readBigEndian(size_t size, int& idx, const std::vector<uint8_t>& in) {
    uint32_t ret = 0;
    while(size) {
        size -= sizeof(uint8_t);
        ret |= static_cast<uint32_t>(in.at(idx++)) << (size * kBitsPerByte);
    }
    return ret;
}

// return {...}로 세 필드를 한 번에 초기화하는 이유: 함수 인자 목록과 달리
// 중괄호 초기화 리스트의 원소는 왼쪽에서 오른쪽 순서로 평가되는 것이 표준에
// 보장되어 있어, idx가 참조로 누적 전진하는 부작용이 순서대로 안전하게 반영된다.
PacketHeader deserializeHeader(const std::vector<uint8_t>& in) {
    int idx = 0;
    return {
        static_cast<uint16_t>(readBigEndian(sizeof(PacketHeader::length), idx, in)),
        static_cast<uint16_t>(readBigEndian(sizeof(PacketHeader::type), idx, in)),
        readBigEndian(sizeof(PacketHeader::sequence), idx, in)
    };
}
