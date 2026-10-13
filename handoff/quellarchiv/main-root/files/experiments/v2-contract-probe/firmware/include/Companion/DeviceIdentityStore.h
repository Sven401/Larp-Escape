#pragma once
#include "Companion/Identity.h"
namespace larp {
namespace companion {
struct DeviceIdentityRecord {
    std::array<std::uint8_t, 28> bytes = {};
    bool operator==(const DeviceIdentityRecord &other) const noexcept {
        return bytes == other.bytes;
    }
};
// Storage encoding only. CRC detects corruption, never authenticity or a role.
bool encodeDeviceIdentityRecord(const Identity &identity,
                                DeviceIdentityRecord &destination) noexcept;
bool decodeDeviceIdentityRecord(const DeviceIdentityRecord &record, Identity &destination) noexcept;
enum class IdentityRecordRead : std::uint8_t { Present, Missing, Failure };
enum class IdentityRecordCreate : std::uint8_t { Confirmed, AlreadyPresent, Uncertain, Failure };
class IDeviceIdentityRecordStore {
  public:
    virtual ~IDeviceIdentityRecordStore() = default;
    // Missing is only a positively observed absent namespace/key. Failures must
    // not modify the destination or be mistaken for an invitation to replace.
    virtual IdentityRecordRead read(DeviceIdentityRecord &destination) noexcept = 0;
    // Exclusive namespace owner: recheck absence and never replace a value.
    virtual IdentityRecordCreate createIfMissing(const DeviceIdentityRecord &record) noexcept = 0;
};
class IDeviceIdentityEntropy {
  public:
    virtual ~IDeviceIdentityEntropy() = default;
    virtual bool make(Identity &destination) noexcept = 0;
};
enum class DeviceIdentityOutcome : std::uint8_t {
    Loaded,
    Created,
    Busy,
    ReadFailure,
    Corrupt,
    EntropyFailure,
    WriteFailure,
    Uncertain,
    VerifyFailure
};
// Trusted serialized startup owner. Immutable display ID only, no peer evidence,
// cryptographic key, GATT origin, GM assignment, automatic reset/rotate/erase.
// A failed begin stays failed until a new startup owner is explicitly created.
class DeviceIdentityOwner {
  public:
    DeviceIdentityOwner(IDeviceIdentityRecordStore &store, IDeviceIdentityEntropy &entropy) noexcept
        : store_(store), entropy_(entropy) {}
    DeviceIdentityOwner(const DeviceIdentityOwner &) = delete;
    DeviceIdentityOwner &operator=(const DeviceIdentityOwner &) = delete;
    DeviceIdentityOutcome begin() noexcept;
    bool identity(Identity &destination) const noexcept;

  private:
    IDeviceIdentityRecordStore &store_;
    IDeviceIdentityEntropy &entropy_;
    Identity identity_;
    DeviceIdentityOutcome outcome_ = DeviceIdentityOutcome::ReadFailure;
    bool attempted_ = false, ready_ = false, busy_ = false;
};
} // namespace companion
} // namespace larp
