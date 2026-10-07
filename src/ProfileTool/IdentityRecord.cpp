#include "IdentityRecord.h"
#include "Companion/DeviceIdentityStore.h"
#include <cstdio>
#include <cstring>
namespace {
int hex(char value) {
    return value >= '0' && value <= '9'   ? value - '0'
           : value >= 'A' && value <= 'F' ? value - 'A' + 10
                                          : -1;
}
} // namespace
int runDeviceIdentityRecord(const char *recordHex) {
    using namespace larp::companion;
    DeviceIdentityRecord record;
    if (!recordHex || std::strlen(recordHex) != 2 * record.bytes.size())
        return 2;
    for (unsigned i = 0; i < record.bytes.size(); ++i) {
        const int high = hex(recordHex[2 * i]), low = hex(recordHex[2 * i + 1]);
        if (high < 0 || low < 0)
            return 2;
        record.bytes[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
    Identity identity;
    if (!decodeDeviceIdentityRecord(record, identity))
        return 1;
    char output[maxDeviceIdentityJsonBytes];
    std::size_t count = 0;
    if (encodeDeviceIdentity(identity, output, sizeof(output), count) !=
        ConfiguredEncodeResult::Success)
        return 1;
    return std::fwrite(output, 1, count, stdout) == count && std::fputc(10, stdout) != EOF &&
                   std::fflush(stdout) == 0
               ? 0
               : 1;
}
