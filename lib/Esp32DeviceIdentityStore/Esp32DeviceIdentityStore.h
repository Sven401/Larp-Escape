#pragma once
#include "Companion/DeviceIdentityStore.h"
// Fixed public-ID namespace/key. Initialize before workers; runtime operations
// share one static zero-wait mutex. No secret/GM/erase/deinit/rotation API.
class Esp32DeviceIdentityStore : public larp::companion::IDeviceIdentityRecordStore {
  public:
    bool begin() noexcept;
    larp::companion::IdentityRecordRead
    read(larp::companion::DeviceIdentityRecord &destination) noexcept override;
    larp::companion::IdentityRecordCreate
    createIfMissing(const larp::companion::DeviceIdentityRecord &record) noexcept override;

  private:
    bool initialized_ = false;
};
