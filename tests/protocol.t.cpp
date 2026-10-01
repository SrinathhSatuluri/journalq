#include <journalq/protocol.h>

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

using namespace journalq;

// Every header stamps its own size in words, so a reader can skip fields it
// does not know, and every default is the value a fresh file needs.
void testDefaults()
{
    {
        FileHeader fh;
        check(fh.magic1() == FileHeader::k_MAGIC1, "FileHeader magic1");
        check(fh.magic2() == FileHeader::k_MAGIC2, "FileHeader magic2");
        check(fh.protocolVersion() == Protocol::k_VERSION,
              "FileHeader version");
        check(fh.headerWords() * Protocol::k_WORD_SIZE == sizeof(FileHeader),
              "FileHeader headerWords from sizeof");
        check(fh.fileType() == FileType::e_UNDEFINED, "FileHeader type");
    }
    {
        DataFileHeader dfh;
        check(dfh.headerWords() * Protocol::k_WORD_SIZE ==
                  sizeof(DataFileHeader),
              "DataFileHeader headerWords from sizeof");
        check(dfh.fileKey() == FileKey{}, "DataFileHeader key is zero");
    }
    {
        JournalFileHeader jfh;
        check(jfh.headerWords() * Protocol::k_WORD_SIZE ==
                  sizeof(JournalFileHeader),
              "JournalFileHeader headerWords from sizeof");
        check(jfh.recordWords() ==
                  Protocol::k_JOURNAL_RECORD_SIZE / Protocol::k_WORD_SIZE,
              "JournalFileHeader recordWords from record size");
        check(jfh.firstSyncPointOffsetWords() == 0,
              "JournalFileHeader no sync point");
    }
    {
        DataHeader dh;
        const auto words = static_cast<std::uint32_t>(sizeof(DataHeader) /
                                                      Protocol::k_WORD_SIZE);
        check(dh.headerWords() == words, "DataHeader headerWords from sizeof");
        check(dh.messageWords() == words,
              "DataHeader messageWords from sizeof");
        check(dh.optionsWords() == 0, "DataHeader no options");
        check(dh.flags() == 0, "DataHeader no flags");
    }
    {
        RecordHeader rh;
        check(rh.type() == RecordType::e_UNDEFINED, "RecordHeader type");
        check(rh.flags() == 0, "RecordHeader flags");
        check(rh.sequenceNumber() == 0, "RecordHeader sequence number");
        check(rh.epoch() == 0, "RecordHeader epoch");
        check(rh.timestamp() == 0, "RecordHeader timestamp");
    }
    {
        MessageRecord mr;
        check(mr.magic() == 0, "MessageRecord unstamped magic is zero");
        check(mr.crc32c() == 0, "MessageRecord crc");
        check(mr.messageOffsetDwords() == 0, "MessageRecord offset");
        check(mr.header().type() == RecordType::e_UNDEFINED,
              "MessageRecord header type");
    }
}

// Fields that share a word must not disturb each other, at every extreme.
void testPacking()
{
    {
        FileHeader fh;
        fh.setProtocolVersion(3).setHeaderWords(63);
        check(fh.protocolVersion() == 3, "FileHeader version max");
        check(fh.headerWords() == 63, "FileHeader headerWords max");
        fh.setProtocolVersion(0);
        check(fh.headerWords() == 63, "FileHeader version leaves words");
        fh.setHeaderWords(0);
        check(fh.protocolVersion() == 0, "FileHeader words leaves version");
        fh.setHeaderWords(9).setProtocolVersion(2);
        check(fh.headerWords() == 9 && fh.protocolVersion() == 2,
              "FileHeader mixed values");
        fh.setFileType(FileType::e_JOURNAL);
        check(fh.fileType() == FileType::e_JOURNAL, "FileHeader file type");
    }
    {
        DataHeader dh;
        dh.setHeaderWords(DataHeader::k_MAX_HEADER_WORDS)
            .setMessageWords(DataHeader::k_MAX_MESSAGE_WORDS);
        check(dh.headerWords() == DataHeader::k_MAX_HEADER_WORDS,
              "DataHeader headerWords max");
        check(dh.messageWords() == DataHeader::k_MAX_MESSAGE_WORDS,
              "DataHeader messageWords max");

        dh.setMessageWords(0);
        check(dh.headerWords() == DataHeader::k_MAX_HEADER_WORDS,
              "DataHeader messageWords leaves headerWords");
        dh.setHeaderWords(0);
        check(dh.messageWords() == 0,
              "DataHeader headerWords leaves messageWords");

        dh.setHeaderWords(5).setMessageWords(123);
        check(dh.headerWords() == 5 && dh.messageWords() == 123,
              "DataHeader mixed values");

        dh.setOptionsWords(DataHeader::k_MAX_OPTIONS_WORDS)
            .setFlags(DataHeader::k_MAX_FLAGS);
        check(dh.optionsWords() == DataHeader::k_MAX_OPTIONS_WORDS,
              "DataHeader optionsWords max");
        check(dh.flags() == DataHeader::k_MAX_FLAGS, "DataHeader flags max");
        dh.setFlags(0);
        check(dh.optionsWords() == DataHeader::k_MAX_OPTIONS_WORDS,
              "DataHeader flags leaves optionsWords");
        dh.setOptionsWords(0);
        check(dh.flags() == 0, "DataHeader optionsWords leaves flags");
    }
    {
        RecordHeader rh;
        rh.setType(RecordType::e_JOURNAL_OP)
            .setFlags(RecordHeader::k_FLAGS_MASK);
        check(rh.type() == RecordType::e_JOURNAL_OP, "RecordHeader type");
        check(rh.flags() == RecordHeader::k_FLAGS_MASK,
              "RecordHeader flags max");
        rh.setFlags(0);
        check(rh.type() == RecordType::e_JOURNAL_OP,
              "RecordHeader flags leaves type");
        rh.setType(RecordType::e_UNDEFINED);
        check(rh.flags() == 0, "RecordHeader type leaves flags");
        rh.setType(RecordType::e_MESSAGE).setFlags(0xABC);
        check(rh.type() == RecordType::e_MESSAGE && rh.flags() == 0xABC,
              "RecordHeader mixed values");
    }
}

// Values wider than one field are split across fields and reassembled.
void testWideFields()
{
    {
        RecordHeader rh;
        rh.setSequenceNumber(RecordHeader::k_MAX_SEQUENCE_NUMBER);
        check(rh.sequenceNumber() == RecordHeader::k_MAX_SEQUENCE_NUMBER,
              "sequence number all 48 bits");
        rh.setSequenceNumber(std::uint64_t{1} << 40);
        check(rh.sequenceNumber() == (std::uint64_t{1} << 40),
              "sequence number upper bits");
        rh.setSequenceNumber(0x0000123456789ABCull);
        check(rh.sequenceNumber() == 0x0000123456789ABCull,
              "sequence number mixed bits");

        rh.setTimestamp(0xFFFFFFFFFFFFFFFFull);
        check(rh.timestamp() == 0xFFFFFFFFFFFFFFFFull, "timestamp all bits");
        rh.setTimestamp(0x0123456789ABCDEFull);
        check(rh.timestamp() == 0x0123456789ABCDEFull, "timestamp mixed bits");

        rh.setEpoch(0xFFFFFFFFu);
        check(rh.epoch() == 0xFFFFFFFFu, "epoch all bits");
    }
    {
        JournalFileHeader jfh;
        jfh.setFirstSyncPointOffsetWords(0x0000000100000000ull);
        check(jfh.firstSyncPointOffsetWords() == 0x0000000100000000ull,
              "sync point offset upper bits");
    }
}

// A record round-trips every field through its manipulators.
void testMessageRecord()
{
    const FileKey key = {0x01, 0x02, 0x03, 0x04, 0x05};
    MessageId     id;
    for (std::size_t i = 0; i < id.size(); ++i) {
        id[i] = static_cast<unsigned char>(0xA0 + i);
    }

    MessageRecord mr;
    mr.header()
        .setType(RecordType::e_MESSAGE)
        .setFlags(7)
        .setSequenceNumber(42)
        .setEpoch(3)
        .setTimestamp(1700000000000000000ull);
    mr.setFileKey(key)
        .setMessageOffsetDwords(0x00ABCDEFu)
        .setMessageId(id)
        .setCrc32c(0xE3069283u)
        .setMagic(MessageRecord::k_MAGIC);

    check(mr.header().type() == RecordType::e_MESSAGE, "record type");
    check(mr.header().flags() == 7, "record flags");
    check(mr.header().sequenceNumber() == 42, "record sequence number");
    check(mr.header().epoch() == 3, "record epoch");
    check(mr.header().timestamp() == 1700000000000000000ull,
          "record timestamp");
    check(mr.fileKey() == key, "record file key");
    check(mr.messageOffsetDwords() == 0x00ABCDEFu, "record offset");
    check(mr.messageId() == id, "record message id");
    check(mr.crc32c() == 0xE3069283u, "record crc");
    check(mr.magic() == MessageRecord::k_MAGIC, "record magic");
}

// The bytes on disk are fixed by the layout, not by the host: a header
// written here decodes on any machine, and the magic is the last word of a
// message record.
void testOnDiskBytes()
{
    {
        DataHeader dh;
        dh.setHeaderWords(3).setMessageWords(0x00010203u);
        dh.setOptionsWords(0x00ABCDEFu).setFlags(0x42);

        unsigned char raw[sizeof(DataHeader)];
        std::memcpy(raw, &dh, sizeof raw);

        // HW=3 occupies the top three bits: 011 followed by 0 0001 0203.
        check(raw[0] == 0x60 && raw[1] == 0x01 && raw[2] == 0x02 &&
                  raw[3] == 0x03,
              "DataHeader first word bytes");
        check(raw[4] == 0xAB && raw[5] == 0xCD && raw[6] == 0xEF &&
                  raw[7] == 0x42,
              "DataHeader second word bytes");
    }
    {
        FileHeader    fh;
        unsigned char raw[sizeof(FileHeader)];
        std::memcpy(raw, &fh, sizeof raw);
        check(raw[0] == '!' && raw[1] == 'j' && raw[2] == 'n' && raw[3] == 'q',
              "FileHeader magic1 spells !jnq");
        check(raw[4] == 'J' && raw[5] == 'N' && raw[6] == 'Q' && raw[7] == '!',
              "FileHeader magic2 spells JNQ!");
        // PV=1 in the top two bits, HW=4 in the low six: 0100 0100.
        check(raw[8] == 0x44, "FileHeader version and words byte");
    }
    {
        MessageRecord mr;
        mr.setMagic(MessageRecord::k_MAGIC);
        unsigned char raw[sizeof(MessageRecord)];
        std::memcpy(raw, &mr, sizeof raw);
        const std::size_t last = sizeof raw - 4;
        check(raw[last] == '*' && raw[last + 1] == 'r' &&
                  raw[last + 2] == 'e' && raw[last + 3] == 'c',
              "MessageRecord magic is the final word");
    }
    {
        // Decoding is the inverse of encoding: bytes read from a file yield
        // the fields that were written.
        const unsigned char raw[sizeof(DataHeader)] =
            {0x60, 0x01, 0x02, 0x03, 0xAB, 0xCD, 0xEF, 0x42, 0, 0, 0, 0};
        DataHeader dh;
        std::memcpy(&dh, raw, sizeof raw);
        check(dh.headerWords() == 3 && dh.messageWords() == 0x00010203u,
              "DataHeader decoded first word");
        check(dh.optionsWords() == 0x00ABCDEFu && dh.flags() == 0x42,
              "DataHeader decoded second word");
    }
}

void testToAscii()
{
    check(std::strcmp(toAscii(FileType::e_DATA), "DATA") == 0, "FileType DATA");
    check(std::strcmp(toAscii(FileType::e_JOURNAL), "JOURNAL") == 0,
          "FileType JOURNAL");
    check(std::strcmp(toAscii(static_cast<FileType>(200)), "(* UNKNOWN *)") ==
              0,
          "FileType unknown");
    check(std::strcmp(toAscii(RecordType::e_MESSAGE), "MESSAGE") == 0,
          "RecordType MESSAGE");
    check(std::strcmp(toAscii(static_cast<RecordType>(15)), "(* UNKNOWN *)") ==
              0,
          "RecordType unknown");
}

}  // namespace

int main()
{
    testDefaults();
    testPacking();
    testWideFields();
    testMessageRecord();
    testOnDiskBytes();
    testToAscii();

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
