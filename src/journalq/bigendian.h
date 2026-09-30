#ifndef INCLUDED_JOURNALQ_BIGENDIAN
#define INCLUDED_JOURNALQ_BIGENDIAN

// Provide unsigned integer types stored in big-endian byte order.
//
// Every on-disk structure in this project is composed of these types. Each
// one occupies exactly its width in bytes, needs no alignment beyond a single
// byte, and converts to and from the host integer only when accessed. A struct
// built from them therefore has the same byte layout on every platform and can
// be read straight out of a file, whatever the byte order of the machine that
// wrote it.
//
// The conversion operators are implicit on purpose: a field reads like an
// integer in expressions and is assigned like one, so record code stays free
// of explicit unpacking.

#include <cstddef>
#include <cstdint>

namespace journalq {

class BigEndianUint16 {
  private:
    unsigned char d_bytes[2] = {0, 0};

  public:
    BigEndianUint16() = default;

    explicit BigEndianUint16(std::uint16_t value) noexcept { *this = value; }

    BigEndianUint16& operator=(std::uint16_t value) noexcept
    {
        d_bytes[0] = static_cast<unsigned char>(value >> 8);
        d_bytes[1] = static_cast<unsigned char>(value);
        return *this;
    }

    operator std::uint16_t() const noexcept
    {
        return static_cast<std::uint16_t>((d_bytes[0] << 8) | d_bytes[1]);
    }
};

class BigEndianUint32 {
  private:
    unsigned char d_bytes[4] = {0, 0, 0, 0};

  public:
    BigEndianUint32() = default;

    explicit BigEndianUint32(std::uint32_t value) noexcept { *this = value; }

    BigEndianUint32& operator=(std::uint32_t value) noexcept
    {
        d_bytes[0] = static_cast<unsigned char>(value >> 24);
        d_bytes[1] = static_cast<unsigned char>(value >> 16);
        d_bytes[2] = static_cast<unsigned char>(value >> 8);
        d_bytes[3] = static_cast<unsigned char>(value);
        return *this;
    }

    operator std::uint32_t() const noexcept
    {
        return (static_cast<std::uint32_t>(d_bytes[0]) << 24) |
               (static_cast<std::uint32_t>(d_bytes[1]) << 16) |
               (static_cast<std::uint32_t>(d_bytes[2]) << 8) |
               static_cast<std::uint32_t>(d_bytes[3]);
    }
};

static_assert(sizeof(BigEndianUint16) == 2);
static_assert(sizeof(BigEndianUint32) == 4);
static_assert(alignof(BigEndianUint16) == 1);
static_assert(alignof(BigEndianUint32) == 1);

}  // namespace journalq

#endif
