package de.larpescape.protocol

data class BoxNfcResult(val request: Long, val nextRequest: Long, val sequence: Long,
    val outcome: Int, val kind: Int, val uid: ByteArray, val data: NfcTagData?) {
    val uidText get() = uid.joinToString("") { "%02X".format(it) }
    val message get() = when (outcome) {
        0 -> if (data == null) "Tag erkannt · keine passende Testpayload gelesen" else "Box hat Payload gelesen · Prüfsumme bestätigt"
        1 -> "Kein Tag erkannt · Tag auflegen und erneut lesen"
        2 -> "Tag oder Speicherformat wird von der Box nicht unterstützt"
        3 -> "Tag ist schreibgeschützt oder hat eine nicht unterstützte Konfiguration"
        4 -> "Keine vorbereiteten Testdaten mit Flags 1 · zuerst in der App schreiben"
        5 -> "Tag oder Inhalt gewechselt · Schreibauftrag abgelehnt"
        6 -> "Box konnte den Lesevorgang oder das Abschalten des NFC-Felds nicht bestätigen"
        7 -> "Schreibausgang unklar · zuerst nur lesen, nicht erneut schreiben"
        8 -> "Box hat Flags 2 geschrieben und frisch zurückgelesen · jetzt denselben Tag am Handy lesen"
        9 -> "NFC-Baugruppe nicht bereit oder noch beschäftigt · erneut lesen"
        10 -> "Ein NFC-Auftrag läuft bereits"
        11 -> "Auftrag abgelaufen · erneut bewusst auslösen"
        else -> "Auftragsfolge widersprüchlich · neu verbinden und nur lesen"
    }
}
