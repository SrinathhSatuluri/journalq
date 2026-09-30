#include <journalq/crc32c.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

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
    using journalq::Crc32c;

    // Check value from the CRC catalogue: CRC-32C("123456789") = 0xE3069283.
    constexpr std::uint32_t k_CHECK = 0xE3069283u;

    check(Crc32c::calculate("123456789", 9) == k_CHECK, "catalogue check");
    check(Crc32c::calculate("", 0) == 0u, "empty input is zero");

    // Two calls over consecutive halves equal one call over the whole.
    const std::uint32_t head = Crc32c::calculate("12345", 5);
    check(Crc32c::calculate("6789", 4, head) == k_CHECK, "incremental update");

    // A single flipped bit changes the checksum.
    char corrupted[] = "123456789";
    corrupted[3]     = static_cast<char>(corrupted[3] ^ 0x01);
    check(Crc32c::calculate(corrupted, 9) != k_CHECK, "bit flip detected");

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
