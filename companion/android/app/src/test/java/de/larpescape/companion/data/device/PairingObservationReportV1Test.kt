package de.larpescape.companion.data.device

import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test

class PairingObservationReportV1Test {
    @Test fun exportKeepsAndroidObservationsSeparateFromMissingFirmwareProof() {
        val report = PairingObservationReportV1.start("2026-10-11T10:00:00Z", "attempt-1")
            .observe("2026-10-11T10:00:01Z", "ANDROID_BLUETOOTH_GATT_CALLBACK", 17,
                "AA:BB:CC:DD:EE:FF", "connection_state", "status=0;state=2")
            .observe("2026-10-11T10:00:02Z", "ANDROID_BOND_STATE_BROADCAST", 17,
                "AA:BB:CC:DD:EE:FF", "bond_state", "OS_BONDED")
        val json = JsonParser.parseString(String(report.export("2026-10-11T10:00:03Z"))).asJsonObject
        assertEquals("ble_pairing_observation_v1", json["schema"].asString)
        assertEquals("attempt-1", json["reportId"].asString)
        assertEquals("2026-10-11T10:00:03Z", json["exportedUtc"].asString)
        val events = json.getAsJsonArray("events")
        assertEquals(2, events.size())
        assertEquals(1, events[0].asJsonObject["sequence"].asInt)
        assertEquals(2, events[1].asJsonObject["sequence"].asInt)
        assertEquals(17, events[1].asJsonObject["connectionGeneration"].asInt)
        assertEquals("OS_BONDED", events[1].asJsonObject["value"].asString)
        val missing = json.getAsJsonArray("missingFirmwareEvidence").map { it.asString }
        assertTrue(missing.any { it.contains("MITM") })
        assertTrue(missing.any { it.contains("role") })
        assertFalse(json.has("deviceGrant"))
        assertFalse(json.has("mitmConfirmed"))
    }

    @Test fun emptyReportDoesNotBecomePairingSuccess() {
        val json = JsonParser.parseString(String(PairingObservationReportV1.start("2026-10-11T10:00:00Z", "attempt-2")
            .export("2026-10-11T10:00:01Z"))).asJsonObject
        assertEquals(0, json.getAsJsonArray("events").size())
        assertFalse(json.has("success"))
        assertEquals(4, json.getAsJsonArray("missingFirmwareEvidence").size())
        assertEquals("NO_ATTEMPT", json.getAsJsonObject("androidAssessment")["code"].asString)
    }

    private fun observed() = PairingObservationReportV1.start("2026-10-11T10:00:00Z", "attempt-3")
        .observe("t1", "ANDROID_APP_USER_ACTION", 4, "AA", "connection_attempt", "connectGatt_requested")
        .observe("t2", "ANDROID_BLUETOOTH_GATT_CALLBACK", 4, "AA", "connection_state", "status=0;state=2")
        .observe("t3", "ANDROID_BLUETOOTH_GATT_SERVICE_TABLE", 4, "AA", "product_service_present", "true")
        .observe("t4", "ANDROID_BOND_STATE_BROADCAST", 4, "AA", "bond_state", "OS_BONDED")

    @Test fun currentConnectionServiceAndOsBondProduceOnlyAndroidAssessment() {
        val report = observed()
        val assessment = report.androidAssessment()
        assertEquals("ANDROID_PAIRING_OBSERVED", assessment.code)
        assertEquals(listOf(1, 2, 3, 4), assessment.evidenceSequences)
        assertEquals("OPEN", assessment.firmwareSecurityEvidence)
        val json = JsonParser.parseString(String(report.export("t5"))).asJsonObject
        assertEquals("ANDROID_PAIRING_OBSERVED", json.getAsJsonObject("androidAssessment")["code"].asString)
        assertEquals(4, json.getAsJsonObject("androidAssessment").getAsJsonArray("evidenceSequences").size())
    }

    @Test fun disconnectRebondAndNewGenerationInvalidatePreviousSuccess() {
        val base = observed()
        assertEquals("NOT_CONNECTED", base.observe("t5", "ANDROID_APP_OR_GATT", 4, "AA",
            "connection_closed", "local_session_ended").androidAssessment().code)
        assertEquals("BOND_OPEN", base.observe("t5", "ANDROID_BLUETOOTH_DEVICE_CREATE_BOND", 4, "AA",
            "bond_request", "OS_REQUEST_ACCEPTED").androidAssessment().code)
        val next = base.observe("t5", "ANDROID_APP_USER_ACTION", 6, "AA", "connection_attempt", "connectGatt_requested")
        assertEquals("NOT_CONNECTED", next.androidAssessment().code)
        assertEquals("SERVICE_OPEN", next.observe("t6", "ANDROID_BLUETOOTH_GATT_CALLBACK", 6, "AA",
            "connection_state", "status=0;state=2").androidAssessment().code)
        assertEquals("BOND_OPEN", base.observe("t5", "ANDROID_BOND_STATE_BROADCAST", 4, "AA",
            "bond_state", "OS_BONDING").androidAssessment().code)
    }

    @Test fun redundantOsBondDoesNotIssueNewRequestOrDowngradeAssessment() {
        assertEquals(BondRequestDecisionV1.ALREADY_BONDED, bondRequestDecisionV1("OS_BONDED"))
        assertEquals(BondRequestDecisionV1.IN_PROGRESS, bondRequestDecisionV1("OS_BONDING"))
        assertEquals(BondRequestDecisionV1.REQUEST, bondRequestDecisionV1("OS_NOT_BONDED"))
        val afterRefresh = observed().observe("t5", "ANDROID_BLUETOOTH_DEVICE_GET_BOND_STATE", 4, "AA",
            "bond_state", "OS_BONDED")
        assertEquals("ANDROID_PAIRING_OBSERVED", afterRefresh.androidAssessment().code)
        assertEquals("NOT_CONNECTED", afterRefresh.observe("t6", "ANDROID_APP_OR_GATT", 4, "AA",
            "connection_closed", "local_session_ended").androidAssessment().code)
    }

    @Test fun journalCapClosesAssessmentAndCannotGrow() {
        var report = observed()
        repeat(PairingObservationReportV1.MAX_EVENTS) {
            report = report.observe("t${it + 5}", "ANDROID_APP", 4, "AA", "scan_started", "requested")
        }
        assertEquals(PairingObservationReportV1.MAX_EVENTS, report.events.size)
        assertEquals("journal_limit_reached;start_new_report", report.events.last().value)
        assertEquals("NOT_CONNECTED", report.androidAssessment().code)
    }
}
