#include <journalq/protocol.h>

namespace journalq {

const char* toAscii(FileType value) noexcept
{
    switch (value) {
    case FileType::e_UNDEFINED:
        return "UNDEFINED";
    case FileType::e_DATA:
        return "DATA";
    case FileType::e_JOURNAL:
        return "JOURNAL";
    }
    return "(* UNKNOWN *)";
}

const char* toAscii(RecordType value) noexcept
{
    switch (value) {
    case RecordType::e_UNDEFINED:
        return "UNDEFINED";
    case RecordType::e_MESSAGE:
        return "MESSAGE";
    case RecordType::e_CONFIRM:
        return "CONFIRM";
    case RecordType::e_DELETION:
        return "DELETION";
    case RecordType::e_QUEUE_OP:
        return "QUEUE_OP";
    case RecordType::e_JOURNAL_OP:
        return "JOURNAL_OP";
    }
    return "(* UNKNOWN *)";
}

}  // namespace journalq
