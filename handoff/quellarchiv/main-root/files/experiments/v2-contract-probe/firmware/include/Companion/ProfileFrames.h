#pragma once
#include "Companion/ProfileSourceEnvelope.h"
namespace larp {
namespace companion {
struct ProfileFrameLimits {
    static const std::size_t HeaderBytes = 8, ValueBytes = 128, PayloadBytes = 120;
    static const std::uint8_t Version = 1, Kind = 6;
};
// Borrowed payload: valid only while the caller's input bytes remain unchanged.
struct ProfileDataFrame {
    std::uint16_t transfer = 0;
    std::uint32_t offset = 0;
    const char *payload = nullptr;
    std::size_t count = 0;
    bool valid() const noexcept;
};
bool decodeProfileDataFrame(const char *bytes, std::size_t count,
                            ProfileDataFrame &destination) noexcept;
bool encodeProfileDataFrame(const ProfileDataFrame &frame, char *bytes, std::size_t capacity,
                            std::size_t &written) noexcept;
enum class ProfileFrameRead : std::uint8_t { Value, End, Failure };
class ISealedProfileFrames {
  public:
    virtual ~ISealedProfileFrames() = default;
    // COMPLETE, exclusive synchronous source. Copies one entire value into the
    // supplied storage. End is final and sets written=0. Empty live queue is never End.
    virtual ProfileFrameRead readFrame(char *bytes, std::size_t capacity, std::size_t &written) = 0;
};
// One owned fixed frame; no document buffer/Profile/heap. This reader checks
// exact transfer/order/length/final End. Import checks original rights/CRC/time.
class FramedProfileReader : public configuration::IProfileReader {
  public:
    FramedProfileReader(ISealedProfileFrames &source,
                        const ProfileSourceEnvelope &envelope) noexcept;
    FramedProfileReader(const FramedProfileReader &) = delete;
    FramedProfileReader &operator=(const FramedProfileReader &) = delete;
    configuration::ReadStatus readByte(std::uint8_t &byte) override;

  private:
    ISealedProfileFrames &source_;
    const ProfileSourceEnvelope envelope_;
    char value_[ProfileFrameLimits::ValueBytes] = {};
    std::uint32_t consumed_ = 0;
    std::size_t position_ = 0, payloadBytes_ = 0;
    configuration::ReadStatus terminal_ = configuration::ReadStatus::Byte;
};
} // namespace companion
} // namespace larp
