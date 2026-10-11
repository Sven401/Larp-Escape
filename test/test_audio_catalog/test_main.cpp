#include "CompanionIntegration/DiagnosticAudioCatalogV1.h"
#include "GameDomain/GameDefinitions.h"

#include <unity.h>

using larp::diagnostic::AudioCatalogEntryV1;
using larp::diagnostic::AudioCatalogSourceV1;
using larp::diagnostic::audioCatalogCountV1;
using larp::diagnostic::audioCatalogEntryV1;
using larp::diagnostic::diagnosticDirectTrackInRangeV1;

void setUp() {}
void tearDown() {}

void test_reichstag_catalogue_tracks_live_role_definitions() {
    const auto &roles = larp::domain::defaultRoleDefinitions();
    for (std::size_t roleIndex = 0; roleIndex < roles.size(); ++roleIndex) {
        AudioCatalogEntryV1 entry = {};
        const auto &role = roles[roleIndex];
        TEST_ASSERT_TRUE(audioCatalogEntryV1(roleIndex * 4, entry));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(AudioCatalogSourceV1::Reichstagsgame),
                              static_cast<int>(entry.source));
        TEST_ASSERT_EQUAL_STRING(role.name, entry.group);
        TEST_ASSERT_EQUAL_STRING("Rollenabschluss", entry.clip);
        TEST_ASSERT_EQUAL_UINT16(role.audioTrack, entry.track);
        for (std::size_t crystal = 0; crystal < role.crystals.size(); ++crystal) {
            TEST_ASSERT_TRUE(audioCatalogEntryV1(roleIndex * 4 + crystal + 1, entry));
            TEST_ASSERT_EQUAL_STRING(role.name, entry.group);
            TEST_ASSERT_EQUAL_STRING(role.crystals[crystal].symbol, entry.clip);
            TEST_ASSERT_EQUAL_UINT16(role.crystals[crystal].audioTrack, entry.track);
        }
    }
}

void test_star_wars_catalogue_matches_source_tracks_and_keeps_provenance() {
    const char *groups[] = {"Quail", "Quail", "Quail", "Quail",
                            "SecRinsui", "SecRinsui", "SecRinsui", "SecRinsui",
                            "|||", "|||", "|||", "|||",
                            "Spiel", "Spiel", "Spiel", "Spiel", "Spiel", "Spiel"};
    const char *clips[] = {"Button1", "Button2", "Button3", "Abschluss",
                           "Button1", "Button2", "Unbenannt (Quelllabel: undefined)", "Abschluss",
                           "Button1", "Button2", "Button3", "Abschluss",
                           "Bonus", "Fehler 995", "Fehler 996", "Fehler 997",
                           "Fehler 998", "Fehler 999"};
    const std::uint16_t expected[] = {101, 103, 109, 110, 204, 206, 207, 210,
                                      302, 305, 308, 310, 900, 995, 996, 997, 998, 999};
    const std::size_t reichstagCount = larp::domain::defaultRoleDefinitions().size() * 4;
    TEST_ASSERT_EQUAL_UINT(reichstagCount + sizeof(expected) / sizeof(expected[0]),
                           audioCatalogCountV1());
    for (std::size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        AudioCatalogEntryV1 entry = {};
        TEST_ASSERT_TRUE(audioCatalogEntryV1(reichstagCount + i, entry));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(AudioCatalogSourceV1::StarWarsCon),
                              static_cast<int>(entry.source));
        TEST_ASSERT_EQUAL_STRING(groups[i], entry.group);
        TEST_ASSERT_EQUAL_STRING(clips[i], entry.clip);
        TEST_ASSERT_EQUAL_UINT16(expected[i], entry.track);
    }
    AudioCatalogEntryV1 reichstag = {}, starWars = {};
    TEST_ASSERT_TRUE(audioCatalogEntryV1(14, reichstag)); // Abenteurerin / Grün: 101
    TEST_ASSERT_TRUE(audioCatalogEntryV1(reichstagCount, starWars));
    TEST_ASSERT_EQUAL_UINT16(101, reichstag.track);
    TEST_ASSERT_EQUAL_UINT16(reichstag.track, starWars.track);
    TEST_ASSERT_TRUE(reichstag.source != starWars.source);
    AudioCatalogEntryV1 unnamed = {};
    TEST_ASSERT_TRUE(audioCatalogEntryV1(reichstagCount + 6, unnamed));
    TEST_ASSERT_EQUAL_UINT16(207, unnamed.track);
    TEST_ASSERT_EQUAL_STRING("Unbenannt (Quelllabel: undefined)", unnamed.clip);
}

void test_out_of_range_lookup_preserves_destination_and_direct_range_is_bounded() {
    AudioCatalogEntryV1 entry = {AudioCatalogSourceV1::Reichstagsgame, "sentinel", "sentinel", 42};
    TEST_ASSERT_FALSE(audioCatalogEntryV1(audioCatalogCountV1(), entry));
    TEST_ASSERT_EQUAL_STRING("sentinel", entry.group);
    TEST_ASSERT_EQUAL_UINT16(42, entry.track);
    TEST_ASSERT_FALSE(diagnosticDirectTrackInRangeV1(0));
    TEST_ASSERT_TRUE(diagnosticDirectTrackInRangeV1(1));
    TEST_ASSERT_TRUE(diagnosticDirectTrackInRangeV1(999));
    TEST_ASSERT_FALSE(diagnosticDirectTrackInRangeV1(1000));
    TEST_ASSERT_FALSE(diagnosticDirectTrackInRangeV1(65535));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_reichstag_catalogue_tracks_live_role_definitions);
    RUN_TEST(test_star_wars_catalogue_matches_source_tracks_and_keeps_provenance);
    RUN_TEST(test_out_of_range_lookup_preserves_destination_and_direct_range_is_bounded);
    return UNITY_END();
}
