#include "ProfileReceive.h"
#include "TemporaryProfileFile.h"
#include <chrono>
#include <cstdio>
#include <memory>
namespace {
using namespace larp::companion;
class HostClock : public IProfileImportClock {
  public:
    std::uint32_t nowMilliseconds() const noexcept override {
        return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count());
    }
};
// Host FAKE authority only. It models contract attribution, never a real peer.
class HostAuthority : public IGameControlChannel {
  public:
    TrustedAuthority value;
    bool readAuthority(TrustedAuthority &destination) const noexcept override {
        destination = value;
        return true;
    }
    bool receive(TrustedGameRequest &) noexcept override {
        return false;
    }
    bool send(const RoutedGameReply &) noexcept override {
        return false;
    }
    bool publish(const RoutedConfiguredStatus &) noexcept override {
        return false;
    }
};
int digit(int byte) {
    return byte >= '0' && byte <= '9'   ? byte - '0'
           : byte >= 'A' && byte <= 'F' ? byte - 'A' + 10
                                        : -1;
}
// Whole bounded hex value; never strlen/fgets truncation at an embedded NUL.
ProfileFrameRead readValue(std::FILE *file, char *bytes, std::size_t &count) {
    count = 0;
    int high = -1;
    bool saw = false, carriage = false;
    for (;;) {
        const int byte = std::fgetc(file);
        if (byte == EOF || byte == 10) {
            if (std::ferror(file) || high >= 0 || (!saw && byte != EOF))
                return ProfileFrameRead::Failure;
            return saw ? ProfileFrameRead::Value : ProfileFrameRead::End;
        }
        if (carriage)
            return ProfileFrameRead::Failure;
        if (byte == 13) {
            carriage = true;
            if (!saw || high >= 0)
                return ProfileFrameRead::Failure;
            continue;
        }
        const int value = digit(byte);
        if (value < 0 || count == ProfileFrameLimits::ValueBytes)
            return ProfileFrameRead::Failure;
        saw = true;
        if (high < 0)
            high = value;
        else {
            bytes[count++] = static_cast<char>((high << 4) | value);
            high = -1;
        }
    }
}
} // namespace
int runProfileReceive(const char *path) {
    HostClock clock;
    HostAuthority authority;
    authority.value.principal = 7;
    authority.value.epoch = 1;
    authority.value.gameMaster = true;
    TemporaryProfileFile file;
    ProfileStaging source(file, authority, clock);
    auto *input = std::fopen(path, "rb");
    if (!input)
        return 1;
    char value[ProfileFrameLimits::ValueBytes];
    std::size_t count = 0;
    ProfileSourceEnvelope envelope;
    bool ok = readValue(input, value, count) == ProfileFrameRead::Value &&
              decodeProfileSourceEnvelope(value, count, envelope) &&
              source.start(envelope, authority.value, clock.nowMilliseconds()) ==
                  ProfileReceiveOutcome::Receiving;
    while (ok) {
        const auto next = readValue(input, value, count);
        if (next == ProfileFrameRead::End)
            break;
        ok = next == ProfileFrameRead::Value &&
             source.data(value, count, authority.value) == ProfileReceiveOutcome::Receiving;
    }
    const bool closed = std::fclose(input) == 0;
    if (!ok || !closed ||
        source.finish(authority.value, envelope.transfer) != ProfileReceiveOutcome::Complete)
        return 1;
    std::unique_ptr<larp::configuration::Profile> profile(new larp::configuration::Profile);
    const auto imported = larp::configuration::readDraftProfile(source, *profile);
    if (!imported || source.discard() != ProfileReceiveOutcome::Discarded)
        return 1;
    const int emitted =
        std::printf("{\"scope\":\"host_profile_receive\",\"transfer\":%u,\"bytes\":%lu,"
                    "\"profileRevision\":%lu,\"complete\":true,\"verification\":\"host_fake\","
                    "\"persistentSave\":false,\"radioMeasured\":false}",
                    envelope.transfer, static_cast<unsigned long>(envelope.bytes),
                    static_cast<unsigned long>(profile->revision));
    return emitted > 0 && std::fputc(10, stdout) != EOF && std::fflush(stdout) == 0 ? 0 : 1;
}
