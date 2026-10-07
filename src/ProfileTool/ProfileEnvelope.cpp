#include "ProfileEnvelope.h"
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
