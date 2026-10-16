package de.larpescape.companion.ui.nfc

import de.larpescape.protocol.*
import de.larpescape.companion.data.device.BoxNfcUiState
import org.junit.Assert.*
import org.junit.Test

class BoxNfcTest {
    private val uid = byteArrayOf(4, 1, 2, 3, 4, 5, 6)
    private val written = BoxNfcResult(2, 3, 2, 8, 2, uid, NfcTestFixture.changed)
    private val read = NfcUiState(listening = true, tagId = written.uidText,
        data = NfcTestFixture.changed, selectionId = 12, readOnlySelectionId = 12)
    @Test fun onlyNewIndependentReadConfirmsRoundtrip() {
        assertTrue(boxRoundtripConfirmed(written, 11, read))
        assertFalse(boxRoundtripConfirmed(written, 12, read))
        assertFalse(boxRoundtripConfirmed(written, 11, read.copy(readOnlySelectionId = 0)))
        assertFalse(boxRoundtripConfirmed(written, 11, read.copy(armed = true)))
    }
    @Test fun wrongTagContentAndIncompleteReadsNeverConfirm() {
        for (tag in listOf(read.copy(tagId = "04010203040507"), read.copy(data = NfcTestFixture.initial),
            read.copy(busy = true), read.copy(listening = false), read.copy(tagId = null))) {
            assertFalse(boxRoundtripConfirmed(written, 11, tag))
        }
        assertFalse(boxRoundtripConfirmed(written.copy(outcome = 7), 11, read))
    }
    @Test fun onlyFreshSupportedPhysicalFixtureCanBeWritten() {
        val initial = written.copy(outcome = 0, data = NfcTestFixture.initial)
        assertTrue(BoxNfcUiState(result = initial).canChange)
        assertFalse(BoxNfcUiState(result = initial, busy = true).canChange)
        assertFalse(BoxNfcUiState(result = written).canChange)
        assertFalse(BoxNfcUiState(result = initial.copy(kind = 1)).canChange)
        assertFalse(BoxNfcUiState(result = initial.copy(uid = ByteArray(4))).canChange)
    }
}
