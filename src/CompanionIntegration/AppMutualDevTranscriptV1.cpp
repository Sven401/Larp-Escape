#include "CompanionIntegration/AppMutualDevTranscriptV1.h"

namespace larp {
namespace app_mutual_dev {
namespace {
template <std::size_t N> bool populated(const std::array<std::uint8_t, N> &value) noexcept {
    for (std::uint8_t byte : value) if (byte) return true;
    return false;
}
bool publicShape(const PublicP256 &value) noexcept {
    if (value[0] != 4) return false;
    for (std::size_t i = 1; i < value.size(); ++i) if (value[i]) return true;
    return false;
}

bool valid(const Transcript &v) noexcept {
    if (!populated(v.chip) || !populated(v.deviceId) || !v.bootId ||
        !v.sourceRegistration || !v.sdkGeneration || v.gattInterface == 0xff ||
        !populated(v.peerAddress) || !v.connectionToken || !v.connectionHandle ||
        !populated(v.phoneInstallId) || !populated(v.boxKeyId) ||
        !populated(v.phoneKeyId) || v.boxKeyId == v.phoneKeyId ||
        !publicShape(v.boxPublic) || !publicShape(v.phonePublic) ||
        !populated(v.boxNonce) || !populated(v.phoneNonce) ||
        v.boxNonce == v.phoneNonce || !v.attempt ||
        !v.boxEnrollmentGeneration || !v.phoneEnrollmentGeneration)
        return false;
    return true;
}

struct Writer {
    Frame &out;
    std::size_t pos;
    explicit Writer(Frame &target) noexcept : out(target), pos(0) {}
    void byte(std::uint8_t v) noexcept { out[pos++] = v; }
    template <std::size_t N> void array(const std::array<std::uint8_t, N> &v) noexcept {
        for (std::uint8_t byteValue : v) byte(byteValue);
    }
    void u16(std::uint16_t v) noexcept { byte(static_cast<std::uint8_t>(v >> 8)); byte(static_cast<std::uint8_t>(v)); }
    void u32(std::uint32_t v) noexcept { for (int shift = 24; shift >= 0; shift -= 8) byte(static_cast<std::uint8_t>(v >> shift)); }
    void u64(std::uint64_t v) noexcept { for (int shift = 56; shift >= 0; shift -= 8) byte(static_cast<std::uint8_t>(v >> shift)); }
};

struct Reader {
    const std::uint8_t *bytes;
    std::size_t pos;
    explicit Reader(const std::uint8_t *source) noexcept : bytes(source), pos(8) {}
    std::uint8_t byte() noexcept { return bytes[pos++]; }
    template <std::size_t N> void array(std::array<std::uint8_t, N> &v) noexcept {
        for (std::size_t i = 0; i < N; ++i) v[i] = byte();
    }
    std::uint16_t u16() noexcept { return static_cast<std::uint16_t>((byte() << 8) | byte()); }
    std::uint32_t u32() noexcept { std::uint32_t v = 0; for (int i = 0; i < 4; ++i) v = (v << 8) | byte(); return v; }
    std::uint64_t u64() noexcept { std::uint64_t v = 0; for (int i = 0; i < 8; ++i) v = (v << 8) | byte(); return v; }
};
} // namespace

bool encode(const Transcript &v, Frame &out) noexcept {
    if (!valid(v)) return false;
    Frame candidate = {};
    Writer w(candidate);
    w.byte(0x41); w.byte(0x4d); w.byte(0x44); w.byte(0x31); // AMD1
    w.byte(1); w.byte(0xff); // fixture version, explicitly no selected suite
    w.byte(0x42); w.byte(0x50); // ordered box, phone roles
    w.array(v.chip); w.array(v.deviceId); w.u64(v.bootId);
    w.u32(v.sourceRegistration); w.u32(v.sdkGeneration); w.byte(v.gattInterface);
    w.array(v.peerAddress); w.u32(v.connectionToken); w.u16(v.connectionHandle);
    w.array(v.phoneInstallId); w.array(v.boxKeyId); w.array(v.boxPublic);
    w.array(v.phoneKeyId); w.array(v.phonePublic);
    w.array(v.boxNonce); w.array(v.phoneNonce); w.u32(v.attempt);
    w.u32(v.boxEnrollmentGeneration); w.u32(v.phoneEnrollmentGeneration);
    w.u32(v.boxRevocationGeneration); w.u32(v.phoneRevocationGeneration);
    if (w.pos != FrameSize) return false;
    out = candidate;
    return true;
}

bool decode(const std::uint8_t *bytes, std::size_t size, Transcript &out) noexcept {
    if (!bytes || size != FrameSize || bytes[0] != 0x41 || bytes[1] != 0x4d ||
        bytes[2] != 0x44 || bytes[3] != 0x31 || bytes[4] != 1 ||
        bytes[5] != 0xff || bytes[6] != 0x42 || bytes[7] != 0x50)
        return false;
    Reader r(bytes);
    Transcript candidate;
    r.array(candidate.chip); r.array(candidate.deviceId); candidate.bootId = r.u64();
    candidate.sourceRegistration = r.u32(); candidate.sdkGeneration = r.u32();
    candidate.gattInterface = r.byte(); r.array(candidate.peerAddress);
    candidate.connectionToken = r.u32(); candidate.connectionHandle = r.u16();
    r.array(candidate.phoneInstallId); r.array(candidate.boxKeyId); r.array(candidate.boxPublic);
    r.array(candidate.phoneKeyId); r.array(candidate.phonePublic);
    r.array(candidate.boxNonce); r.array(candidate.phoneNonce); candidate.attempt = r.u32();
    candidate.boxEnrollmentGeneration = r.u32(); candidate.phoneEnrollmentGeneration = r.u32();
    candidate.boxRevocationGeneration = r.u32(); candidate.phoneRevocationGeneration = r.u32();
    if (r.pos != FrameSize || !valid(candidate)) return false;
    out = candidate;
    return true;
}

bool ClosedFake::begin(const Transcript &trustedLocal) noexcept {
    if (state_ != State::Empty) return false;
    if (!encode(trustedLocal, expected_)) { state_ = State::Closed; return false; }
    state_ = State::AwaitFrame;
    return true;
}

bool ClosedFake::check(const std::uint8_t *received, std::size_t size) noexcept {
    if (state_ != State::AwaitFrame) return false;
    Transcript parsed;
    if (!decode(received, size, parsed)) { state_ = State::Closed; return false; }
    for (std::size_t i = 0; i < FrameSize; ++i)
        if (received[i] != expected_[i]) { state_ = State::Closed; return false; }
    state_ = State::TranscriptCheckedClosed;
    return true;
}

} // namespace app_mutual_dev
} // namespace larp
