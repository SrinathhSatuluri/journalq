#include <journalq/protocolutil.h>

#include <cstddef>
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

using namespace journalq;

// For every payload length up to a few units, padding is never empty, never
// longer than a unit, and lands the record exactly on a boundary.
void testPaddingArithmetic()
{
    for (std::size_t length = 0; length <= 40; ++length) {
        std::size_t       padding = 0;
        const std::size_t numDwords =
            ProtocolUtil::calcNumDwordsAndPadding(&padding, length);
        check(padding >= 1, "dword padding is never empty");
        check(padding <= static_cast<std::size_t>(Protocol::k_DWORD_SIZE),
              "dword padding is at most one unit");
        check((length + padding) % Protocol::k_DWORD_SIZE == 0,
              "dword padded length is aligned");
        check(numDwords * Protocol::k_DWORD_SIZE == length + padding,
              "dword count matches padded length");

        std::size_t       wordPadding = 0;
        const std::size_t numWords =
            ProtocolUtil::calcNumWordsAndPadding(&wordPadding, length);
        check(wordPadding >= 1, "word padding is never empty");
        check(wordPadding <= static_cast<std::size_t>(Protocol::k_WORD_SIZE),
              "word padding is at most one unit");
        check((length + wordPadding) % Protocol::k_WORD_SIZE == 0,
              "word padded length is aligned");
        check(numWords * Protocol::k_WORD_SIZE == length + wordPadding,
              "word count matches padded length");
    }

    // The boundary cases spelled out: a payload already on a boundary gets a
    // full unit, one byte short of a boundary gets a single byte.
    std::size_t padding = 0;
    ProtocolUtil::calcNumDwordsAndPadding(&padding, 0);
    check(padding == 8, "empty payload gets 8 bytes of dword padding");
    ProtocolUtil::calcNumDwordsAndPadding(&padding, 8);
    check(padding == 8, "aligned payload gets 8 bytes of dword padding");
    ProtocolUtil::calcNumDwordsAndPadding(&padding, 7);
    check(padding == 1, "payload one short gets 1 byte of dword padding");
    ProtocolUtil::calcNumWordsAndPadding(&padding, 4);
    check(padding == 4, "aligned payload gets 4 bytes of word padding");
    ProtocolUtil::calcNumWordsAndPadding(&padding, 3);
    check(padding == 1, "payload one short gets 1 byte of word padding");
}

// Every padding byte carries the padding length, so the last byte alone
// recovers the payload length.
void testPaddingBytes()
{
    const auto dword = static_cast<std::size_t>(Protocol::k_DWORD_SIZE);
    for (std::size_t n = 1; n <= dword; ++n) {
        unsigned char buffer[Protocol::k_DWORD_SIZE] = {};
        ProtocolUtil::appendPaddingDwordRaw(buffer, n);
        bool allEqual = true;
        for (std::size_t i = 0; i < n; ++i) {
            allEqual = allEqual && buffer[i] == n;
        }
        check(allEqual, "dword padding bytes hold the padding length");
        for (std::size_t i = n; i < sizeof buffer; ++i) {
            check(buffer[i] == 0, "dword padding writes only its own bytes");
        }
    }

    const auto word = static_cast<std::size_t>(Protocol::k_WORD_SIZE);
    for (std::size_t n = 1; n <= word; ++n) {
        unsigned char buffer[Protocol::k_WORD_SIZE] = {};
        ProtocolUtil::appendPaddingRaw(buffer, n);
        bool allEqual = true;
        for (std::size_t i = 0; i < n; ++i) {
            allEqual = allEqual && buffer[i] == n;
        }
        check(allEqual, "word padding bytes hold the padding length");
    }
}

// Writing a payload with its padding and reading the length back is the
// identity, for every length that fits in the buffer.
void testRoundTrip()
{
    unsigned char record[64];
    const auto    dword = static_cast<std::size_t>(Protocol::k_DWORD_SIZE);
    for (std::size_t length = 0; length + dword <= sizeof record; ++length) {
        for (std::size_t i = 0; i < length; ++i) {
            record[i] = static_cast<unsigned char>(0x80 | i);
        }
        std::size_t padding = 0;
        ProtocolUtil::calcNumDwordsAndPadding(&padding, length);
        ProtocolUtil::appendPaddingDwordRaw(record + length, padding);

        const std::size_t paddedLength = length + padding;
        check(ProtocolUtil::calcUnpaddedLength(record, paddedLength) == length,
              "unpadded length round-trips");
    }
}

}  // namespace

int main()
{
    testPaddingArithmetic();
    testPaddingBytes();
    testRoundTrip();

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
