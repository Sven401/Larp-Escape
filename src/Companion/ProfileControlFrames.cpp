#include "Companion/ProfileControlFrames.h"
namespace larp {
namespace companion {
namespace {
std::uint32_t get(const char *p, unsigned n) {
    std::uint32_t v = 0;
    for (unsigned i = 0; i < n; ++i)
        v |= static_cast<std::uint32_t>(static_cast<unsigned char>(p[i])) << (i * 8);
    return v;
}
void put(char *p, unsigned n, std::uint32_t v) {
    for (unsigned i = 0; i < n; ++i)
        p[i] = static_cast<char>(v >> (i * 8));
}
bool header(const char *p, std::size_t n, std::size_t expected, std::uint8_t kind) {
    return p && n == expected && static_cast<unsigned char>(p[0]) == 1 &&
           static_cast<unsigned char>(p[1]) == kind;
}
} // namespace
bool ProfileControlFrame::valid() const noexcept {
    return transfer && request && static_cast<unsigned>(operation) >= 1 &&
           static_cast<unsigned>(operation) <= 3;
}
bool ProfileResponseFrame::valid() const noexcept {
    const auto c = static_cast<unsigned>(code), d = static_cast<unsigned>(detail);
    if (!command.valid() || c < 1 || c > 13 || d > 11)
        return false;
    if (c <= 4 && d != 0)
        return false;
    if (c <= 3 && (!draftRevision || !configurationRevision))
        return false;
    if (code == ProfileResponseCode::DraftReady && command.operation != ProfileOperation::Finish)
        return false;
    if ((code == ProfileResponseCode::ApplyQueued || code == ProfileResponseCode::Saved ||
         code == ProfileResponseCode::ApplyFailed) &&
        command.operation != ProfileOperation::Apply)
        return false;
    return code != ProfileResponseCode::Aborted || command.operation == ProfileOperation::Abort;
}
bool decodeProfileControl(const char *p, std::size_t n, ProfileControlFrame &out) noexcept {
    if (!header(p, n, 12, 7) || p[5] || p[6] || p[7])
        return false;
    ProfileControlFrame v;
    v.transfer = static_cast<std::uint16_t>(get(p + 2, 2));
    v.operation = static_cast<ProfileOperation>(static_cast<unsigned char>(p[4]));
    v.request = get(p + 8, 4);
    if (!v.valid())
        return false;
    out = v;
    return true;
}
bool encodeProfileControl(const ProfileControlFrame &source, char *p, std::size_t cap,
                          std::size_t &n) noexcept {
    n = 0;
    const auto v = source;
    if (!p || cap < 12 || !v.valid())
        return false;
    p[0] = 1;
    p[1] = 7;
    put(p + 2, 2, v.transfer);
    p[4] = static_cast<char>(v.operation);
    p[5] = p[6] = p[7] = 0;
    put(p + 8, 4, v.request);
    n = 12;
    return true;
}
bool decodeProfileResponse(const char *p, std::size_t n, ProfileResponseFrame &out) noexcept {
    if (!header(p, n, 20, 8))
        return false;
    ProfileResponseFrame v;
    v.command.transfer = static_cast<std::uint16_t>(get(p + 2, 2));
    v.command.request = get(p + 4, 4);
    v.command.operation = static_cast<ProfileOperation>(static_cast<unsigned char>(p[8]));
    v.code = static_cast<ProfileResponseCode>(static_cast<unsigned char>(p[9]));
    v.detail = static_cast<ProfileResponseDetail>(get(p + 10, 2));
    v.draftRevision = get(p + 12, 4);
    v.configurationRevision = get(p + 16, 4);
    if (!v.valid())
        return false;
    out = v;
    return true;
}
bool encodeProfileResponse(const ProfileResponseFrame &source, char *p, std::size_t cap,
                           std::size_t &n) noexcept {
    n = 0;
    const auto v = source;
    if (!p || cap < 20 || !v.valid())
        return false;
    p[0] = 1;
    p[1] = 8;
    put(p + 2, 2, v.command.transfer);
    put(p + 4, 4, v.command.request);
    p[8] = static_cast<char>(v.command.operation);
    p[9] = static_cast<char>(v.code);
    put(p + 10, 2, static_cast<std::uint16_t>(v.detail));
    put(p + 12, 4, v.draftRevision);
    put(p + 16, 4, v.configurationRevision);
    n = 20;
    return true;
}
} // namespace companion
} // namespace larp
