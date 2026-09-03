#pragma once
#include <cstdint>
#include <vector>

struct PacketHeader {
    uint16_t length;
    uint16_t type;
    uint32_t sequence;
};

void serializeHeader (const PacketHeader& header, std::vector<uint8_t>& out);
PacketHeader deserializeHeader(const std::vector<uint8_t>& in);
