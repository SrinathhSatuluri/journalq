#ifndef INCLUDED_JOURNALQ_PROTOCOL
#define INCLUDED_JOURNALQ_PROTOCOL

// Provide the on-disk layouts of a journalq queue.
//
// A queue is stored in two files. The journal holds fixed-size records, one
// per event, that index the data file, which holds variable-size message
// payloads. Every structure here is a plain byte layout of big-endian fields.
// Sizes are stored in 4-byte words, records in the data file are padded to
// 8-byte boundaries, and each header carries its own size in words so a
// reader can skip over fields it does not know about.
//
// The structures are trivially copyable and have no alignment requirement
// beyond one byte, so they are read and written directly at file offsets.
// Layout diagrams show 32-bit rows; a field's width in bits is given where it
// shares a word with its neighbors.

#include <journalq/bigendian.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace journalq {

// ===============
// struct Protocol
// ===============

/// Constants shared by every on-disk structure.
struct Protocol {
    /// Unit in which header and record sizes are expressed.
    static constexpr int k_WORD_SIZE = 4;

    /// Alignment of every record in the data file.
    static constexpr int k_DWORD_SIZE = 8;

    /// Version of this layout, bumped only for incompatible changes.
    static constexpr int k_VERSION = 1;

    /// Size in bytes of every journal record, whatever its type.
    static constexpr int k_JOURNAL_RECORD_SIZE = 60;

    /// Length in bytes of a file key: the leading bytes of a hash of a data
    /// file's name, stored in that file's header and in every journal record
    /// that points into it, so a journal cannot be paired with the wrong
    /// data file.
    static constexpr int k_KEY_LENGTH = 5;

    /// Length in bytes of a message identifier.
    static constexpr int k_MESSAGE_ID_LENGTH = 16;
};

using FileKey   = std::array<unsigned char, Protocol::k_KEY_LENGTH>;
using MessageId = std::array<unsigned char, Protocol::k_MESSAGE_ID_LENGTH>;

namespace detail {

/// Return a mask of `numBits` one bits starting at bit `startIdx`.
constexpr std::uint32_t bitMask(int numBits, int startIdx) noexcept
{
    return ((std::uint32_t{1} << numBits) - 1u) << startIdx;
}

}  // namespace detail

// ===============
// enum FileType
// ===============

/// Which of a queue's files a `FileHeader` begins.
enum class FileType : unsigned char {
    e_UNDEFINED = 0,
    e_DATA      = 1,
    e_JOURNAL   = 2
};

/// Return the name of `value`, or a distinct string for an unknown value.
const char* toAscii(FileType value) noexcept;

// =================
// struct FileHeader
// =================

/// The header at offset zero of every file. It identifies the file as a
/// journalq file, states which layout version wrote it, and says which of
/// the queue's files it is.
struct FileHeader {
    // FileHeader layout [16 bytes]:
    //..
    //   +---------------+---------------+---------------+---------------+
    //   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
    //   +---------------+---------------+---------------+---------------+
    //   |                            Magic1                             |
    //   +---------------+---------------+---------------+---------------+
    //   |                            Magic2                             |
    //   +---------------+---------------+---------------+---------------+
    //   |PV |    HW     |   FileType    |           Reserved            |
    //   +---------------+---------------+---------------+---------------+
    //   |                           Reserved                            |
    //   +---------------+---------------+---------------+---------------+
    //
    //  Magic1, Magic2.........: Identify a journalq file
    //  Protocol Version (PV)..: Layout version, 2 bits
    //  Header Words (HW)......: Size of this header in words, 6 bits
    //  FileType...............: Which file this is, see `FileType`
    //..

    // NOTE: The size of this struct must be a multiple of 8 so the data file
    //       records that follow the file headers stay 8-byte aligned.

  private:
    // PRIVATE CONSTANTS
    static constexpr int k_PROTOCOL_VERSION_NUM_BITS = 2;
    static constexpr int k_HEADER_WORDS_NUM_BITS     = 6;

    static constexpr int k_PROTOCOL_VERSION_START_IDX = 6;
    static constexpr int k_HEADER_WORDS_START_IDX     = 0;

    static constexpr unsigned char k_PROTOCOL_VERSION_MASK =
        static_cast<unsigned char>(
            detail::bitMask(k_PROTOCOL_VERSION_NUM_BITS,
                            k_PROTOCOL_VERSION_START_IDX));
    static constexpr unsigned char k_HEADER_WORDS_MASK =
        static_cast<unsigned char>(
            detail::bitMask(k_HEADER_WORDS_NUM_BITS, k_HEADER_WORDS_START_IDX));

  public:
    // CONSTANTS

    /// Bytes a reader needs before it can check every field above.
    static constexpr int k_MIN_HEADER_SIZE = 10;

    static constexpr std::uint32_t k_MAGIC1 = 0x216A6E71;  // !jnq
    static constexpr std::uint32_t k_MAGIC2 = 0x4A4E5121;  // JNQ!

  private:
    // DATA
    BigEndianUint32 d_magic1;
    BigEndianUint32 d_magic2;
    unsigned char   d_protoVerAndHeaderWords = 0;
    unsigned char   d_fileType               = 0;
    unsigned char   d_reserved[6]            = {};

  public:
    // CREATORS

    /// Create a header with the magic words set, the protocol version set to
    /// the current one, `headerWords` derived from `sizeof`, and the file
    /// type undefined.
    FileHeader() noexcept;

    // MANIPULATORS
    FileHeader& setMagic1(std::uint32_t value) noexcept;
    FileHeader& setMagic2(std::uint32_t value) noexcept;
    FileHeader& setProtocolVersion(unsigned char value) noexcept;
    FileHeader& setHeaderWords(unsigned char value) noexcept;
    FileHeader& setFileType(FileType value) noexcept;

    // ACCESSORS
    std::uint32_t magic1() const noexcept;
    std::uint32_t magic2() const noexcept;
    unsigned char protocolVersion() const noexcept;
    unsigned char headerWords() const noexcept;
    FileType      fileType() const noexcept;
};

// =====================
// struct DataFileHeader
// =====================

/// The header that follows the `FileHeader` in a data file.
struct DataFileHeader {
    // DataFileHeader layout [8 bytes]:
    //..
    //   +---------------+---------------+---------------+---------------+
    //   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
    //   +---------------+---------------+---------------+---------------+
    //   |      HW       |   Reserved    |            FileKey            |
    //   +---------------+---------------+---------------+---------------+
    //   |                    FileKey                    |   Reserved    |
    //   +---------------+---------------+---------------+---------------+
    //
    //  Header Words (HW)..: Size of this header in words
    //  FileKey............: Key of this file, see `Protocol::k_KEY_LENGTH`
    //..

    // NOTE: The size of this struct must be a multiple of 8, see FileHeader.

  public:
    // CONSTANTS
    static constexpr int k_MIN_HEADER_SIZE = 1;

  private:
    // DATA
    unsigned char d_headerWords = 0;
    unsigned char d_reserved1   = 0;
    FileKey       d_fileKey     = {};
    unsigned char d_reserved2   = 0;

  public:
    // CREATORS

    /// Create a header with `headerWords` derived from `sizeof` and every
    /// other field zero.
    DataFileHeader() noexcept;

    // MANIPULATORS
    DataFileHeader& setHeaderWords(unsigned char value) noexcept;
    DataFileHeader& setFileKey(const FileKey& value) noexcept;

    // ACCESSORS
    unsigned char  headerWords() const noexcept;
    const FileKey& fileKey() const noexcept;
};

// ========================
// struct JournalFileHeader
// ========================

/// The header that follows the `FileHeader` in a journal file.
struct JournalFileHeader {
    // JournalFileHeader layout [12 bytes]:
    //..
    //   +---------------+---------------+---------------+---------------+
    //   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
    //   +---------------+---------------+---------------+---------------+
    //   |      HW       |  RecordWords  |           Reserved            |
    //   +---------------+---------------+---------------+---------------+
    //   |              First SyncPoint Offset Upper Bits                |
    //   +---------------+---------------+---------------+---------------+
    //   |              First SyncPoint Offset Lower Bits                |
    //   +---------------+---------------+---------------+---------------+
    //
    //  Header Words (HW)......: Size of this header in words
    //  RecordWords............: Size in words of every record in this file
    //  First SyncPoint Offset.: Offset in words of the first sync point
    //                           record, or 0 if there is none yet
    //..

  public:
    // CONSTANTS
    static constexpr int k_MIN_HEADER_SIZE = 1;

  private:
    // DATA
    unsigned char   d_headerWords = 0;
    unsigned char   d_recordWords = 0;
    unsigned char   d_reserved[2] = {};
    BigEndianUint32 d_firstSyncPointOffsetUpperBits;
    BigEndianUint32 d_firstSyncPointOffsetLowerBits;

  public:
    // CREATORS

    /// Create a header with `headerWords` derived from `sizeof`,
    /// `recordWords` derived from `Protocol::k_JOURNAL_RECORD_SIZE`, and no
    /// sync point.
    JournalFileHeader() noexcept;

    // MANIPULATORS
    JournalFileHeader& setHeaderWords(unsigned char value) noexcept;
    JournalFileHeader& setRecordWords(unsigned char value) noexcept;
    JournalFileHeader&
    setFirstSyncPointOffsetWords(std::uint64_t value) noexcept;

    // ACCESSORS
    unsigned char headerWords() const noexcept;
    unsigned char recordWords() const noexcept;
    std::uint64_t firstSyncPointOffsetWords() const noexcept;
};

// =================
// struct DataHeader
// =================

/// The header of every record in the data file. The record is the header,
/// an optional options area, the payload, and 1 to 8 bytes of padding that
/// bring the total to a multiple of 8; each padding byte holds the padding
/// length, so the last byte of a record says how much of it is padding.
struct DataHeader {
    // DataHeader layout [12 bytes]:
    //..
    //   +---------------+---------------+---------------+---------------+
    //   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
    //   +---------------+---------------+---------------+---------------+
    //   | HW  |                     MessageWords                        |
    //   +---------------+---------------+---------------+---------------+
    //   |                  OptionsWords                 |     Flags     |
    //   +---------------+---------------+---------------+---------------+
    //   |                           Reserved                            |
    //   +---------------+---------------+---------------+---------------+
    //
    //  Header Words (HW)..: Size of this header in words, 3 bits
    //  MessageWords.......: Size in words of the whole record: this header,
    //                       the options area, the payload and the padding,
    //                       29 bits
    //  OptionsWords.......: Size in words of the options area, 24 bits
    //  Flags..............: Reserved for record flags, 8 bits
    //..

  public:
    // CONSTANTS

    /// Bytes a reader needs before it can check the sizes above.
    static constexpr int k_MIN_HEADER_SIZE = 4;

  private:
    // PRIVATE CONSTANTS
    static constexpr int k_HEADER_WORDS_NUM_BITS  = 3;
    static constexpr int k_MSG_WORDS_NUM_BITS     = 29;
    static constexpr int k_OPTIONS_WORDS_NUM_BITS = 24;
    static constexpr int k_FLAGS_NUM_BITS         = 8;

    static constexpr int k_HEADER_WORDS_START_IDX  = 29;
    static constexpr int k_MSG_WORDS_START_IDX     = 0;
    static constexpr int k_OPTIONS_WORDS_START_IDX = 8;
    static constexpr int k_FLAGS_START_IDX         = 0;

    static constexpr std::uint32_t k_HEADER_WORDS_MASK =
        detail::bitMask(k_HEADER_WORDS_NUM_BITS, k_HEADER_WORDS_START_IDX);
    static constexpr std::uint32_t k_MSG_WORDS_MASK =
        detail::bitMask(k_MSG_WORDS_NUM_BITS, k_MSG_WORDS_START_IDX);
    static constexpr std::uint32_t k_OPTIONS_WORDS_MASK =
        detail::bitMask(k_OPTIONS_WORDS_NUM_BITS, k_OPTIONS_WORDS_START_IDX);
    static constexpr std::uint32_t k_FLAGS_MASK =
        detail::bitMask(k_FLAGS_NUM_BITS, k_FLAGS_START_IDX);

  public:
    // CONSTANTS
    static constexpr std::uint32_t k_MAX_HEADER_WORDS =
        (1u << k_HEADER_WORDS_NUM_BITS) - 1;
    static constexpr std::uint32_t k_MAX_MESSAGE_WORDS =
        (1u << k_MSG_WORDS_NUM_BITS) - 1;
    static constexpr std::uint32_t k_MAX_OPTIONS_WORDS =
        (1u << k_OPTIONS_WORDS_NUM_BITS) - 1;
    static constexpr std::uint32_t k_MAX_FLAGS = (1u << k_FLAGS_NUM_BITS) - 1;

  private:
    // DATA
    BigEndianUint32 d_headerWordsAndMessageWords;
    BigEndianUint32 d_optionsWordsAndFlags;
    unsigned char   d_reserved[4] = {};

  public:
    // CREATORS

    /// Create a header with `headerWords` and `messageWords` both derived
    /// from `sizeof`, describing a record that is nothing but this header,
    /// and every other field zero.
    DataHeader() noexcept;

    // MANIPULATORS
    DataHeader& setHeaderWords(std::uint32_t value) noexcept;
    DataHeader& setMessageWords(std::uint32_t value) noexcept;
    DataHeader& setOptionsWords(std::uint32_t value) noexcept;
    DataHeader& setFlags(std::uint32_t value) noexcept;

    // ACCESSORS
    std::uint32_t headerWords() const noexcept;
    std::uint32_t messageWords() const noexcept;
    std::uint32_t optionsWords() const noexcept;
    std::uint32_t flags() const noexcept;
};

// ================
// enum RecordType
// ================

/// The kind of event a journal record describes.
enum class RecordType : unsigned char {
    e_UNDEFINED  = 0,
    e_MESSAGE    = 1,  // a message was appended to the data file
    e_CONFIRM    = 2,  // a consumer confirmed a message
    e_DELETION   = 3,  // a message was removed
    e_QUEUE_OP   = 4,  // the queue was created, purged or deleted
    e_JOURNAL_OP = 5   // a sync point or other journal-level event
};

/// Return the name of `value`, or a distinct string for an unknown value.
const char* toAscii(RecordType value) noexcept;

// ===================
// struct RecordHeader
// ===================

/// The header of every journal record.
struct RecordHeader {
    // RecordHeader layout [20 bytes]:
    //..
    //   +---------------+---------------+---------------+---------------+
    //   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
    //   +---------------+---------------+---------------+---------------+
    //   | Type  |         Flags         |  Sequence Number Upper Bits   |
    //   +---------------+---------------+---------------+---------------+
    //   |                  Sequence Number Lower Bits                   |
    //   +---------------+---------------+---------------+---------------+
    //   |                            Epoch                              |
    //   +---------------+---------------+---------------+---------------+
    //   |                     Timestamp Upper Bits                      |
    //   +---------------+---------------+---------------+---------------+
    //   |                     Timestamp Lower Bits                      |
    //   +---------------+---------------+---------------+---------------+
    //
    //  Type.............: Kind of record, 4 bits, see `RecordType`
    //  Flags............: Meaning depends on the record type, 12 bits
    //  Sequence Number..: Position of this record within its epoch, 48 bits
    //  Epoch............: Incremented each time the queue is opened for
    //                     writing, so (epoch, sequence number) identifies a
    //                     record uniquely across restarts
    //  Timestamp........: Time the record was written, nanoseconds since
    //                     the Unix epoch
    //..

  private:
    // PRIVATE CONSTANTS
    static constexpr int k_TYPE_NUM_BITS   = 4;
    static constexpr int k_TYPE_START_IDX  = 12;
    static constexpr int k_FLAGS_START_IDX = 0;

    static constexpr std::uint16_t k_TYPE_MASK = static_cast<std::uint16_t>(
        detail::bitMask(k_TYPE_NUM_BITS, k_TYPE_START_IDX));

  public:
    // CONSTANTS
    static constexpr int           k_FLAGS_NUM_BITS = 12;
    static constexpr std::uint16_t k_FLAGS_MASK = static_cast<std::uint16_t>(
        detail::bitMask(k_FLAGS_NUM_BITS, k_FLAGS_START_IDX));

    static constexpr std::uint64_t k_MAX_SEQUENCE_NUMBER =
        (std::uint64_t{1} << 48) - 1;

  private:
    // DATA
    BigEndianUint16 d_typeAndFlags;
    BigEndianUint16 d_seqNumUpperBits;
    BigEndianUint32 d_seqNumLowerBits;
    BigEndianUint32 d_epoch;
    BigEndianUint32 d_timestampUpperBits;
    BigEndianUint32 d_timestampLowerBits;

  public:
    // CREATORS

    /// Create a header with every field zero and the type undefined.
    RecordHeader() noexcept = default;

    // MANIPULATORS
    RecordHeader& setType(RecordType value) noexcept;
    RecordHeader& setFlags(std::uint16_t value) noexcept;
    RecordHeader& setSequenceNumber(std::uint64_t value) noexcept;
    RecordHeader& setEpoch(std::uint32_t value) noexcept;
    RecordHeader& setTimestamp(std::uint64_t value) noexcept;

    // ACCESSORS
    RecordType    type() const noexcept;
    std::uint16_t flags() const noexcept;
    std::uint64_t sequenceNumber() const noexcept;
    std::uint32_t epoch() const noexcept;
    std::uint64_t timestamp() const noexcept;
};

// ====================
// struct MessageRecord
// ====================

/// A journal record saying that a message was appended to the data file, and
/// where. It is the record recovery uses to rebuild the queue's index.
struct MessageRecord {
    // MessageRecord layout [60 bytes]:
    //..
    //   +---------------+---------------+---------------+---------------+
    //   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
    //   +---------------+---------------+---------------+---------------+
    //   |                            Header                             |
    //   +                                                               +
    //   |                          (20 bytes)                           |
    //   +                                                               +
    //   |                                                               |
    //   +                                                               +
    //   |                                                               |
    //   +                                                               +
    //   |                                                               |
    //   +---------------+---------------+---------------+---------------+
    //   |           Reserved            |            FileKey            |
    //   +---------------+---------------+---------------+---------------+
    //   |                    FileKey                    |   Reserved    |
    //   +---------------+---------------+---------------+---------------+
    //   |                      MessageOffsetDwords                      |
    //   +---------------+---------------+---------------+---------------+
    //   |                           MessageId                           |
    //   +                                                               +
    //   |                          (16 bytes)                           |
    //   +                                                               +
    //   |                                                               |
    //   +                                                               +
    //   |                                                               |
    //   +---------------+---------------+---------------+---------------+
    //   |                            CRC-32C                            |
    //   +---------------+---------------+---------------+---------------+
    //   |                           Reserved                            |
    //   +---------------+---------------+---------------+---------------+
    //   |                            Magic                              |
    //   +---------------+---------------+---------------+---------------+
    //
    //  Header...............: Record header, see `RecordHeader`
    //  FileKey..............: Key of the data file the message lives in
    //  MessageOffsetDwords..: Offset of the message's `DataHeader` in that
    //                         file, in units of 8 bytes
    //  MessageId............: Identifier of the message
    //  CRC-32C..............: Checksum of the message payload
    //  Magic................: Marks a fully written record. It is the last
    //                         field on purpose: a record whose tail never
    //                         reached the disk fails this check first.
    //..

  public:
    // CONSTANTS
    static constexpr std::uint32_t k_MAGIC = 0x2A726563;  // *rec

  private:
    // DATA
    RecordHeader    d_header;
    unsigned char   d_reserved1[2] = {};
    FileKey         d_fileKey      = {};
    unsigned char   d_reserved2    = 0;
    BigEndianUint32 d_messageOffsetDwords;
    MessageId       d_messageId = {};
    BigEndianUint32 d_crc32c;
    unsigned char   d_reserved3[4] = {};
    BigEndianUint32 d_magic;

  public:
    // CREATORS

    /// Create a record with every field zero, including the magic: a record
    /// only becomes valid once its writer stamps it.
    MessageRecord() noexcept = default;

    // MANIPULATORS
    RecordHeader&  header() noexcept;
    MessageRecord& setFileKey(const FileKey& value) noexcept;
    MessageRecord& setMessageOffsetDwords(std::uint32_t value) noexcept;
    MessageRecord& setMessageId(const MessageId& value) noexcept;
    MessageRecord& setCrc32c(std::uint32_t value) noexcept;
    MessageRecord& setMagic(std::uint32_t value) noexcept;

    // ACCESSORS
    const RecordHeader& header() const noexcept;
    const FileKey&      fileKey() const noexcept;
    std::uint32_t       messageOffsetDwords() const noexcept;
    const MessageId&    messageId() const noexcept;
    std::uint32_t       crc32c() const noexcept;
    std::uint32_t       magic() const noexcept;
};

// ============================================================================
//                              LAYOUT INVARIANTS
// ============================================================================

static_assert(sizeof(FileHeader) == 16);
static_assert(sizeof(DataFileHeader) == 8);
static_assert(sizeof(JournalFileHeader) == 12);
static_assert(sizeof(DataHeader) == 12);
static_assert(sizeof(RecordHeader) == 20);
static_assert(sizeof(MessageRecord) == Protocol::k_JOURNAL_RECORD_SIZE);

// Records in the data file are 8-byte aligned, so the headers before them
// must add up to a multiple of 8.
static_assert((sizeof(FileHeader) + sizeof(DataFileHeader)) %
                  Protocol::k_DWORD_SIZE ==
              0);

// Every header size must be expressible in words.
static_assert(sizeof(FileHeader) % Protocol::k_WORD_SIZE == 0);
static_assert(sizeof(DataFileHeader) % Protocol::k_WORD_SIZE == 0);
static_assert(sizeof(JournalFileHeader) % Protocol::k_WORD_SIZE == 0);
static_assert(sizeof(DataHeader) % Protocol::k_WORD_SIZE == 0);
static_assert(Protocol::k_JOURNAL_RECORD_SIZE % Protocol::k_WORD_SIZE == 0);

// The structures are copied to and from file offsets as raw bytes.
static_assert(std::is_trivially_copyable_v<FileHeader>);
static_assert(std::is_trivially_copyable_v<DataFileHeader>);
static_assert(std::is_trivially_copyable_v<JournalFileHeader>);
static_assert(std::is_trivially_copyable_v<DataHeader>);
static_assert(std::is_trivially_copyable_v<RecordHeader>);
static_assert(std::is_trivially_copyable_v<MessageRecord>);
static_assert(alignof(MessageRecord) == 1);

// ============================================================================
//                             INLINE DEFINITIONS
// ============================================================================

// -----------------
// struct FileHeader
// -----------------

inline FileHeader::FileHeader() noexcept
: d_magic1(k_MAGIC1)
, d_magic2(k_MAGIC2)
{
    setProtocolVersion(static_cast<unsigned char>(Protocol::k_VERSION));
    setHeaderWords(
        static_cast<unsigned char>(sizeof(FileHeader) / Protocol::k_WORD_SIZE));
}

inline FileHeader& FileHeader::setMagic1(std::uint32_t value) noexcept
{
    d_magic1 = value;
    return *this;
}

inline FileHeader& FileHeader::setMagic2(std::uint32_t value) noexcept
{
    d_magic2 = value;
    return *this;
}

inline FileHeader& FileHeader::setProtocolVersion(unsigned char value) noexcept
{
    assert(value <= (1u << k_PROTOCOL_VERSION_NUM_BITS) - 1);

    d_protoVerAndHeaderWords = static_cast<unsigned char>(
        (d_protoVerAndHeaderWords & k_HEADER_WORDS_MASK) |
        (value << k_PROTOCOL_VERSION_START_IDX));
    return *this;
}

inline FileHeader& FileHeader::setHeaderWords(unsigned char value) noexcept
{
    assert(value <= (1u << k_HEADER_WORDS_NUM_BITS) - 1);

    d_protoVerAndHeaderWords = static_cast<unsigned char>(
        (d_protoVerAndHeaderWords & k_PROTOCOL_VERSION_MASK) |
        (value << k_HEADER_WORDS_START_IDX));
    return *this;
}

inline FileHeader& FileHeader::setFileType(FileType value) noexcept
{
    d_fileType = static_cast<unsigned char>(value);
    return *this;
}

inline std::uint32_t FileHeader::magic1() const noexcept
{
    return d_magic1;
}

inline std::uint32_t FileHeader::magic2() const noexcept
{
    return d_magic2;
}

inline unsigned char FileHeader::protocolVersion() const noexcept
{
    return static_cast<unsigned char>(
        (d_protoVerAndHeaderWords & k_PROTOCOL_VERSION_MASK) >>
        k_PROTOCOL_VERSION_START_IDX);
}

inline unsigned char FileHeader::headerWords() const noexcept
{
    return static_cast<unsigned char>(
        (d_protoVerAndHeaderWords & k_HEADER_WORDS_MASK) >>
        k_HEADER_WORDS_START_IDX);
}

inline FileType FileHeader::fileType() const noexcept
{
    return static_cast<FileType>(d_fileType);
}

// ---------------------
// struct DataFileHeader
// ---------------------

inline DataFileHeader::DataFileHeader() noexcept
: d_headerWords(static_cast<unsigned char>(sizeof(DataFileHeader) /
                                           Protocol::k_WORD_SIZE))
{
}

inline DataFileHeader&
DataFileHeader::setHeaderWords(unsigned char value) noexcept
{
    d_headerWords = value;
    return *this;
}

inline DataFileHeader& DataFileHeader::setFileKey(const FileKey& value) noexcept
{
    d_fileKey = value;
    return *this;
}

inline unsigned char DataFileHeader::headerWords() const noexcept
{
    return d_headerWords;
}

inline const FileKey& DataFileHeader::fileKey() const noexcept
{
    return d_fileKey;
}

// ------------------------
// struct JournalFileHeader
// ------------------------

inline JournalFileHeader::JournalFileHeader() noexcept
: d_headerWords(static_cast<unsigned char>(sizeof(JournalFileHeader) /
                                           Protocol::k_WORD_SIZE))
, d_recordWords(static_cast<unsigned char>(Protocol::k_JOURNAL_RECORD_SIZE /
                                           Protocol::k_WORD_SIZE))
{
}

inline JournalFileHeader&
JournalFileHeader::setHeaderWords(unsigned char value) noexcept
{
    d_headerWords = value;
    return *this;
}

inline JournalFileHeader&
JournalFileHeader::setRecordWords(unsigned char value) noexcept
{
    d_recordWords = value;
    return *this;
}

inline JournalFileHeader&
JournalFileHeader::setFirstSyncPointOffsetWords(std::uint64_t value) noexcept
{
    d_firstSyncPointOffsetUpperBits = static_cast<std::uint32_t>(value >> 32);
    d_firstSyncPointOffsetLowerBits = static_cast<std::uint32_t>(value);
    return *this;
}

inline unsigned char JournalFileHeader::headerWords() const noexcept
{
    return d_headerWords;
}

inline unsigned char JournalFileHeader::recordWords() const noexcept
{
    return d_recordWords;
}

inline std::uint64_t
JournalFileHeader::firstSyncPointOffsetWords() const noexcept
{
    return (static_cast<std::uint64_t>(d_firstSyncPointOffsetUpperBits) << 32) |
           static_cast<std::uint64_t>(d_firstSyncPointOffsetLowerBits);
}

// -----------------
// struct DataHeader
// -----------------

inline DataHeader::DataHeader() noexcept
{
    const auto words =
        static_cast<std::uint32_t>(sizeof(DataHeader) / Protocol::k_WORD_SIZE);
    setHeaderWords(words);
    setMessageWords(words);
}

inline DataHeader& DataHeader::setHeaderWords(std::uint32_t value) noexcept
{
    assert(value <= k_MAX_HEADER_WORDS);

    d_headerWordsAndMessageWords =
        (d_headerWordsAndMessageWords & k_MSG_WORDS_MASK) |
        (value << k_HEADER_WORDS_START_IDX);
    return *this;
}

inline DataHeader& DataHeader::setMessageWords(std::uint32_t value) noexcept
{
    assert(value <= k_MAX_MESSAGE_WORDS);

    d_headerWordsAndMessageWords =
        (d_headerWordsAndMessageWords & k_HEADER_WORDS_MASK) |
        (value << k_MSG_WORDS_START_IDX);
    return *this;
}

inline DataHeader& DataHeader::setOptionsWords(std::uint32_t value) noexcept
{
    assert(value <= k_MAX_OPTIONS_WORDS);

    d_optionsWordsAndFlags = (d_optionsWordsAndFlags & k_FLAGS_MASK) |
                             (value << k_OPTIONS_WORDS_START_IDX);
    return *this;
}

inline DataHeader& DataHeader::setFlags(std::uint32_t value) noexcept
{
    assert(value <= k_MAX_FLAGS);

    d_optionsWordsAndFlags = (d_optionsWordsAndFlags & k_OPTIONS_WORDS_MASK) |
                             (value << k_FLAGS_START_IDX);
    return *this;
}

inline std::uint32_t DataHeader::headerWords() const noexcept
{
    return (d_headerWordsAndMessageWords & k_HEADER_WORDS_MASK) >>
           k_HEADER_WORDS_START_IDX;
}

inline std::uint32_t DataHeader::messageWords() const noexcept
{
    return (d_headerWordsAndMessageWords & k_MSG_WORDS_MASK) >>
           k_MSG_WORDS_START_IDX;
}

inline std::uint32_t DataHeader::optionsWords() const noexcept
{
    return (d_optionsWordsAndFlags & k_OPTIONS_WORDS_MASK) >>
           k_OPTIONS_WORDS_START_IDX;
}

inline std::uint32_t DataHeader::flags() const noexcept
{
    return (d_optionsWordsAndFlags & k_FLAGS_MASK) >> k_FLAGS_START_IDX;
}

// -------------------
// struct RecordHeader
// -------------------

inline RecordHeader& RecordHeader::setType(RecordType value) noexcept
{
    d_typeAndFlags = static_cast<std::uint16_t>(
        (d_typeAndFlags & k_FLAGS_MASK) |
        (static_cast<std::uint16_t>(value) << k_TYPE_START_IDX));
    return *this;
}

inline RecordHeader& RecordHeader::setFlags(std::uint16_t value) noexcept
{
    assert(value <= k_FLAGS_MASK);

    d_typeAndFlags = static_cast<std::uint16_t>((d_typeAndFlags & k_TYPE_MASK) |
                                                (value << k_FLAGS_START_IDX));
    return *this;
}

inline RecordHeader&
RecordHeader::setSequenceNumber(std::uint64_t value) noexcept
{
    assert(value <= k_MAX_SEQUENCE_NUMBER);

    d_seqNumUpperBits = static_cast<std::uint16_t>(value >> 32);
    d_seqNumLowerBits = static_cast<std::uint32_t>(value);
    return *this;
}

inline RecordHeader& RecordHeader::setEpoch(std::uint32_t value) noexcept
{
    d_epoch = value;
    return *this;
}

inline RecordHeader& RecordHeader::setTimestamp(std::uint64_t value) noexcept
{
    d_timestampUpperBits = static_cast<std::uint32_t>(value >> 32);
    d_timestampLowerBits = static_cast<std::uint32_t>(value);
    return *this;
}

inline RecordType RecordHeader::type() const noexcept
{
    return static_cast<RecordType>((d_typeAndFlags & k_TYPE_MASK) >>
                                   k_TYPE_START_IDX);
}

inline std::uint16_t RecordHeader::flags() const noexcept
{
    return static_cast<std::uint16_t>((d_typeAndFlags & k_FLAGS_MASK) >>
                                      k_FLAGS_START_IDX);
}

inline std::uint64_t RecordHeader::sequenceNumber() const noexcept
{
    return (static_cast<std::uint64_t>(d_seqNumUpperBits) << 32) |
           static_cast<std::uint64_t>(d_seqNumLowerBits);
}

inline std::uint32_t RecordHeader::epoch() const noexcept
{
    return d_epoch;
}

inline std::uint64_t RecordHeader::timestamp() const noexcept
{
    return (static_cast<std::uint64_t>(d_timestampUpperBits) << 32) |
           static_cast<std::uint64_t>(d_timestampLowerBits);
}

// --------------------
// struct MessageRecord
// --------------------

inline RecordHeader& MessageRecord::header() noexcept
{
    return d_header;
}

inline MessageRecord& MessageRecord::setFileKey(const FileKey& value) noexcept
{
    d_fileKey = value;
    return *this;
}

inline MessageRecord&
MessageRecord::setMessageOffsetDwords(std::uint32_t value) noexcept
{
    d_messageOffsetDwords = value;
    return *this;
}

inline MessageRecord&
MessageRecord::setMessageId(const MessageId& value) noexcept
{
    d_messageId = value;
    return *this;
}

inline MessageRecord& MessageRecord::setCrc32c(std::uint32_t value) noexcept
{
    d_crc32c = value;
    return *this;
}

inline MessageRecord& MessageRecord::setMagic(std::uint32_t value) noexcept
{
    d_magic = value;
    return *this;
}

inline const RecordHeader& MessageRecord::header() const noexcept
{
    return d_header;
}

inline const FileKey& MessageRecord::fileKey() const noexcept
{
    return d_fileKey;
}

inline std::uint32_t MessageRecord::messageOffsetDwords() const noexcept
{
    return d_messageOffsetDwords;
}

inline const MessageId& MessageRecord::messageId() const noexcept
{
    return d_messageId;
}

inline std::uint32_t MessageRecord::crc32c() const noexcept
{
    return d_crc32c;
}

inline std::uint32_t MessageRecord::magic() const noexcept
{
    return d_magic;
}

}  // namespace journalq

#endif
