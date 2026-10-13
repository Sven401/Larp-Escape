#include "Companion/DeviceIdentityStore.h"
#include "Companion/ProfileSourceEnvelope.h"
namespace larp {
namespace companion {
namespace {
std::uint32_t recordCrc(const DeviceIdentityRecord &record) noexcept {
    std::uint32_t crc = 0xffffffffu;
    for (unsigned i = 0; i < 24; ++i)
        crc = profileSourceCrcByte(crc, record.bytes[i]);
    return crc ^ 0xffffffffu;
}
class IdentityCall {
  public:
    explicit IdentityCall(bool &busy) : busy_(busy) {
        busy_ = true;
    }
    ~IdentityCall() {
        busy_ = false;
    }

  private:
    bool &busy_;
};
} // namespace
bool encodeDeviceIdentityRecord(const Identity &identity,
                                DeviceIdentityRecord &destination) noexcept {
    const auto selected = identity;
    if (!selected.valid())
        return false;
    DeviceIdentityRecord record;
    record.bytes[0] = 'L';
    record.bytes[1] = 'D';
    record.bytes[2] = 'I';
    record.bytes[3] = 'D';
    record.bytes[4] = 1;
    for (unsigned i = 0; i < 16; ++i)
        record.bytes[8 + i] = selected.bytes[i];
    const auto crc = recordCrc(record);
    for (unsigned i = 0; i < 4; ++i)
        record.bytes[24 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
    destination = record;
    return true;
}
bool decodeDeviceIdentityRecord(const DeviceIdentityRecord &caller,
                                Identity &destination) noexcept {
    const auto record = caller;
    if (record.bytes[0] != 'L' || record.bytes[1] != 'D' || record.bytes[2] != 'I' ||
        record.bytes[3] != 'D' || record.bytes[4] != 1 || record.bytes[5] || record.bytes[6] ||
        record.bytes[7])
        return false;
    std::uint32_t crc = 0;
    for (unsigned i = 0; i < 4; ++i)
        crc |= std::uint32_t(record.bytes[24 + i]) << (8 * i);
    if (crc != recordCrc(record))
        return false;
    Identity identity;
    for (unsigned i = 0; i < 16; ++i)
        identity.bytes[i] = record.bytes[8 + i];
    if (!identity.valid())
        return false;
    destination = identity;
    return true;
}
DeviceIdentityOutcome DeviceIdentityOwner::begin() noexcept {
    if (busy_)
        return DeviceIdentityOutcome::Busy;
    if (attempted_)
        return outcome_;
    IdentityCall call(busy_);
    attempted_ = true;
    DeviceIdentityRecord record;
    const auto read = store_.read(record);
    if (read == IdentityRecordRead::Present) {
        if (!decodeDeviceIdentityRecord(record, identity_))
            return outcome_ = DeviceIdentityOutcome::Corrupt;
        ready_ = true;
        return outcome_ = DeviceIdentityOutcome::Loaded;
    }
    if (read != IdentityRecordRead::Missing)
        return outcome_ = DeviceIdentityOutcome::ReadFailure;
    Identity candidate;
    if (!entropy_.make(candidate) || !encodeDeviceIdentityRecord(candidate, record))
        return outcome_ = DeviceIdentityOutcome::EntropyFailure;
    const auto selected = record;
    const auto created = store_.createIfMissing(selected);
    if (created == IdentityRecordCreate::Uncertain)
        return outcome_ = DeviceIdentityOutcome::Uncertain;
    if (created != IdentityRecordCreate::Confirmed &&
        created != IdentityRecordCreate::AlreadyPresent)
        return outcome_ = DeviceIdentityOutcome::WriteFailure;
    DeviceIdentityRecord confirmed;
    Identity identity;
    if (store_.read(confirmed) != IdentityRecordRead::Present ||
        !decodeDeviceIdentityRecord(confirmed, identity))
        return outcome_ = DeviceIdentityOutcome::VerifyFailure;
    if (created == IdentityRecordCreate::Confirmed && !(confirmed == selected))
        return outcome_ = DeviceIdentityOutcome::VerifyFailure;
    identity_ = identity;
    ready_ = true;
    return outcome_ = created == IdentityRecordCreate::Confirmed ? DeviceIdentityOutcome::Created
                                                                 : DeviceIdentityOutcome::Loaded;
}
bool DeviceIdentityOwner::identity(Identity &destination) const noexcept {
    if (busy_ || !ready_)
        return false;
    destination = identity_;
    return true;
}
} // namespace companion
} // namespace larp
