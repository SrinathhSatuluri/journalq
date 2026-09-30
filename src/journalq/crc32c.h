#ifndef INCLUDED_JOURNALQ_CRC32C
#define INCLUDED_JOURNALQ_CRC32C

// Provide the CRC-32C (Castagnoli) checksum used on every on-disk record.
//
// The checksum is computed over a record's payload when it is written and
// verified when it is read back, so a record whose bytes changed on disk is
// rejected during recovery instead of being served.
//
// The implementation is a table-driven software routine. It is correct on any
// platform and fast enough for the write path at the sizes this log handles;
// a hardware-accelerated variant can replace it behind the same interface.

#include <cstddef>
#include <cstdint>

namespace journalq {

class Crc32c {
  public:
    // Return the CRC-32C of the `length` bytes at `data`, continuing from
    // `crc`, the value returned by a previous call over the preceding bytes.
    // Pass 0 (the default) to start a new checksum. Checksumming a buffer in
    // several calls yields the same value as checksumming it in one call.
    static std::uint32_t calculate(const void*   data,
                                   std::size_t   length,
                                   std::uint32_t crc = 0) noexcept;
};

}  // namespace journalq

#endif
