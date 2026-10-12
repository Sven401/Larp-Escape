#include "CompanionIntegration/DiagnosticRawExpanderProbe.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

using namespace larp::diagnostic;

struct Reader final : IRawRegisterReader {
    RawRegisterRead next;
    unsigned calls = 0;
    std::uint8_t address = 0, reg = 0;
    RawRegisterRead read(std::uint8_t requestedAddress,
                         std::uint8_t requestedRegister) noexcept override {
        ++calls;
        address = requestedAddress;
        reg = requestedRegister;
        return next;
    }
};

int main() {
    Reader reader;
    RawExpanderProbe probe(reader);
    reader.next = {RawRegisterStatus::Valid, 0x40, 0};
    auto high = probe.sample();
    assert(reader.calls == 1 && reader.address == 0x20 && reader.reg == 0x13);
    assert(high.readable() && high.rawHigh && high.rawByte == 0x40);
    assert(high.bitIndex == 6 && !high.physicalChannelQualified);

    reader.next = {RawRegisterStatus::Valid, 0xbf, 0};
    auto low = probe.sample();
    assert(low.readable() && !low.rawHigh && low.rawByte == 0xbf);
    assert(!low.physicalChannelQualified);

    reader.next = {RawRegisterStatus::RegisterSelectFailed, 0xff, 2};
    auto failed = probe.sample();
    assert(!failed.readable() && failed.busCode == 2 && !failed.rawHigh);
    assert(failed.rawByte == 0 && !failed.physicalChannelQualified);

    reader.next = {RawRegisterStatus::ShortRead, 0xff, 0};
    auto shortRead = probe.sample();
    assert(!shortRead.readable() && shortRead.rawByte == 0 && !shortRead.rawHigh);

    reader.next = {RawRegisterStatus::ReadFailed, 0xff, 0};
    auto readFailed = probe.sample();
    assert(!readFailed.readable() && readFailed.rawByte == 0 && !readFailed.rawHigh);
    std::puts("5 raw expander probe cases passed");
}
