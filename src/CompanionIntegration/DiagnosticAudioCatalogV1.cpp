#include "CompanionIntegration/DiagnosticAudioCatalogV1.h"
#include "GameDomain/GameDefinitions.h"

#include <array>

namespace larp {
namespace diagnostic {
namespace {
constexpr std::size_t ReichstagClipsPerRole = 4;

// Source: origin/Starwarscon at 540aeec1362c2d29a3f28267a0abf3b794cf8c1b,
// lib/Config/OptionDefinitions.cpp and lib/Config/OptionConfig.h.
const std::array<AudioCatalogEntryV1, 18> StarWarsClips = {{
    {AudioCatalogSourceV1::StarWarsCon, "Quail", "Button1", 101},
    {AudioCatalogSourceV1::StarWarsCon, "Quail", "Button2", 103},
    {AudioCatalogSourceV1::StarWarsCon, "Quail", "Button3", 109},
    {AudioCatalogSourceV1::StarWarsCon, "Quail", "Abschluss", 110},
    {AudioCatalogSourceV1::StarWarsCon, "SecRinsui", "Button1", 204},
    {AudioCatalogSourceV1::StarWarsCon, "SecRinsui", "Button2", 206},
    {AudioCatalogSourceV1::StarWarsCon, "SecRinsui", "Unbenannt (Quelllabel: undefined)", 207},
    {AudioCatalogSourceV1::StarWarsCon, "SecRinsui", "Abschluss", 210},
    {AudioCatalogSourceV1::StarWarsCon, "|||", "Button1", 302},
    {AudioCatalogSourceV1::StarWarsCon, "|||", "Button2", 305},
    {AudioCatalogSourceV1::StarWarsCon, "|||", "Button3", 308},
    {AudioCatalogSourceV1::StarWarsCon, "|||", "Abschluss", 310},
    {AudioCatalogSourceV1::StarWarsCon, "Spiel", "Bonus", 900},
    {AudioCatalogSourceV1::StarWarsCon, "Spiel", "Fehler 995", 995},
    {AudioCatalogSourceV1::StarWarsCon, "Spiel", "Fehler 996", 996},
    {AudioCatalogSourceV1::StarWarsCon, "Spiel", "Fehler 997", 997},
    {AudioCatalogSourceV1::StarWarsCon, "Spiel", "Fehler 998", 998},
    {AudioCatalogSourceV1::StarWarsCon, "Spiel", "Fehler 999", 999},
}};
} // namespace

std::size_t audioCatalogCountV1() noexcept {
    return domain::defaultRoleDefinitions().size() * ReichstagClipsPerRole + StarWarsClips.size();
}

bool audioCatalogEntryV1(std::size_t index, AudioCatalogEntryV1 &out) noexcept {
    const auto &roles = domain::defaultRoleDefinitions();
    const std::size_t reichstagClips = roles.size() * ReichstagClipsPerRole;
    if (index < reichstagClips) {
        const auto &role = roles[index / ReichstagClipsPerRole];
        const std::size_t offset = index % ReichstagClipsPerRole;
        out = offset == 0
                  ? AudioCatalogEntryV1{AudioCatalogSourceV1::Reichstagsgame, role.name,
                                        "Rollenabschluss", role.audioTrack}
                  : AudioCatalogEntryV1{AudioCatalogSourceV1::Reichstagsgame, role.name,
                                        role.crystals[offset - 1].symbol,
                                        role.crystals[offset - 1].audioTrack};
        return true;
    }
    index -= reichstagClips;
    if (index >= StarWarsClips.size()) return false;
    out = StarWarsClips[index];
    return true;
}

bool diagnosticDirectTrackInRangeV1(std::uint16_t track) noexcept {
    return track >= 1 && track <= 999;
}

} // namespace diagnostic
} // namespace larp
