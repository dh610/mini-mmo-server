#pragma once
#include <cstdint>
#include <vector>

// 구조체를 memcpy로 통째 전송하지 않는 이유: 패딩/정렬이 컴파일러마다 다를 수 있고,
// 엔디안이 안 맞으면 서버·봇이 같은 저장소여도 어긋날 수 있다. 필드별로 손직렬화한다.
struct PacketHeader {
    uint16_t length;
    uint16_t type;
    uint32_t sequence;
};

void serializeHeader (const PacketHeader& header, std::vector<uint8_t>& out);
PacketHeader deserializeHeader(const std::vector<uint8_t>& in);
