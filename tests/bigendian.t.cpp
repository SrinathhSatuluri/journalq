#include <journalq/bigendian.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main()
{
    using journalq::BigEndianUint16;
    using journalq::BigEndianUint32;

    // A default-constructed field is zero, so a zeroed struct is a valid one.
    {
        BigEndianUint16 u16;
        BigEndianUint32 u32;
        check(u16 == 0, "u16 default is zero");
        check(u32 == 0, "u32 default is zero");
    }

    // The bytes in memory are the big-endian encoding, most significant
    // byte first, regardless of the host byte order.
    {
        BigEndianUint16 u16(0x0102);
        BigEndianUint32 u32(0x01020304u);

        unsigned char raw16[2];
        unsigned char raw32[4];
        std::memcpy(raw16, &u16, sizeof raw16);
        std::memcpy(raw32, &u32, sizeof raw32);

        check(raw16[0] == 0x01 && raw16[1] == 0x02, "u16 byte order");
        check(raw32[0] == 0x01 && raw32[1] == 0x02 && raw32[2] == 0x03 &&
                  raw32[3] == 0x04,
              "u32 byte order");
    }

    // Assignment and conversion round-trip every bit, including the top one.
    {
        BigEndianUint16 u16;
        BigEndianUint32 u32;

        u16 = 0xFFFF;
        u32 = 0xFFFFFFFFu;
        check(u16 == 0xFFFF, "u16 all bits");
        check(u32 == 0xFFFFFFFFu, "u32 all bits");

        u16 = 0x8000;
        u32 = 0x80000000u;
        check(u16 == 0x8000, "u16 top bit");
        check(u32 == 0x80000000u, "u32 top bit");
    }

    // A field reads as an integer in expressions.
    {
        BigEndianUint32 words(3);
        check(words * 4 == 12, "u32 in arithmetic");
        check((words & 0x1u) == 1, "u32 in bit operations");
    }

    // Bytes read from a file decode the same way they were written.
    {
        const unsigned char onDisk[4] = {0xDE, 0xAD, 0xBE, 0xEF};
        BigEndianUint32     u32;
        std::memcpy(&u32, onDisk, sizeof onDisk);
        check(u32 == 0xDEADBEEFu, "decode from raw bytes");
    }

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
