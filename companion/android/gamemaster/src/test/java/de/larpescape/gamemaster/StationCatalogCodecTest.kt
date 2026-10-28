package de.larpescape.gamemaster

import de.larpescape.gamemaster.data.*
import de.larpescape.gamemaster.station.Station
import org.junit.Assert.*
import org.junit.Test

class StationCatalogCodecTest {
    private val device = "11".repeat(16)
    private val entry = StationEntry(Station("one","Veranstaltung","Altes Tor","AA:BB:CC:DD:EE:01",device),
        ("{\"schemaVersion\":1,\"deviceId\":\"$device\",\"secret\":\"" + "22".repeat(32) + "\"}").toByteArray())
    @Test fun roundTripPreservesStationAndImportedCredentialBytes() {
        val original = entry.bytes.copyOf()
        val encoded = StationCatalogCodec.encode(listOf(entry)); val decoded = StationCatalogCodec.decode(encoded)
        assertEquals(entry.station,decoded.single().station); assertArrayEquals(original,decoded.single().bytes)
        assertArrayEquals(original,entry.bytes)
    }
    @Test fun rejectsTrailingBytesTruncationUnknownVersionAndCountOverflow() {
        val encoded = StationCatalogCodec.encode(listOf(entry))
        val variants = listOf(encoded + byteArrayOf(0),encoded.copyOf(encoded.size - 1),encoded.copyOf().also { it[3] = 2 },encoded.copyOf().also { it[7] = 33 })
        for (bad in variants) {
            try { StationCatalogCodec.decode(bad); fail("invalid catalog accepted") } catch (_: Exception) {}
        }
    }
    @Test fun preventsCredentialFromBeingStoredUnderAnotherDeviceIdentity() {
        try { StationCatalogCodec.encode(listOf(entry.copy(station = entry.station.copy(deviceId = "33".repeat(16))))); fail("wrong identity accepted") } catch (_: IllegalArgumentException) {}
    }
    @Test fun duplicateMappingsCannotBeEncoded() {
        try { StationCatalogCodec.encode(listOf(entry,entry.copy(station = entry.station.copy(id = "two",name = "Turm")))) ; fail("duplicate accepted") } catch (_: IllegalArgumentException) {}
    }
    @Test fun emptyCatalogIsAValidIndependentInstallation() {
        assertTrue(StationCatalogCodec.decode(StationCatalogCodec.encode(emptyList())).isEmpty())
    }
}
