#pragma once
#include <cstdint>
#include <vector>

// 헤더의 type 필드에 실리는 값. 이걸 보고 어느 파서를 부를지 정한다.
enum class PacketType : uint16_t {
    Hello = 1,       // S->C  네 id는 이거다 (접속 직후 1회)
    Move = 10,       // C->S  이 좌표로 이동하겠다
    Snapshot = 20,   // S->C  지금 네 AOI 안 상태
};

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

// --- 페이로드 직렬화용 ---
// 헤더뿐 아니라 payload도 같은 방식으로 쓴다. 구조체를 통째로 보내지 않는 이유는
// Snapshot이 본질적으로 가변 길이라서다 — 주변 인원이 tick마다 다르다.

void appendBigEndian(size_t size, uint32_t var, std::vector<uint8_t>& out);
uint32_t readBigEndian(size_t size, int& idx, const std::vector<uint8_t>& in);

// out 앞부분(0번지)의 length 필드를 out.size()로 채운다.
// 페이로드를 다 쓴 뒤에야 전체 길이를 알 수 있어서, 헤더를 먼저 쓰고 나중에 되돌아와 메운다.
void patchLength(std::vector<uint8_t>& out);
