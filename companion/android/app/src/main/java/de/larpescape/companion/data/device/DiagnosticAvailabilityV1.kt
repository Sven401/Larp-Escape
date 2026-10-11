package de.larpescape.companion.data.device

import de.larpescape.protocol.DiagnosticGattFailureV1
import de.larpescape.protocol.DiagnosticGattPhaseV1

/** Connection observation only. No value in this type permits a device action. */
enum class DiagnosticAvailabilityV1(val detail: String) {
    DISCONNECTED("Kein ESP verbunden. Suche starten und ein Gerät verbinden."),
    PERMISSION_MISSING("Bluetooth-Zugriff fehlt. Ohne Erlaubnis ist keine Prüfung möglich."),
    BLUETOOTH_OFF("Bluetooth ist ausgeschaltet. Bitte einschalten und erneut suchen."),
    SEARCHING("Suche läuft. Zum Prüfen anschließend ein ESP verbinden."),
    SEARCH_COMPLETE("Suche beendet. Für die Diagnoseprüfung ein gefundenes ESP verbinden."),
    SEARCH_FAILED("Die Gerätesuche ist fehlgeschlagen. Bitte erneut versuchen."),
    CONNECTING("Verbindung und Dienste werden geprüft."),
    CONNECTION_FAILED("Die BLE-Verbindung ist fehlgeschlagen. Bitte erneut verbinden."),
    DISCOVERY_FAILED("Die GATT-Dienstsuche ist fehlgeschlagen. Bitte erneut verbinden."),
    LAB_ONLY("Vertragslabor verbunden. Das ist keine Prüfung der Produktdiagnose."),
    PRODUCT_SERVICE_MISSING("Der erwartete ESP-Produktdienst fehlt auf dieser Verbindung."),
    SERVICE_MISSING("Der erwartete Diagnose-GATT-Dienst fehlt auf dieser Verbindung."),
    CHARACTERISTIC_MISSING("Mindestens ein Diagnose-Schreib- oder Antwortmerkmal fehlt."),
    CHARACTERISTIC_AMBIGUOUS("Ein Diagnosemerkmal ist mehrfach vorhanden. Verbindung gesperrt."),
    PROPERTIES_WRONG("Diagnosemerkmale unterstützen Schreiben mit Antwort oder Benachrichtigung nicht wie vereinbart."),
    CCCD_MISSING("Ein benötigtes Diagnose-Benachrichtigungsabo fehlt."),
    MTU_PENDING("Diagnose-MTU wird ausgehandelt."),
    MTU_FAILED("Die Diagnose-MTU konnte nicht bestätigt werden."),
    MTU_TOO_SMALL("Die nutzbare BLE-Wertgröße liegt unter 128 Byte."),
    CONTROL_SUBSCRIPTION_PENDING("Kontrollantworten werden abonniert."),
    STEP_SUBSCRIPTION_PENDING("Schrittantworten werden abonniert."),
    SUBSCRIPTION_FAILED("Ein Diagnose-Antwortabo konnte nicht bestätigt werden."),
    SETUP_TIMEOUT("Die Diagnoseverbindung wurde nicht rechtzeitig eingerichtet."),
    ROLE_MISSING("Diagnose-GATT ist vorbereitet. Eine geprüfte Geräte- und Spielleitungsrolle fehlt; Geräteaktionen bleiben gesperrt."),
    ROLE_CHANGED("Die Rollenbindung passt nicht mehr zur ursprünglichen Verbindung."),
    TRANSPORT_ONLY("Diagnose-GATT ist vorbereitet. Es liegt keine bestätigte Geräteaktion oder Hardwareprüfung vor."),
    OTHER_BLOCK("Die Diagnoseverbindung ist gesperrt. Erneut verbinden und den Status prüfen."),
}

/** Converts actual GATT callbacks, never a cached or synthetic preview response. */
fun diagnosticAvailability(
    phase: DiagnosticGattPhaseV1,
    failure: DiagnosticGattFailureV1,
    blockStatus: String,
): DiagnosticAvailabilityV1 = when (phase) {
    DiagnosticGattPhaseV1.DISCOVERING -> DiagnosticAvailabilityV1.CONNECTING
    DiagnosticGattPhaseV1.MTU_PENDING -> DiagnosticAvailabilityV1.MTU_PENDING
    DiagnosticGattPhaseV1.CONTROL_CCCD_PENDING -> DiagnosticAvailabilityV1.CONTROL_SUBSCRIPTION_PENDING
    DiagnosticGattPhaseV1.STEP_CCCD_PENDING -> DiagnosticAvailabilityV1.STEP_SUBSCRIPTION_PENDING
    DiagnosticGattPhaseV1.TRANSPORT_READY -> when (blockStatus) {
        "ROLE_ISSUER_MISSING" -> DiagnosticAvailabilityV1.ROLE_MISSING
        "ROLE_GENERATION_MISMATCH" -> DiagnosticAvailabilityV1.ROLE_CHANGED
        else -> DiagnosticAvailabilityV1.TRANSPORT_ONLY
    }
    DiagnosticGattPhaseV1.BLOCKED -> when (failure) {
        DiagnosticGattFailureV1.WRONG_SERVICE -> DiagnosticAvailabilityV1.SERVICE_MISSING
        DiagnosticGattFailureV1.MISSING_CHARACTERISTIC -> DiagnosticAvailabilityV1.CHARACTERISTIC_MISSING
        DiagnosticGattFailureV1.AMBIGUOUS_CHARACTERISTIC -> DiagnosticAvailabilityV1.CHARACTERISTIC_AMBIGUOUS
        DiagnosticGattFailureV1.WRONG_PROPERTIES -> DiagnosticAvailabilityV1.PROPERTIES_WRONG
        DiagnosticGattFailureV1.MISSING_CCCD -> DiagnosticAvailabilityV1.CCCD_MISSING
        DiagnosticGattFailureV1.MTU_REQUEST_FAILED, DiagnosticGattFailureV1.MTU_CALLBACK_FAILED -> DiagnosticAvailabilityV1.MTU_FAILED
        DiagnosticGattFailureV1.MTU_TOO_SMALL -> DiagnosticAvailabilityV1.MTU_TOO_SMALL
        DiagnosticGattFailureV1.CONTROL_CCCD_FAILED, DiagnosticGattFailureV1.STEP_CCCD_FAILED -> DiagnosticAvailabilityV1.SUBSCRIPTION_FAILED
        DiagnosticGattFailureV1.SETUP_TIMEOUT -> DiagnosticAvailabilityV1.SETUP_TIMEOUT
        else -> DiagnosticAvailabilityV1.OTHER_BLOCK
    }
}
