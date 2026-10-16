#pragma once
#include <cstddef>
#include <cstdint>

namespace nfcprobe {
constexpr std::size_t MaxMessage = 1024;
constexpr std::size_t MaxFrame = 262;
constexpr std::size_t Chunk = 64;
enum class Result { Ok, NoTarget, Unsupported, ReadOnly, NotPrepared, Changed, Error, Unknown, Verified };
const char *name(Result result);
enum class Kind { Unknown, Type4, Ntag213, Ntag215, Ntag216 };
const char *name(Kind kind);
struct Target {
    std::uint8_t uid[10] = {};
    std::uint8_t uidSize = 0, sak = 0, atqa[2] = {};
};
struct Snapshot {
    Target target;
    Kind kind = Kind::Unknown;
    bool writable = false;
    std::uint16_t capacity = 0, rawSize = 0, offset = 0, size = 0, lengthOffset = 0;
    std::uint16_t mle = 0, mlc = 0, file = 0;
    std::uint8_t bytes[MaxMessage] = {};
};

// The channel returns validated response payload, excluding D5/response code.
class Channel {
 public:
    virtual ~Channel() {}
    virtual bool command(const std::uint8_t *command, std::size_t size,
                         std::uint8_t *reply, std::size_t capacity, std::size_t &received) = 0;
    virtual std::uint32_t now() const = 0;
};
// Only normal PN532 frames are supported. No truncation, stale tail or extended frame.
bool decodeFrame(const std::uint8_t *frame, std::size_t size, std::uint8_t expected,
                 std::uint8_t *payload, std::size_t capacity, std::size_t &received);
std::size_t encodeCommand(const std::uint8_t *command, std::size_t size,
                          std::uint8_t *frame, std::size_t capacity);
Result decodeTarget(const std::uint8_t *bytes, std::size_t size, Target &target);
std::uint32_t crc32(const std::uint8_t *bytes, std::size_t size);
// Find and validate the unique game record; preserve all other unchunked NDEF records.
bool gamePayload(const std::uint8_t *message, std::size_t size,
                 std::size_t &payloadOffset, std::uint16_t &flags);
bool isPrepared(const std::uint8_t *message, std::size_t size);
bool prepareChange(Snapshot &snapshot);
bool parseType2(Snapshot &snapshot);

class Session {
 public:
    explicit Session(Channel &channel) : channel_(channel) {}
    Result read(Snapshot &snapshot);
    Result change(Snapshot &verified, const Target *expected = nullptr);
    const char *detail() const { return detail_; }
 private:
    bool cmd(const std::uint8_t *bytes, std::size_t size, std::uint8_t *out,
             std::size_t capacity, std::size_t &received);
    bool exchange(const std::uint8_t *bytes, std::size_t size, std::uint8_t *out,
                  std::size_t capacity, std::size_t &received);
    bool apdu(const std::uint8_t *bytes, std::size_t size, std::uint8_t *out,
              std::size_t capacity, std::size_t &received);
    bool selectFile(std::uint16_t file);
    bool readBinary(std::uint16_t offset, std::uint8_t *out, std::size_t size, std::uint16_t mle);
    bool updateBinary(std::uint16_t offset, const std::uint8_t *bytes, std::size_t size);
    bool page(std::uint8_t number, std::uint8_t *out);
    bool writePage(std::uint8_t number, const std::uint8_t *bytes);
    Result selected(Snapshot &snapshot);
    Result type2(Snapshot &snapshot);
    Result type4(Snapshot &snapshot);
    Result writeType2(const Snapshot &before, const Snapshot &next);
    Result writeType4(const Snapshot &next);
    bool same(const Snapshot &a, const Snapshot &b) const;
    Channel &channel_;
    std::uint32_t started_ = 0;
    const char *detail_ = "none";
    // Private static-size scratch owned by this one session, not on the ESP task stack.
    Snapshot before_, check_, next_;
};
}
