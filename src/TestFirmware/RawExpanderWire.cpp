#include "RawExpanderWire.h"

larp::diagnostic::RawRegisterRead RawExpanderWireReader::read(
    std::uint8_t address, std::uint8_t reg) noexcept {
    using larp::diagnostic::RawRegisterRead;
    using larp::diagnostic::RawRegisterStatus;
    wire_.beginTransmission(address);
    if (wire_.write(reg) != 1) {
        (void)wire_.endTransmission(true);
        return {RawRegisterStatus::RegisterSelectFailed, 0, 0xff};
    }
    const std::uint8_t code = wire_.endTransmission(false);
    if (code != 0)
        return {RawRegisterStatus::RegisterSelectFailed, 0, code};
    if (wire_.requestFrom(address, static_cast<std::uint8_t>(1),
                          static_cast<std::uint8_t>(true)) != 1)
        return {RawRegisterStatus::ShortRead, 0, 0};
    const int value = wire_.read();
    if (value < 0 || value > 255)
        return {RawRegisterStatus::ReadFailed, 0, 0};
    return {RawRegisterStatus::Valid, static_cast<std::uint8_t>(value), 0};
}
