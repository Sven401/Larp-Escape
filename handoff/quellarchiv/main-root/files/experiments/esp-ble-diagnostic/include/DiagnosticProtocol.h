#pragma once
#include <cstddef>
#include <cstdint>
namespace diagnostic {
inline void put32(std::uint8_t *p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
inline std::uint32_t get32(const std::uint8_t *p) {
    std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= std::uint32_t(p[i]) << (8 * i);
    return v;
}
struct Session {
    std::uint32_t boot = 0, processed = 0, previous = 0;
    void clearConnection() { previous = 0; }
    void receive(const std::uint8_t *p, std::size_t n, std::uint8_t (&reply)[16]) {
        const auto id = n >= 6 ? get32(p + 2) : 0;
        unsigned reason = 0;
        if (n != 8 || p[0] != 1 || p[6] || p[7] || !id) reason = 2;
        else if (id <= previous) reason = 3;
        else if (processed == UINT32_MAX) reason = 4;
        else { previous = id; ++processed; if (p[1] != 1) reason = 1; }
        reply[0] = 1; reply[1] = reason ? 2 : 1;
        put32(reply + 2, id); put32(reply + 6, boot); put32(reply + 10, processed);
        reply[14] = static_cast<std::uint8_t>(reason); reply[15] = 0;
    }
};
} // namespace diagnostic
