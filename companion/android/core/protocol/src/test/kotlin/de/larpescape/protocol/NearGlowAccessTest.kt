package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class NearGlowAccessTest {
    private val json = """{"schemaVersion":1,"protocol":"LARP-PROX-BLE/1","deviceId":"${"12".repeat(16)}","secret":"${"34".repeat(32)}","name":"Larp Näheglut","stationMac":"02:00:00:00:00:01"}"""
    @Test fun onlyExplicitSeparateStationCredentialAccepted() {
        assertEquals("02:00:00:00:00:01", NearGlowAccess.parse(json.toByteArray()).stationMac)
        for (bad in listOf(json.replace("1,", "true,"), json.replace("LARP-PROX-BLE/1", "LARP-LED/2"),
            json.replace("02:00:00:00:00:01", "FF:FF:FF:FF:FF:FF"),
            json.dropLast(1) + ",\"schemaVersion\":1}", json + "{}",
            json.replace("\"name\":\"Larp Näheglut\",", ""),
            """{"schemaVersion":1,"macA":"02:00:00:00:00:01","macB":"02:00:00:00:00:02","secret":"${"34".repeat(32)}","phase":"paired","protocol":1}""")) {
            assertThrows(Exception::class.java) { NearGlowAccess.parse(bad.toByteArray()) }
        }
        assertThrows(IllegalArgumentException::class.java) { NearGlowAccess.parse(ByteArray(2049)) }
        assertThrows(IllegalArgumentException::class.java) { NearGlowAccess.parse(byteArrayOf(0xC3.toByte(), 0x28)) }
    }
}
