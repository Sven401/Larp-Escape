package de.larpescape.companion.data.device

import de.larpescape.protocol.BoxNfcResult
import de.larpescape.protocol.NfcTestFixture

data class BoxNfcUiState(val supported: Boolean = false, val busy: Boolean = false,
    val result: BoxNfcResult? = null, val message: String = "Box verbinden; die Testfirmware muss NFC über die App unterstützen.") {
    val canChange get() = !busy && result?.let { it.outcome == 0 && it.uid.size == 7 && it.kind in 2..4 && it.data == NfcTestFixture.initial } == true
}
