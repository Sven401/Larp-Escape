package de.larpescape.companion.data.device

internal fun audioResultMessage(outcome: Int): String = when (outcome) {
    0 -> "Audio-Befehl über UART übertragen · Höreindruck offen"
    1 -> "Box lehnt Audio ab: Spiel ist nicht gestoppt"
    2 -> "Audiozustand hat sich geändert · erneut bewusst starten"
    4 -> "Box lehnt ab: Audio-Ausgabe nicht bereit"
    8 -> "Box lehnt Audio ab: Auftrag abgelaufen"
    9 -> "Box ist beschäftigt · erneut bewusst versuchen"
    10 -> "Box meldet einen Fehler der Audio-Ausgabe"
    11 -> "Audio-Auftrag wurde durch eine andere Bedienung beendet"
    else -> "Box hat den Audio-Auftrag abgelehnt ($outcome)"
}
