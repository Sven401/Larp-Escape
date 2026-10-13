#include "Companion/PeerRegistry.h"
#include <algorithm>
#include <cstring>
namespace larp {
namespace companion {
namespace {
void registryPut32(std::uint8_t *p, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::uint32_t registryGet32(const std::uint8_t *p) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= std::uint32_t(p[i]) << (8 * i);
    return value;
}
std::uint32_t registryCrc(const PeerRegistryRecord &value) {
    std::uint32_t crc = 0xffffffffu;
    for (unsigned i = 0; i < 400; ++i)
        crc = profileSourceCrcByte(crc, value.bytes[i]);
    return crc ^ 0xffffffffu;
}
} // namespace
bool validPeerRegistry(const PeerRegistrySnapshot &value) noexcept {
    if (!value.device.valid() || value.count > PeerRegistryLimits::Peers ||
        (!value.generation && value.count))
        return false;
    for (unsigned i = 0; i < value.peers.size(); ++i) {
        const auto &entry = value.peers[i];
        if (i >= value.count) {
            if (!(entry == PeerRegistryEntry{}))
                return false;
            continue;
        }
        if (!entry.credential.peer.valid() || !entry.credential.generation ||
            entry.credential.key[0] != 4 ||
            (entry.gameMaster ? !entry.principal : entry.principal != 0))
            return false;
        if (i && !std::lexicographical_compare(value.peers[i - 1].credential.peer.bytes.begin(),
                                               value.peers[i - 1].credential.peer.bytes.end(),
                                               entry.credential.peer.bytes.begin(),
                                               entry.credential.peer.bytes.end()))
            return false;
        for (unsigned j = 0; j < i; ++j)
            if (entry.credential.key == value.peers[j].credential.key ||
                (entry.principal && entry.principal == value.peers[j].principal))
                return false;
    }
    return true;
}
bool encodePeerRegistry(const PeerRegistrySnapshot &caller,
                        PeerRegistryRecord &destination) noexcept {
    const auto selected = caller;
    if (!selected.generation || !validPeerRegistry(selected))
        return false;
    PeerRegistryRecord result;
    result.bytes[0] = 'L';
    result.bytes[1] = 'P';
    result.bytes[2] = 'R';
    result.bytes[3] = 'G';
    result.bytes[4] = 1;
    result.bytes[5] = selected.count;
    registryPut32(result.bytes.data() + 8, selected.generation);
    std::memcpy(result.bytes.data() + 12, selected.device.bytes.data(), 16);
    for (unsigned i = 0; i < selected.count; ++i) {
        const auto &entry = selected.peers[i];
        auto *p = result.bytes.data() + 32 + i * 92;
        std::memcpy(p, entry.credential.peer.bytes.data(), 16);
        std::memcpy(p + 16, entry.credential.key.data(), 65);
        registryPut32(p + 81, entry.credential.generation);
        registryPut32(p + 85, entry.principal);
        p[89] = entry.gameMaster ? 1 : 0;
    }
    registryPut32(result.bytes.data() + 400, registryCrc(result));
    destination = result;
    return true;
}
bool decodePeerRegistry(const PeerRegistryRecord &caller,
                        PeerRegistrySnapshot &destination) noexcept {
    const auto record = caller;
    const auto *p = record.bytes.data();
    if (p[0] != 'L' || p[1] != 'P' || p[2] != 'R' || p[3] != 'G' || p[4] != 1 || p[5] > 4 || p[6] ||
        p[7] || p[28] || p[29] || p[30] || p[31] || registryGet32(p + 400) != registryCrc(record))
        return false;
    PeerRegistrySnapshot result;
    result.generation = registryGet32(p + 8);
    result.count = p[5];
    std::memcpy(result.device.bytes.data(), p + 12, 16);
    if (!result.generation)
        return false;
    for (unsigned i = 0; i < 4; ++i) {
        const auto *entry = p + 32 + i * 92;
        if (i >= result.count) {
            for (unsigned at = 0; at < 92; ++at)
                if (entry[at])
                    return false;
            continue;
        }
        if (entry[89] > 1 || entry[90] || entry[91])
            return false;
        auto &out = result.peers[i];
        std::memcpy(out.credential.peer.bytes.data(), entry, 16);
        std::memcpy(out.credential.key.data(), entry + 16, 65);
        out.credential.generation = registryGet32(entry + 81);
        out.principal = registryGet32(entry + 85);
        out.gameMaster = entry[89] != 0;
    }
    if (!validPeerRegistry(result))
        return false;
    destination = result;
    return true;
}
} // namespace companion
} // namespace larp
