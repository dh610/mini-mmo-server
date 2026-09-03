#pragma once
#include <cstdint>
#include <vector>

// 구조체 통째 memcpy 대신 필드별 손직렬화하는 이유: 패딩/정렬은 컴파일러마다
// 다를 수 있고 엔디안도 안 맞을 수 있다. length는 헤더 포함 패킷 전체 길이.
struct PacketHeader {
    uint16_t length;
    uint16_t type;
    uint32_t sequence;
};

// out 뒤에 이어붙인다 (out을 비우지 않음).
void serializeHeader (const PacketHeader& header, std::vector<uint8_t>& out);

// in의 startOffset부터 읽는다 (부분 복사 없이 누적 버퍼에서 바로 파싱하기 위함).
// 남은 바이트가 부족하면 std::out_of_range.
PacketHeader deserializeHeader(const std::vector<uint8_t>& in, size_t startOffset);
