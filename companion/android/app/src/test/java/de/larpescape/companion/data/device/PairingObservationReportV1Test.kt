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
    }
}
