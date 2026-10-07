#pragma once
#include "Companion/PeerRegistry.h"
// Private fixed public-registry namespace/key. Sole startup initialization;
// runtime operations share one zero-wait static mutex, with same-boot fence.
// No erase/format/recovery/reset and no private keys. NVS runs on a worker.
class Esp32PeerRegistryStore : public larp::companion::IPeerRegistryStore {
  public:
    bool begin() noexcept;
    larp::companion::PeerRegistryRead read(larp::companion::PeerRegistryRecord &) noexcept override;
    larp::companion::PeerRegistryWrite
    replace(const larp::companion::PeerRegistryRecord *,
            const larp::companion::PeerRegistryRecord &) noexcept override;
    void fence() noexcept override;

  private:
    bool initialized_ = false;
};
