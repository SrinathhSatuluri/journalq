#ifndef INCLUDED_JOURNALQ_PROTOCOLUTIL
#define INCLUDED_JOURNALQ_PROTOCOLUTIL

// Provide the padding rules shared by every variable-size record.
//
// A record in the data file is padded to a multiple of 8 bytes, and every
// padding byte holds the padding length. Padding is never empty: a record
// whose payload already ends on a boundary gets a full 8 bytes. That is what
// lets a reader recover the payload length from the record alone: the last
// byte says how much of the record is padding, and a value outside 1 to 8 is
// proof that the record is not what its header claims.
//
// The word-sized flavor, padding to 4 bytes, follows the same rule with a
// range of 1 to 4.

#include <journalq/protocol.h>

#include <cassert>
#include <cstddef>
#include <cstring>

namespace journalq {

struct ProtocolUtil {
    // CLASS METHODS

    /// Return the number of 8-byte units needed to hold `length` bytes plus
    /// padding, and load into `*numPaddingBytes` the padding length, which
    /// is at least 1 and at most `Protocol::k_DWORD_SIZE`.
    static std::size_t calcNumDwordsAndPadding(std::size_t* numPaddingBytes,
                                               std::size_t  length) noexcept;

    /// Return the number of 4-byte units needed to hold `length` bytes plus
    /// padding, and load into `*numPaddingBytes` the padding length, which
    /// is at least 1 and at most `Protocol::k_WORD_SIZE`.
    static std::size_t calcNumWordsAndPadding(std::size_t* numPaddingBytes,
                                              std::size_t  length) noexcept;

    /// Write `numPaddingBytes` bytes at `destination`, each holding the
    /// value `numPaddingBytes`. The behavior is undefined unless
    /// `numPaddingBytes` is between 1 and `Protocol::k_DWORD_SIZE`.
    static void appendPaddingDwordRaw(unsigned char* destination,
                                      std::size_t    numPaddingBytes) noexcept;

    /// Write `numPaddingBytes` bytes at `destination`, each holding the
    /// value `numPaddingBytes`. The behavior is undefined unless
    /// `numPaddingBytes` is between 1 and `Protocol::k_WORD_SIZE`.
    static void appendPaddingRaw(unsigned char* destination,
                                 std::size_t    numPaddingBytes) noexcept;

    /// Return the length of the payload within the `paddedLength` bytes at
    /// `data`, read from the padding byte at the end. The behavior is
    /// undefined unless `paddedLength` is at least 1 and the last byte is a
    /// valid padding length, which a reader must check before calling this.
    static std::size_t calcUnpaddedLength(const unsigned char* data,
                                          std::size_t paddedLength) noexcept;
};

// ============================================================================
//                             INLINE DEFINITIONS
// ============================================================================

inline std::size_t
ProtocolUtil::calcNumDwordsAndPadding(std::size_t* numPaddingBytes,
                                      std::size_t  length) noexcept
{
    // Adding a full unit before dividing guarantees at least one byte of
    // padding, so the padding byte always exists.
    const std::size_t numDwords =
        (length + Protocol::k_DWORD_SIZE) / Protocol::k_DWORD_SIZE;
    *numPaddingBytes = numDwords * Protocol::k_DWORD_SIZE - length;
    return numDwords;
}

inline std::size_t
ProtocolUtil::calcNumWordsAndPadding(std::size_t* numPaddingBytes,
                                     std::size_t  length) noexcept
{
    const std::size_t numWords =
        (length + Protocol::k_WORD_SIZE) / Protocol::k_WORD_SIZE;
    *numPaddingBytes = numWords * Protocol::k_WORD_SIZE - length;
    return numWords;
}

inline void
ProtocolUtil::appendPaddingDwordRaw(unsigned char* destination,
                                    std::size_t    numPaddingBytes) noexcept
{
    assert(numPaddingBytes >= 1);
    assert(numPaddingBytes <= static_cast<std::size_t>(Protocol::k_DWORD_SIZE));

    std::memset(destination,
                static_cast<int>(numPaddingBytes),
                numPaddingBytes);
}

inline void ProtocolUtil::appendPaddingRaw(unsigned char* destination,
                                           std::size_t numPaddingBytes) noexcept
{
    assert(numPaddingBytes >= 1);
    assert(numPaddingBytes <= static_cast<std::size_t>(Protocol::k_WORD_SIZE));

    std::memset(destination,
                static_cast<int>(numPaddingBytes),
                numPaddingBytes);
}

inline std::size_t
ProtocolUtil::calcUnpaddedLength(const unsigned char* data,
                                 std::size_t          paddedLength) noexcept
{
    assert(paddedLength >= 1);
    assert(data[paddedLength - 1] >= 1);
    assert(data[paddedLength - 1] <= paddedLength);

    return paddedLength - data[paddedLength - 1];
}

}  // namespace journalq

#endif
