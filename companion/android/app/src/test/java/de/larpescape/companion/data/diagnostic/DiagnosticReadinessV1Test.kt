package de.larpescape.companion.data.diagnostic

import de.larpescape.protocol.HardwareDiagnosticActionV1
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class DiagnosticReadinessV1Test {
    @Test
    fun everyRequiredAreaHasActionsAndConcreteBlockers() {
        val items = ReichstagsboxDiagnosticReadinessV1.items
        assertEquals(
            listOf("leds", "audio", "nfc", "matrix", "wake", "esp_now", "completion"),
            items.map { it.id },
        )
        items.forEach {
            assertTrue(it.requestedActions.isNotBlank())
            assertFalse(it.actions.isEmpty())
            assertEquals(it.actions.size, it.actions.map { action -> action.id }.distinct().size)
            assertFalse(it.blockers.isEmpty())
            assertTrue(it.blockers.all(String::isNotBlank))
        }
    }

    @Test
    fun everyRequestedActionIsTrackedIndividually() {
        val ids = ReichstagsboxDiagnosticReadinessV1.items.flatMap { it.actions }.map { it.id }.toSet()
        assertEquals(
            setOf(
                "led_select_indices", "led_select_group", "led_set_color", "led_set_brightness", "led_configure_effect", "led_start", "led_duration_expiry", "led_stop", "led_independent_off",
                "audio_named_reichstag_clip", "audio_named_star_wars_clip", "audio_direct_track", "audio_play", "audio_stop", "audio_repeat", "audio_volume", "audio_busy_status", "audio_uart_status", "audio_error_status",
                "nfc_phone_write_box_read", "nfc_phone_write_reapply", "nfc_phone_write_byte_compare", "nfc_box_write_phone_read", "nfc_explicit_write_confirmation", "nfc_box_write_reapply", "nfc_box_write_byte_compare",
                "matrix_raw_press", "matrix_raw_release", "matrix_live_highlight", "matrix_scan_error", "matrix_validate_81", "matrix_rotation", "matrix_mirror", "matrix_orientation_suggest", "matrix_orientation_confirm",
                "wake_raw_level", "wake_edge", "wake_single", "wake_combination",
                "esp_now_identified_packet", "esp_now_short_range", "esp_now_operational_range", "esp_now_receiver_report",
                "completion_three_cold_starts", "completion_endurance", "completion_open_errors", "completion_user_observations", "completion_report_export",
            ),
            ids,
        )
        assertEquals(HardwareDiagnosticActionV1.entries.map { it.wireId }.toSet(), ids)
    }

    @Test
    fun onlyImplementedDeviceIndependentDomainsAreMarkedReady() {
        val softwareOnly = ReichstagsboxDiagnosticReadinessV1.items
            .flatMap { it.actions }
            .filter { it.pathState == DiagnosticActionPathStateV1.APP_DOMAIN_READY_DEVICE_BLOCKED }
            .map { it.id }
            .toSet()
        assertEquals(
            setOf(
                "matrix_rotation", "matrix_mirror", "matrix_orientation_suggest", "matrix_orientation_confirm",
                "completion_open_errors", "completion_user_observations",
            ),
            softwareOnly,
        )
    }

    @Test
    fun onlyConfirmedLedProfileIsExposed() {
        assertTrue(ReichstagsboxDiagnosticReadinessV1.dataPinProfile.contains("GPIO15"))
        assertTrue(ReichstagsboxDiagnosticReadinessV1.alternativeDataPinProfiles.startsWith("Keine"))
        assertFalse(ReichstagsboxDiagnosticReadinessV1.dataPinProfile.contains("GPIO4"))
    }

    @Test
    fun protectedGattPermissionAndJustWorksConflictStayVisibleAndClosed() {
        val completion = ReichstagsboxDiagnosticReadinessV1.items.single { it.id == "completion" }
        assertTrue(completion.blockers.any { it.contains("MITM-geschützte Control-/Step-Writes") })
        assertTrue(completion.blockers.any { it.contains("APP_MUTUAL") && it.contains("GATT-Migration") })
        assertTrue(completion.blockers.any { it.contains("InsufficientAuthorization") })
        assertTrue(completion.actions.filter { it.id in setOf(
            "completion_three_cold_starts", "completion_endurance", "completion_report_export",
        ) }.all { it.pathState == DiagnosticActionPathStateV1.BLOCKED })
    }
}
