#include "ProfileEnvelope.h"
#include "Companion/ProfileFrames.h"
#include "Companion/ProfileSourceEnvelope.h"
#include "GameConfiguration/ProfileReader.h"
#include <cstdio>
#include <memory>
namespace {
bool unsignedDecimal(const char *text, std::uint32_t &value) {
    if (!text || !*text)
        return false;
    std::uint32_t result = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9' || result > (UINT32_MAX - unsigned(*p - '0')) / 10)
            return false;
        result = result * 10 + unsigned(*p - '0');
    }
    value = result;
    return true;
}
class MeasuredFile : public larp::configuration::IProfileReader {
  public:
    explicit MeasuredFile(std::FILE *file) : file_(file) {}
    larp::configuration::ReadStatus readByte(std::uint8_t &byte) override {
        using larp::configuration::ReadStatus;
        const int value = std::fgetc(file_);
        if (value == EOF)
            return std::ferror(file_) ? ReadStatus::Failure : ReadStatus::End;
        if (count == larp::configuration::ImportLimits::DocumentBytes)
            return ReadStatus::Failure;
        ++count;
        byte = static_cast<std::uint8_t>(value);
        crc = larp::companion::profileSourceCrcByte(crc, byte);
        return ReadStatus::Byte;
    }
    std::uint32_t count = 0, crc = 0xffffffffu;

  private:
    std::FILE *file_;
};
} // namespace
int runProfileEnvelope(const char *path, const char *draft, const char *configuration) {
    using namespace larp::companion;
    ProfileSourceEnvelope envelope;
    envelope.transfer = 1;
    if (!unsignedDecimal(draft, envelope.expectedDraftRevision) ||
        !unsignedDecimal(configuration, envelope.expectedConfigurationRevision) ||
        envelope.expectedDraftRevision == UINT32_MAX || !envelope.expectedConfigurationRevision)
        return 2;
    auto *file = std::fopen(path, "rb");
    if (!file)
        return 1;
    MeasuredFile reader(file);
    std::unique_ptr<larp::configuration::Profile> profile(new larp::configuration::Profile);
    const auto parsed = larp::configuration::readDraftProfile(reader, *profile);
    const bool closed = std::fclose(file) == 0;
    if (!parsed || !closed)
        return 1;
    envelope.bytes = reader.count;
    envelope.crc32 = reader.crc ^ 0xffffffffu;
    char encoded[ProfileSourceLimits::EnvelopeBytes];
    std::size_t written = 0;
    if (!encodeProfileSourceEnvelope(envelope, encoded, sizeof(encoded), written))
        return 1;
    for (std::size_t i = 0; i < written; ++i)
        if (std::printf("%02X", static_cast<unsigned char>(encoded[i])) != 2)
            return 1;
    return std::fputc(10, stdout) != EOF && std::fflush(stdout) == 0 ? 0 : 1;
}

int runProfileFrames(const char *path, const char *draft, const char *configuration,
                     const char *maximumValue) {
    using namespace larp::companion;
    ProfileSourceEnvelope envelope;
    envelope.transfer = 1;
    std::uint32_t maximum = 0;
    if (!unsignedDecimal(draft, envelope.expectedDraftRevision) ||
        !unsignedDecimal(configuration, envelope.expectedConfigurationRevision) ||
        envelope.expectedDraftRevision == UINT32_MAX || !envelope.expectedConfigurationRevision ||
        !unsignedDecimal(maximumValue, maximum) || maximum < ProfileSourceLimits::EnvelopeBytes ||
        maximum > ProfileFrameLimits::ValueBytes)
        return 2;
    auto *file = std::fopen(path, "rb");
    if (!file)
        return 1;
    MeasuredFile reader(file);
    std::unique_ptr<larp::configuration::Profile> profile(new larp::configuration::Profile);
    const auto parsed = larp::configuration::readDraftProfile(reader, *profile);
    const bool rewound = std::fseek(file, 0, SEEK_SET) == 0;
    if (!parsed || !rewound) {
        std::fclose(file);
        return 1;
    }
    profile.reset(); // No Profile is retained while emitting frame values.
    envelope.bytes = reader.count;
    envelope.crc32 = reader.crc ^ 0xffffffffu;
    char encoded[ProfileFrameLimits::ValueBytes], payload[ProfileFrameLimits::PayloadBytes];
    auto emit = [](const char *bytes, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i)
            if (std::printf("%02X", static_cast<unsigned char>(bytes[i])) != 2)
                return false;
        return std::fputc(10, stdout) != EOF;
    };
    std::size_t written = 0;
    bool ok = encodeProfileSourceEnvelope(envelope, encoded, sizeof(encoded), written) &&
              emit(encoded, written);
    MeasuredFile second(file);
    while (ok && second.count < envelope.bytes) {
        const auto offset = second.count;
        std::size_t count = 0;
        while (count < maximum - ProfileFrameLimits::HeaderBytes && second.count < envelope.bytes) {
            std::uint8_t byte = 0;
            if (second.readByte(byte) != larp::configuration::ReadStatus::Byte) {
                ok = false;
                break;
            }
            payload[count++] = static_cast<char>(byte);
        }
        ProfileDataFrame frame;
        frame.transfer = envelope.transfer;
        frame.offset = offset;
        frame.payload = payload;
        frame.count = count;
        ok = ok && encodeProfileDataFrame(frame, encoded, sizeof(encoded), written) &&
             emit(encoded, written);
    }
    std::uint8_t extra = 0;
    ok = ok && second.readByte(extra) == larp::configuration::ReadStatus::End &&
         second.count == envelope.bytes && (second.crc ^ 0xffffffffu) == envelope.crc32;
    const bool closed = std::fclose(file) == 0;
    return ok && closed && std::fflush(stdout) == 0 ? 0 : 1;
}
