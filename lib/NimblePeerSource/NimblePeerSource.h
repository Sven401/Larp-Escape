#pragma once
#include "PinnedPeerTls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
extern "C" {
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
}
namespace larp {
namespace companion {
struct PeerCipherFragment {
    static const std::size_t NotifyBytes = 20; // Safe even at default ATT MTU 23.
    TlsOrigin origin;
    std::array<unsigned char, PinnedPeerTls::FragmentBytes> bytes{};
    std::uint16_t count = 0;
};
class NimblePeerSource;
class IPeerCipherIngress {
  public:
    virtual ~IPeerCipherIngress() = default;
    // Fixed copies only, zero-wait. False fences the source; no silent drops.
    virtual bool open(const TlsOrigin &) noexcept = 0;
    virtual bool push(const PeerCipherFragment &) noexcept = 0;
    // Trusted physical UI only: displaying a number does not accept pairing.
    // Host-only, bounded zero-wait outgoing-mailbox poll. False fences link.
    virtual bool poll(NimblePeerSource &) noexcept = 0;
    virtual void compare(const TlsOrigin &, std::uint32_t number) noexcept = 0;
};
enum class PeerNotify : std::uint8_t { Sent, Busy, Stale, Failed };
// Process-lifetime singleton. Host task alone owns GAP/GATT/security state.
// Does not assert global HCI/ACL ordering or grant roles; S2 device gate remains.
class NimblePeerSource final : public ITlsLifetime {
  public:
    explicit NimblePeerSource(IPeerCipherIngress &v) noexcept : ingress_(v) {}
    NimblePeerSource(const NimblePeerSource &) = delete;
    NimblePeerSource &operator=(const NimblePeerSource &) = delete;
    // Sole trusted startup caller, before launching any competing BLE owner.
    bool begin() noexcept;
    bool current(const TlsOrigin &) const noexcept override;
    // Any trusted worker may irreversibly fence this original link. Host poll
    // then terminates it. Neither call can authorize a connection or role.
    void revoke(const TlsOrigin &) noexcept;
    // Original numeric comparison from a trusted physical administrator.
    bool confirm(const TlsOrigin &, std::uint32_t number, bool accepted) noexcept;
    // Called by host-owned periodic consumer only, at most NotifyBytes.
    // The current() lock snapshot selects this copied ciphertext atomically.
    // A subsequent worker revoke prevents later selections; it cannot retract
    // an already selected transport submission. This is not role authorization.
    // No global attribute reads or SDK/queue work under the snapshot spinlock.
    PeerNotify notify(const PeerCipherFragment &) noexcept;
    bool ready() const noexcept;

  private:
    IPeerCipherIngress &ingress_;
    mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    TlsOrigin view_;
    mutable bool eligible_ = false, banned_ = false, ready_ = false, heartbeatFailed_ = false;
    std::uint32_t heartbeat_ = 0;
    static const std::uint32_t HeartbeatMilliseconds = 100;
    struct Confirmation {
        TlsOrigin origin;
        std::uint32_t number = 0;
        bool accepted = false;
    };
    Confirmation confirmation_;
    bool confirmationPending_ = false;
    TaskHandle_t host_ = nullptr;
    StaticTask_t hostStorage_{};
    // ESP-IDF takes stack size in BYTES; StackType_t is port-dependent.
    alignas(portBYTE_ALIGNMENT) std::array<StackType_t, 8192 / sizeof(StackType_t)> hostStack_{};
    TlsOrigin original_;
    bool attempted_ = false, live_ = false, opened_ = false, subscribed_ = false;
    bool comparing_ = false, broken_ = false;
    std::uint32_t epoch_ = 0, compareNumber_ = 0, compareAt_ = 0;
    std::uint16_t receiveHandle_ = 0, transmitHandle_ = 0;
    std::uint8_t addressType_ = 0;
    ble_gatt_chr_def characteristics_[3]{};
    ble_gatt_svc_def services_[2]{};
    ble_npl_callout poll_{};
    static NimblePeerSource *selected_;
    bool onHost() const noexcept;
    bool secure() const noexcept;
    void publish(bool) noexcept;
    void fence() noexcept;
    void update() noexcept;
    void advertise() noexcept;
    void poll() noexcept;
    static int access(std::uint16_t, std::uint16_t, ble_gatt_access_ctxt *, void *);
    static int gap(ble_gap_event *, void *);
    static void sync();
    static void reset(int);
    static void run(void *);
    static void tick(ble_npl_event *);
};
} // namespace companion
} // namespace larp
