#include "NfcProbe.h"
#include <algorithm>
#include <cstring>

namespace nfcprobe {
namespace {
constexpr char Mime[] = "application/vnd.yourapp.tagdata";
std::uint16_t be16(const std::uint8_t *p) { return (std::uint16_t(p[0]) << 8) | p[1]; }
std::uint32_t le32(const std::uint8_t *p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
void put32(std::uint8_t *p, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = std::uint8_t(value >> (i * 8));
}
bool testData(const std::uint8_t *p) {
    if (p[0] != 1 || le32(p + 1) != 20261015U || p[6] != 0 ||
        p[15] != 1 || p[16] != 0 || p[17] != 1 || p[18] != 1 || p[19] || p[20]) return false;
    for (unsigned i = 7; i < 15; ++i) if (p[i]) return false;
    return true;
}
}
const char *name(Result r) {
    switch (r) {
    case Result::Ok: return "OK"; case Result::NoTarget: return "NO_TARGET";
    case Result::Unsupported: return "UNSUPPORTED"; case Result::ReadOnly: return "READ_ONLY";
    case Result::NotPrepared: return "NOT_PREPARED"; case Result::Changed: return "CHANGED";
    case Result::Unknown: return "UNKNOWN"; case Result::Verified: return "VERIFIED";
    default: return "ERROR";
    }
}
const char *name(Kind k) {
    switch (k) {
    case Kind::Type4: return "TYPE4"; case Kind::Ntag213: return "NTAG213";
    case Kind::Ntag215: return "NTAG215"; case Kind::Ntag216: return "NTAG216";
    default: return "UNKNOWN";
    }
}
bool decodeFrame(const std::uint8_t *f, std::size_t n, std::uint8_t expected,
                 std::uint8_t *out, std::size_t capacity, std::size_t &received) {
    received = 0;
    if (!f || !out || n < 9 || n > MaxFrame || f[0] || f[1] || f[2] != 0xff ||
        f[3] < 2 || std::size_t(f[3]) + 7 != n || std::uint8_t(f[3] + f[4]) ||
        f[5] != 0xd5 || f[6] != expected || f[n - 1] || std::size_t(f[3] - 2) > capacity) return false;
    std::uint8_t sum = 0;
    for (std::size_t i = 5; i < n - 1; ++i) sum = std::uint8_t(sum + f[i]);
    if (sum) return false;
    received = f[3] - 2;
    std::memcpy(out, f + 7, received);
    return true;
}
std::size_t encodeCommand(const std::uint8_t *command, std::size_t size,
                          std::uint8_t *frame, std::size_t capacity) {
    if (!command || !frame || !size || size > 253 || size + 8 > capacity) return 0;
    frame[0] = frame[1] = 0; frame[2] = 0xff;
    frame[3] = std::uint8_t(size + 1); frame[4] = std::uint8_t(0 - frame[3]); frame[5] = 0xd4;
    std::uint8_t sum = 0xd4;
    for (std::size_t i = 0; i < size; ++i) { frame[6 + i] = command[i]; sum = std::uint8_t(sum + command[i]); }
    frame[6 + size] = std::uint8_t(0 - sum); frame[7 + size] = 0;
    return size + 8;
}
Result decodeTarget(const std::uint8_t *b, std::size_t n, Target &t) {
    t = Target();
    if (n == 1 && b && b[0] == 0) return Result::NoTarget;
    if (!b || n < 6 || b[0] != 1 || b[1] != 1 ||
        (b[5] != 4 && b[5] != 7 && b[5] != 10) || n < std::size_t(6 + b[5])) return Result::Error;
    const std::size_t end = 6 + b[5];
    if (n != end && (b[end] < 1 || std::size_t(b[end]) != n - end)) return Result::Error;
    if ((b[4] & 0x20) && n == end) return Result::Error;
    t.atqa[0] = b[2]; t.atqa[1] = b[3]; t.sak = b[4]; t.uidSize = b[5];
    std::memcpy(t.uid, b + 6, t.uidSize);
    return Result::Ok;
}
std::uint32_t crc32(const std::uint8_t *b, std::size_t n) {
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t i = 0; i < n; ++i) {
        crc ^= b[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return crc ^ 0xffffffffU;
}
bool gamePayload(const std::uint8_t *b, std::size_t n, std::size_t &offset, std::uint16_t &flags) {
    offset = 0; flags = 0;
    if (!b || !n || n > MaxMessage) return false;
    std::size_t pos = 0, count = 0; bool found = false, ended = false;
    while (pos < n) {
        if (ended || n - pos < 3) return false;
        const auto header = b[pos++]; const auto typeSize = b[pos++];
        const auto tnf = header & 7;
        if (bool(header & 0x80) != (count == 0) || (header & 0x20) || tnf >= 6) return false;
        std::size_t payloadSize = 0;
        if (header & 0x10) payloadSize = b[pos++];
        else {
            if (n - pos < 4) return false;
            const std::uint32_t large = (std::uint32_t(b[pos]) << 24) | (std::uint32_t(b[pos + 1]) << 16) |
                                        (std::uint32_t(b[pos + 2]) << 8) | b[pos + 3];
            if (large > MaxMessage) return false;
            payloadSize = large; pos += 4;
        }
        std::size_t idSize = 0;
        if (header & 0x08) { if (pos == n) return false; idSize = b[pos++]; }
        if (std::size_t(typeSize) + idSize > n - pos) return false;
        const std::size_t typeOffset = pos;
        pos += typeSize + idSize;
        if (payloadSize > n - pos || (tnf == 0 && (typeSize || idSize || payloadSize)) ||
            (tnf == 5 && typeSize)) return false;
        if (tnf == 2 && typeSize == sizeof(Mime) - 1 && std::memcmp(b + typeOffset, Mime, typeSize) == 0) {
            if (found || payloadSize != 25 || !testData(b + pos) ||
                crc32(b + pos, 21) != le32(b + pos + 21)) return false;
            found = true; offset = pos; flags = std::uint16_t(b[pos + 5]) | (std::uint16_t(b[pos + 6]) << 8);
        }
        pos += payloadSize; ended = (header & 0x40) != 0; ++count;
    }
    return found && ended;
}
bool isPrepared(const std::uint8_t *b, std::size_t n) {
    std::size_t offset = 0; std::uint16_t flags = 0;
    return gamePayload(b, n, offset, flags) && flags == 1;
}
bool prepareChange(Snapshot &s) {
    if (s.offset > s.rawSize || s.size > s.rawSize - s.offset) return false;
    std::size_t offset = 0; std::uint16_t flags = 0;
    if (!gamePayload(s.bytes + s.offset, s.size, offset, flags) || flags != 1) return false;
    auto *p = s.bytes + s.offset + offset; p[5] = 2; p[6] = 0; put32(p + 21, crc32(p, 21));
    return true;
}
bool parseType2(Snapshot &s) {
    std::size_t pos = 0; bool found = false, control = false;
    while (pos < s.rawSize) {
        const auto type = s.bytes[pos++];
        if (type == 0) continue;
        if (type == 0xfe) break;
        if (pos == s.rawSize) return false;
        const std::size_t lengthPosition = pos; const auto length = s.bytes[pos++];
        // The narrow test supports only short TLVs. Unknown reserved-memory maps are not written.
        if (length == 0xff || std::size_t(length) > s.rawSize - pos) return false;
        if (type == 3) {
            if (found || !length) return false;
            found = true; s.lengthOffset = std::uint16_t(lengthPosition);
            s.offset = std::uint16_t(pos); s.size = length;
        } else if (type == 1 && !control && !found && s.kind == Kind::Ntag213 && length == 3 &&
                   s.bytes[pos] == 0xa0 && s.bytes[pos + 1] == 0x0c && s.bytes[pos + 2] == 0x34) {
            // NTAG213 factory dynamic-lock TLV: lock bytes at 160, outside CC data [16,160).
            control = true;
        } else return false;
        pos += length;
    }
    return found;
}
bool Session::cmd(const std::uint8_t *b, std::size_t n, std::uint8_t *out,
                  std::size_t capacity, std::size_t &received) {
    received = 0;
    if (std::uint32_t(channel_.now() - started_) >= 20000U) { detail_ = "operation_deadline"; return false; }
    if (!channel_.command(b, n, out, capacity, received)) { detail_ = "pn532_transport_or_frame"; return false; }
    return true;
}
bool Session::exchange(const std::uint8_t *b, std::size_t n, std::uint8_t *out,
                       std::size_t capacity, std::size_t &received) {
    received = 0;
    if (n > Chunk + 5) { detail_ = "command_too_large"; return false; }
    std::uint8_t request[Chunk + 7] = {0x40, 1}, reply[Chunk + 8] = {};
    std::memcpy(request + 2, b, n); std::size_t size = 0;
    if (!cmd(request, n + 2, reply, sizeof(reply), size)) return false;
    // Reject PN532 errors, MI continuation and NAD. Never silently truncate.
    if (!size || reply[0] != 0 || size - 1 > capacity) { detail_ = "pn532_rf_status_or_length"; return false; }
    received = size - 1; std::memcpy(out, reply + 1, received); return true;
}
bool Session::apdu(const std::uint8_t *b, std::size_t n, std::uint8_t *out,
                   std::size_t capacity, std::size_t &received) {
    std::uint8_t reply[Chunk + 2] = {}; std::size_t size = 0; received = 0;
    if (!exchange(b, n, reply, sizeof(reply), size)) return false;
    if (size < 2 || reply[size - 2] != 0x90 || reply[size - 1] != 0 || size - 2 > capacity) {
        detail_ = "apdu_status_or_length"; return false;
    }
    received = size - 2; std::memcpy(out, reply, received); return true;
}
bool Session::selectFile(std::uint16_t file) {
    const std::uint8_t request[] = {0, 0xa4, 0, 0x0c, 2, std::uint8_t(file >> 8), std::uint8_t(file)};
    std::uint8_t reply[1]; std::size_t size = 0;
    return apdu(request, sizeof(request), reply, sizeof(reply), size) && size == 0;
}
bool Session::readBinary(std::uint16_t offset, std::uint8_t *out, std::size_t n, std::uint16_t mle) {
    if (!mle) return false;
    while (n) {
        const auto count = std::min<std::size_t>(n, std::min<std::size_t>(Chunk, mle));
        const std::uint8_t request[] = {0, 0xb0, std::uint8_t(offset >> 8), std::uint8_t(offset), std::uint8_t(count)};
        std::size_t size = 0;
        if (!apdu(request, sizeof(request), out, count, size) || size != count) return false;
        offset = std::uint16_t(offset + count); out += count; n -= count;
    }
    return true;
}
bool Session::updateBinary(std::uint16_t offset, const std::uint8_t *b, std::size_t n) {
    if (!n || n > Chunk) return false;
    std::uint8_t request[Chunk + 5] = {0, 0xd6, std::uint8_t(offset >> 8), std::uint8_t(offset), std::uint8_t(n)};
    std::memcpy(request + 5, b, n); std::uint8_t reply[1]; std::size_t size = 0;
    return apdu(request, n + 5, reply, sizeof(reply), size) && size == 0;
}
bool Session::page(std::uint8_t number, std::uint8_t *out) {
    const std::uint8_t request[] = {0x30, number}; std::size_t size = 0;
    return exchange(request, sizeof(request), out, 16, size) && size == 16;
}
bool Session::writePage(std::uint8_t number, const std::uint8_t *b) {
    std::uint8_t request[] = {0xa2, number, b[0], b[1], b[2], b[3]}, reply[1]; std::size_t size = 0;
    if (!exchange(request, sizeof(request), reply, sizeof(reply), size)) return false;
    // PN532's MIFARE command processing consumes ACK; tolerate an explicit ACK only.
    return size == 0 || (size == 1 && reply[0] == 0x0a);
}
Result Session::read(Snapshot &s) {
    started_ = channel_.now(); detail_ = "none"; s = Snapshot();
    const std::uint8_t request[] = {0x4a, 1, 0}; std::uint8_t reply[64]; std::size_t size = 0;
    if (!cmd(request, sizeof(request), reply, sizeof(reply), size)) return Result::Error;
    const auto result = decodeTarget(reply, size, s.target);
    if (result != Result::Ok) { detail_ = "target_activation"; return result; }
    return selected(s);
}
Result Session::selected(Snapshot &s) {
    return (s.target.sak & 0x20) ? type4(s) : type2(s);
}
Result Session::type2(Snapshot &s) {
    if (s.target.sak != 0 || s.target.uidSize != 7 || s.target.uid[0] != 4) {
        detail_ = "unsupported_type2_identity"; return Result::Unsupported;
    }
    // InDataExchange treats 0x60 as MIFARE Authenticate A. GET_VERSION must use
    // InCommunicateThru on the already selected Type-A tag (RF CRC handled by PN532).
    const std::uint8_t request[] = {0x42, 0x60}; std::uint8_t reply[9] = {}; std::size_t size = 0;
    if (!cmd(request, sizeof(request), reply, sizeof(reply), size)) return Result::Error;
    if (size != 9 || reply[0] != 0) { detail_ = "get_version_rf_status_or_length"; return Result::Unsupported; }
    const auto *version = reply + 1;
    const std::uint8_t expected[] = {0, 4, 4, 2, 1, 0};
    if (std::memcmp(version, expected, 6) || version[7] != 3) {
        detail_ = "unknown_get_version"; return Result::Unsupported;
    }
    std::uint8_t dynamic = 0, ccSize = 0;
    switch (version[6]) {
    case 0x0f: s.kind = Kind::Ntag213; dynamic = 40; ccSize = 0x12; break;
    case 0x11: s.kind = Kind::Ntag215; dynamic = 130; ccSize = 0x3e; break;
    case 0x13: s.kind = Kind::Ntag216; dynamic = 226; ccSize = 0x6d; break;
    default: detail_ = "unknown_ntag_capacity"; return Result::Unsupported;
    }
    std::uint8_t first[16] = {}, config[16] = {};
    if (!page(0, first) || !page(dynamic, config)) return Result::Error;
    // Manufacturer UID bytes and CC must match the selected, supported model.
    if (std::memcmp(first, s.target.uid, 3) || std::memcmp(first + 4, s.target.uid + 3, 4) ||
        first[12] != 0xe1 || first[13] != 0x10 || first[14] != ccSize || (first[15] >> 4) != 0) {
        detail_ = "type2_uid_or_cc"; return Result::Unsupported;
    }
    s.capacity = std::uint16_t(ccSize) * 8; s.rawSize = s.capacity;
    // Conservative policy: any set lock bit or password/mirroring excludes all writes.
    s.writable = first[15] == 0 && first[10] == 0 && first[11] == 0 &&
                 config[0] == 0 && config[1] == 0 && config[2] == 0 &&
                 config[7] == 0xff && (config[4] & 0xc0) == 0;
    for (std::size_t offset = 0; offset < s.rawSize; offset += 16) {
        std::uint8_t block[16];
        if (!page(std::uint8_t(4 + offset / 4), block)) return Result::Error;
        std::memcpy(s.bytes + offset, block, std::min<std::size_t>(16, s.rawSize - offset));
    }
    if (!parseType2(s)) { detail_ = "unsupported_type2_tlv"; return Result::Unsupported; }
    return Result::Ok;
}
Result Session::type4(Snapshot &s) {
    s.kind = Kind::Type4;
    const std::uint8_t select[] = {0, 0xa4, 4, 0, 7, 0xd2, 0x76, 0, 0, 0x85, 1, 1, 0};
    std::uint8_t reply[32]; std::size_t size = 0;
    if (!apdu(select, sizeof(select), reply, sizeof(reply), size) || !selectFile(0xe103)) return Result::Error;
    std::uint8_t cc[15];
    if (!readBinary(0, cc, sizeof(cc), 15)) return Result::Error;
    if (be16(cc) != 15 || cc[2] != 0x20 || be16(cc + 3) < 15 || be16(cc + 5) < 2 ||
        cc[7] != 4 || cc[8] != 6 || cc[13] != 0 || (cc[14] != 0 && cc[14] != 0xff)) {
        detail_ = "unsupported_type4_cc"; return Result::Unsupported;
    }
    s.file = be16(cc + 9); const auto fileSize = be16(cc + 11);
    if (s.file != 0xe104 || fileSize < 2 || fileSize > MaxMessage + 2) {
        detail_ = "unsupported_type4_file"; return Result::Unsupported;
    }
    s.capacity = fileSize - 2; s.writable = cc[14] == 0;
    s.mle = be16(cc + 3); s.mlc = be16(cc + 5);
    if (!selectFile(s.file) || !readBinary(0, reply, 2, s.mle)) return Result::Error;
    s.size = be16(reply); s.rawSize = s.size; s.offset = 0;
    if (!s.size || s.size > s.capacity || s.size > MaxMessage) { detail_ = "type4_nlen"; return Result::Unsupported; }
    if (!readBinary(2, s.bytes, s.size, s.mle)) return Result::Error;
    // NLEN may not change under us while reading chunks.
    if (!readBinary(0, reply, 2, s.mle) || be16(reply) != s.size) { detail_ = "type4_nlen_changed"; return Result::Changed; }
    return Result::Ok;
}
bool Session::same(const Snapshot &a, const Snapshot &b) const {
    return a.kind == b.kind && a.target.uidSize == b.target.uidSize &&
           std::memcmp(a.target.uid, b.target.uid, a.target.uidSize) == 0 && a.target.sak == b.target.sak &&
           a.capacity == b.capacity && a.writable == b.writable && a.rawSize == b.rawSize &&
           a.offset == b.offset && a.size == b.size && a.lengthOffset == b.lengthOffset &&
           a.mle == b.mle && a.mlc == b.mlc && a.file == b.file &&
           std::memcmp(a.bytes, b.bytes, a.rawSize) == 0;
}
Result Session::change(Snapshot &verified, const Target *expected) {
    verified = Snapshot();
    auto result = read(before_);
    if (result != Result::Ok) return result;
    if (expected && (expected->uidSize != before_.target.uidSize ||
        std::memcmp(expected->uid, before_.target.uid, expected->uidSize) != 0)) {
        detail_ = "expected_uid_changed"; return Result::Changed;
    }
    if (!before_.writable) { detail_ = "write_protection_or_mirroring"; return Result::ReadOnly; }
    next_ = before_;
    if (!prepareChange(next_)) { detail_ = "required_test_record_absent"; return Result::NotPrepared; }
    // A second live read from the same activation binds the complete preimage and metadata.
    check_ = Snapshot(); check_.target = before_.target;
    result = selected(check_);
    if (result != Result::Ok) return result;
    if (!same(before_, check_)) { detail_ = "preimage_changed"; return Result::Changed; }
    result = before_.kind == Kind::Type4 ? writeType4(next_) : writeType2(before_, next_);
    if (result != Result::Ok) return result;
    check_ = Snapshot(); check_.target = before_.target;
    result = selected(check_);
    if (result != Result::Ok || !same(next_, check_)) { detail_ = "readback_failed_or_different"; return Result::Unknown; }
    verified = check_; detail_ = "fresh_readback_exact"; return Result::Verified;
}
Result Session::writeType4(const Snapshot &next) {
    const std::uint8_t zero[] = {0, 0};
    // From the first attempted UPDATE onward every failure is uncertain, never retried.
    if (!updateBinary(0, zero, sizeof(zero))) return Result::Unknown;
    for (std::size_t offset = 0; offset < next.size;) {
        const auto count = std::min<std::size_t>(next.size - offset, std::min<std::size_t>(Chunk, next.mlc));
        if (!updateBinary(std::uint16_t(offset + 2), next.bytes + offset, count)) return Result::Unknown;
        offset += count;
    }
    const std::uint8_t length[] = {std::uint8_t(next.size >> 8), std::uint8_t(next.size)};
    return updateBinary(0, length, sizeof(length)) ? Result::Ok : Result::Unknown;
}
Result Session::writeType2(const Snapshot &before, const Snapshot &next) {
    const std::size_t lengthPage = next.lengthOffset / 4;
    std::uint8_t bytes[4]; std::memcpy(bytes, before.bytes + 4 * lengthPage, 4);
    bytes[next.lengthOffset % 4] = 0;
    if (!writePage(std::uint8_t(4 + lengthPage), bytes)) return Result::Unknown;
    // Only user pages intersecting a changed byte. All preserved bytes come from the live preimage.
    for (std::size_t offset = 0; offset < next.rawSize; offset += 4) {
        if (offset / 4 == lengthPage || std::memcmp(before.bytes + offset, next.bytes + offset, 4) == 0) continue;
        if (!writePage(std::uint8_t(4 + offset / 4), next.bytes + offset)) return Result::Unknown;
    }
    // Commit the original short TLV length last, along with any changed bytes sharing this page.
    return writePage(std::uint8_t(4 + lengthPage), next.bytes + lengthPage * 4) ? Result::Ok : Result::Unknown;
}
}
