#include <journalq/crc32c.h>

#include <array>

namespace journalq {

namespace {

// Reflected form of the Castagnoli polynomial 0x1EDC6F41.
constexpr std::uint32_t k_POLYNOMIAL = 0x82F63B78u;

constexpr std::array<std::uint32_t, 256> makeTable()
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t value = i;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value & 1u) ? (k_POLYNOMIAL ^ (value >> 1)) : (value >> 1);
        }
        table[i] = value;
    }
    return table;
}

constexpr std::array<std::uint32_t, 256> k_TABLE = makeTable();

}  // namespace

std::uint32_t Crc32c::calculate(const void* data,
                                std::size_t length,
                                std::uint32_t crc) noexcept
{
    const auto* bytes = static_cast<const unsigned char*>(data);

    // The stored form is the inverted running value, so inverting a previous
    // result restores the state needed to continue from it.
    crc = ~crc;
    for (std::size_t i = 0; i < length; ++i) {
        crc = k_TABLE[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}

}  // namespace journalq
