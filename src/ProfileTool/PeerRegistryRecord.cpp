#include "PeerRegistryRecord.h"
#include "Companion/PeerRegistry.h"
#include <cstdio>
#include <cstring>
namespace {
int hex(char value) {
    return value >= '0' && value <= '9'   ? value - '0'
           : value >= 'A' && value <= 'F' ? value - 'A' + 10
                                          : -1;
}
} // namespace
int runPeerRegistryRecord(const char *text) {
    using namespace larp::companion;
    PeerRegistryRecord record;
    if (!text || std::strlen(text) != 2 * record.bytes.size())
        return 2;
    for (unsigned i = 0; i < record.bytes.size(); ++i) {
        const int high = hex(text[2 * i]), low = hex(text[2 * i + 1]);
        if (high < 0 || low < 0)
            return 2;
        record.bytes[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
    PeerRegistrySnapshot snapshot;
    if (!decodePeerRegistry(record, snapshot))
        return 1;
    char device[33];
    static const char digits[] = "0123456789ABCDEF";
    for (unsigned i = 0; i < 16; ++i) {
        device[2 * i] = digits[snapshot.device.bytes[i] >> 4];
        device[2 * i + 1] = digits[snapshot.device.bytes[i] & 15];
    }
    device[32] = 0;
    unsigned roles = 0;
    for (unsigned i = 0; i < snapshot.count; ++i)
        if (snapshot.peers[i].gameMaster)
            ++roles;
    return std::printf("{\"scope\":\"inspect_public_peer_registry\",\"device\":\"%s\","
                       "\"generation\":%lu,\"peers\":%u,\"gameMasters\":%u,\"recordBytes\":404,"
                       "\"cryptoValidated\":false,\"stored\":false}\n",
                       device, static_cast<unsigned long>(snapshot.generation),
                       unsigned(snapshot.count), roles) > 0 &&
                   std::fflush(stdout) == 0
               ? 0
               : 1;
}
