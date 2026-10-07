#pragma once
#include "Esp32GattsSource.h"
#include "SdkGattsService.h"
namespace larp {
namespace companion {
enum class GattsServicePhase : std::uint8_t {
    Idle,
    Creating,
    Command,
    Profile,
    Installing,
    Starting,
    Ready,
    Failed
};
enum class GattsServiceCopy : std::uint8_t { Copied, Ignored, Unavailable, Overflow };
struct GattsServiceSnapshot {
    GattsServicePhase phase = GattsServicePhase::Idle;
    GattsEndpointSetup setup;
};
// One callback producer copies; one separate worker consumes. begin on that
// worker before SDK initialization. SDK API calls may allocate/wait there.
// Process lifetime, exactly one startup attempt; no automatic SDK reset/retry.
class Esp32GattsService {
  public:
    Esp32GattsService(Esp32GattsSource &source, IWriteResponseBearer &responses,
                      const IProfileImportClock &clock, SdkGattsServiceDriver &driver)
        : source_(source), responses_(responses), clock_(clock), driver_(driver) {}
    Esp32GattsService(const Esp32GattsService &) = delete;
    Esp32GattsService &operator=(const Esp32GattsService &) = delete;
    bool begin() noexcept;
    GattsServiceCopy copyEvent(const GattsServiceEvent &) noexcept;
    // <=1 received event and <=1 SDK/configuration post per worker call.
    void service() noexcept;
    GattsServiceSnapshot snapshot() const noexcept;
    bool ready() const noexcept;

  private:
    struct Copied {
        GattsSourceRegistration original;
        GattsServiceEvent event;
        std::uint32_t receivedAt = 0;
    };
    static const std::size_t Depth = 4;
    Esp32GattsSource &source_;
    IWriteResponseBearer &responses_;
    const IProfileImportClock &clock_;
    SdkGattsServiceDriver &driver_;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t control_ = {};
    alignas(Copied) std::uint8_t storage_[Depth * sizeof(Copied)] = {};
    mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    GattsServiceSnapshot publication_;
    GattsEndpointSetup setup_;
    GattsServiceEvent previous_;
    GattsSourceRegistration previousOriginal_;
    TaskHandle_t worker_ = nullptr, producer_ = nullptr;
    std::uint32_t startedAt_ = 0;
    bool broken_ = false, busy_ = false, havePrevious_ = false;
    GattsServicePhase phase_ = GattsServicePhase::Idle;
    void phase(GattsServicePhase) noexcept;
    bool current(const Copied &) const noexcept;
};
} // namespace companion
} // namespace larp
