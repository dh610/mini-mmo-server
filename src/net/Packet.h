#pragma once
#include <cstdint>
#include <vector>

// 헤더의 type 필드에 실리는 값. 이걸 보고 어느 파서를 부를지 정한다.
enum class PacketType : uint16_t {
    Hello = 1,       // S->C  네 id는 이거다 (접속 직후 1회)
    Move = 10,       // C->S  이 좌표로 이동하겠다
    Snapshot = 20,   // S->C  지금 네 AOI 안 상태
};

// 필드별 직렬화. length는 헤더를 포함한 패킷 전체 길이다.
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

void appendBigEndian(size_t size, uint32_t var, std::vector<uint8_t>& out);
uint32_t readBigEndian(size_t size, int& idx, const std::vector<uint8_t>& in);

void patchLength(std::vector<uint8_t>& out);
