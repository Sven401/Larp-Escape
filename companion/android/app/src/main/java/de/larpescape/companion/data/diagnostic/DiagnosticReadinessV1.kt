package de.larpescape.companion.data.diagnostic

/**
 * Product-specific readiness snapshot for the Reichstagsbox diagnostic plan.
 *
 * This is deliberately not a capability advertisement. Every entry remains
 * blocked until firmware reports a current authority, component ownership and
 * the component-specific evidence named here. Keeping the list in code makes
 * omissions visible in tests without inventing an executable device path.
 */
data class DiagnosticReadinessItemV1(
    val id: String,
    val title: String,
    val requestedActions: String,
    val actions: List<DiagnosticActionReadinessV1>,
    val blockers: List<String>,
)

enum class DiagnosticActionPathStateV1 {
    BLOCKED,
    APP_DOMAIN_READY_DEVICE_BLOCKED,
}

data class DiagnosticActionReadinessV1(
    val id: String,
    val label: String,
    val pathState: DiagnosticActionPathStateV1 = DiagnosticActionPathStateV1.BLOCKED,
)

private fun blockedActions(vararg actions: Pair<String, String>) =
    actions.map { (id, label) -> DiagnosticActionReadinessV1(id, label) }

object ReichstagsboxDiagnosticReadinessV1 {
    const val dataPinProfile = "GPIO15 · as-built vom Nutzer bestätigt · Funktion offen"
    const val alternativeDataPinProfiles = "Keine elektrisch belegte konfliktfreie Alternative"

    val items = listOf(
        DiagnosticReadinessItemV1(
            id = "leds",
            title = "LEDs · 23 Indizes",
            requestedActions = "Einzel/Gruppe, RGB, Helligkeit, Twinkle, Burst, Ripple, BrightnessRamp, Palette, Start, Dauer, Stop",
            actions = blockedActions(
                "led_select_indices" to "Einzelne der 23 LEDs wählen",
                "led_select_group" to "LED-Gruppe wählen",
                "led_set_color" to "RGB-Farbe setzen",
                "led_set_brightness" to "Helligkeit begrenzen",
                "led_configure_effect" to "Effekt und gültige Parameter wählen",
                "led_start" to "Effekt starten",
                "led_duration_expiry" to "Begrenzte Dauer ablaufen lassen",
                "led_stop" to "Effekt stoppen",
                "led_independent_off" to "Unabhängiges All-off belegen",
            ),
            blockers = listOf(
                "Kein produktiver Diagnose-Grant oder Komponentenbesitz",
                "Kein elektrischer oder sichtbarer Ausnachweis trotz Software-Timer",
                "LED-Typ, Pegel und Versorgungsreserve sind unbestätigt",
                "GPIO4 ist durch PN532-SS belegt und keine Alternative",
            ),
        ),
        DiagnosticReadinessItemV1(
            id = "audio",
            title = "Audio / SD / DFPlayer",
            requestedActions = "Benannte Clips, begrenzte Tracknummer, Play, Stop, Wiederholung, Lautstärke, BUSY/UART/Fehler",
            actions = blockedActions(
                "audio_named_reichstag_clip" to "Benannten Reichstagsgame-Clip wählen",
                "audio_named_star_wars_clip" to "Benannten Star-Wars-Con-Clip wählen",
                "audio_direct_track" to "Begrenzte direkte Tracknummer wählen",
                "audio_play" to "Wiedergabe starten",
                "audio_stop" to "Wiedergabe stoppen",
                "audio_repeat" to "Wiederholung setzen",
                "audio_volume" to "Lautstärke setzen",
                "audio_busy_status" to "BUSY getrennt auswerten",
                "audio_uart_status" to "UART-Antwort getrennt auswerten",
                "audio_error_status" to "Fehlerantwort getrennt auswerten",
            ),
            blockers = listOf(
                "Kein produktiver Diagnose-Grant oder DFPlayer-Besitz",
                "UART-, BUSY- und Wake-Verdrahtung sind am As-built nicht bestätigt",
                "SD-Inhalt und hörbare Ausgabe sind nicht belegt",
            ),
        ),
        DiagnosticReadinessItemV1(
            id = "nfc",
            title = "NFC · beide Richtungen",
            requestedActions = "Handy schreibt → Box liest; Box schreibt → Handy liest; Entfernen/Neuauflegen und Bytevergleich",
            actions = blockedActions(
                "nfc_phone_write_box_read" to "Handy schreibt, Box liest unabhängig",
                "nfc_phone_write_reapply" to "Tag entfernen und neu auflegen",
                "nfc_phone_write_byte_compare" to "Box-Lesedaten bytegenau vergleichen",
                "nfc_box_write_phone_read" to "Box schreibt, Handy liest unabhängig",
                "nfc_explicit_write_confirmation" to "Box-Schreibbestätigung prüfen",
                "nfc_box_write_reapply" to "Tag erneut entfernen und neu auflegen",
                "nfc_box_write_byte_compare" to "Handy-Lesedaten bytegenau vergleichen",
            ),
            blockers = listOf(
                "PN532 startet an der Originalbox nicht wiederholt stabil",
                "Produktiver begrenzter PN532-Rohport und Tag-Schreibschutz fehlen",
                "NFC-Spielleiter-Authentisierung und Test-Tag-Vertrag sind noch getrennt offen",
            ),
        ),
        DiagnosticReadinessItemV1(
            id = "matrix",
            title = "9×9-Keymatrix",
            requestedActions = "Rohes Drücken/Loslassen, Live-Matrix, 81 Punkte, Drehung, Spiegelung, bestätigte Orientierung",
            actions = blockedActions(
                "matrix_raw_press" to "Rohes Druckereignis anzeigen",
                "matrix_raw_release" to "Rohes Loslassereignis anzeigen",
                "matrix_live_highlight" to "Live-Zelle hervorheben",
                "matrix_scan_error" to "Scanfehler getrennt anzeigen",
                "matrix_validate_81" to "Alle 81 Punkte abgleichen",
            ) + listOf(
                DiagnosticActionReadinessV1("matrix_rotation", "Anzeige manuell drehen", DiagnosticActionPathStateV1.APP_DOMAIN_READY_DEVICE_BLOCKED),
                DiagnosticActionReadinessV1("matrix_mirror", "Anzeige manuell spiegeln", DiagnosticActionPathStateV1.APP_DOMAIN_READY_DEVICE_BLOCKED),
                DiagnosticActionReadinessV1("matrix_orientation_suggest", "Orientierung aus mehreren bestätigten Punkten vorschlagen", DiagnosticActionPathStateV1.APP_DOMAIN_READY_DEVICE_BLOCKED),
                DiagnosticActionReadinessV1("matrix_orientation_confirm", "Orientierung erst nach Nutzerbestätigung zur Speicherung freigeben", DiagnosticActionPathStateV1.APP_DOMAIN_READY_DEVICE_BLOCKED),
            ),
            blockers = listOf(
                "Matrixreihe 1 liegt laut Quellprofil auf MCP23017 GPB7; dieser Pin ist output-only",
                "Boot-/Ruhe-Pinrichtungen widersprechen dem Scanprofil",
                "Kabelbaum, Rohzuordnung und Scanfehlerverhalten sind physisch unbestätigt",
            ),
        ),
        DiagnosticReadinessItemV1(
            id = "wake",
            title = "Vier Wake-Eingänge",
            requestedActions = "Pegel und Flanken live; einzeln und in Kombination prüfen",
            actions = blockedActions(
                "wake_raw_level" to "Rohpegel je Eingang anzeigen",
                "wake_edge" to "Flanke je Eingang anzeigen",
                "wake_single" to "Jeden Eingang einzeln prüfen",
                "wake_combination" to "Eingänge in Kombination prüfen",
            ),
            blockers = listOf(
                "Wake 1 liegt laut Quellprofil auf MCP23017 GPB7; dieser Pin ist output-only",
                "HIGH-aktive Interpretation, Pull-up-Ruhepegel, Nummerierung und Kabelbaum sind unbestätigt",
            ),
        ),
        DiagnosticReadinessItemV1(
            id = "esp_now",
            title = "ESP-NOW / Empfänger",
            requestedActions = "Pakete mit Lauf-/Paketkennung auf kurze Strecke und Einsatzentfernung senden",
            actions = blockedActions(
                "esp_now_identified_packet" to "Paket mit Lauf- und Paketkennung senden",
                "esp_now_short_range" to "Kurze Strecke prüfen",
                "esp_now_operational_range" to "Einsatzentfernung prüfen",
                "esp_now_receiver_report" to "Korreliertes Empfängerprotokoll abgleichen",
            ),
            blockers = listOf(
                "Kein produktiver Diagnose-Grant oder Funkbesitz",
                "Kein dauerhaftes, korrelierbares Empfängerprotokoll",
                "Kanal 0, Wi-Fi/BLE-Koexistenz und Empfängerkanal sind ungeprüft",
            ),
        ),
        DiagnosticReadinessItemV1(
            id = "completion",
            title = "Abschlusslauf",
            requestedActions = "Drei Kaltstarts, längerer gemeinsamer Lauf, offene Fehler, Beobachtungen und Endbericht",
            actions = blockedActions(
                "completion_three_cold_starts" to "Drei Kaltstarts ausführen",
                "completion_endurance" to "Längeren gemeinsamen Lauf ausführen",
            ) + listOf(
                DiagnosticActionReadinessV1("completion_open_errors", "Offene Fehler erhalten", DiagnosticActionPathStateV1.APP_DOMAIN_READY_DEVICE_BLOCKED),
                DiagnosticActionReadinessV1("completion_user_observations", "Nutzerbeobachtungen getrennt erfassen", DiagnosticActionPathStateV1.APP_DOMAIN_READY_DEVICE_BLOCKED),
                DiagnosticActionReadinessV1("completion_report_export", "Vollständigen Endbericht exportieren"),
            ),
            blockers = listOf(
                "Box-BLE nutzt IO_CAP_NONE/Just Works; MITM-geschützte Control-/Step-Writes sind damit nicht erreichbar",
                "APP_MUTUAL über Just Works braucht eine geprüfte Auth-/GATT-Migration; NFC-Ziel ersetzt BLE-MITM nicht",
                "Keine produktive Spielleiterrolle; Control-/Step-Writes bleiben mit InsufficientAuthorization geschlossen",
                "Der vollständige Lauf wartet auf die Hardwarekorrekturen und alle Komponentenpfade",
            ),
        ),
    )
}
