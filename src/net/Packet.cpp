#include "Packet.h"

constexpr int kBitsPerByte = 8;

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

uint32_t readBigEndian(size_t size, int& idx, const std::vector<uint8_t>& in) {
    uint32_t ret = 0;
    while(size) {
        size -= sizeof(uint8_t);
        ret |= static_cast<uint32_t>(in.at(idx++)) << (size * kBitsPerByte);
    }
    return ret;
}

PacketHeader deserializeHeader(const std::vector<uint8_t>& in) {
    int idx = 0;
    return {
        static_cast<uint16_t>(readBigEndian(sizeof(PacketHeader::length), idx, in)),
        static_cast<uint16_t>(readBigEndian(sizeof(PacketHeader::type), idx, in)),
        readBigEndian(sizeof(PacketHeader::sequence), idx, in)
    };
}
