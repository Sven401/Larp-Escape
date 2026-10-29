# PR #100: Mergevorbereitung des begrenzten Vorbereitungspakets

Stand: 29.10.2026. Auftrag: PR #100 in einen mergefähigen Stand bringen.
Verantwortlich für Plattform/Integration und Nachweise: Hauptagent `/root`.
Anderer Security-/Qualitätsgegenprüfer: `/root/pr100_review`.
Varianten: Android-Protokollkern, Dev/GM unverändert angebunden; optionaler
Legacy-GATTS-Rückkanal und bestehende Dienstvarianten; keine neue Produktaktivierung.

## Umfang und Herkunft

PR-Head vor Überarbeitung: `0c8ced2d7296d97b627c855d9141385a197e1549`.
Aktuelle Integrationsbasis: `75422b6f2da2223a15651311e5901c6d800d8c41`
mit #102 und #104. Konfliktfreier Zusammenführungscommit dieser Vorbereitung:
`90332fd8168195e7f92adea1c884178848d040c1`.

Der bestehende PR liefert Profilcodec/Transaktionskern und optionalen
Firmware-Rückkanal. Er ist als solches Vorbereitungspaket unabhängig von
#101/#103 integrierbar. Er implementiert oder aktiviert keinen vollständigen
BLE-Produktkanal. Die ursprüngliche Produktaufgabe bleibt offen; ihre
[Übergabe](BLE-ROLLEN-PROFILE-2026-10-28.md) und der
[Fortsetzungsplan mit S1–S6](BLE-FORTSETZUNGSPLAN-2026-10-28.md) bleiben maßgeblich.
Der Nutzerauftrag vom 29.10. nimmt die PR-Vorbereitung wieder auf; historische
Sitzungsabschlüsse sind keine Sperre für diese begrenzte Integrationsarbeit.

#101 und #103 werden hier nicht eingespielt. Der dort weiterentwickelte
SDK-/TLS-/NimBLE-Pfad bleibt eigene Folgearbeit. Ein Merge dieses Pakets
qualifiziert insbesondere nicht das alte SDK für den späteren geschützten
Produktkanal. Geräte-/Stack-/Zeitabnahmen und S1–S6 werden nicht geschlossen.

## Gegenprüfung und Aktivierungsgrenzen

Der andere Bearbeiter prüfte am ursprünglichen PR-Head ProfileTransferV1,
ProfileExchange, deren Tests/Profilkontrollvertrag, Esp32GattsOutput, optionalen
Dienstaufbau, SDK-Permissions, Endpointprüfung und GattsOutputCases.
Kein P1/P2-Befund im begrenzten Umfang. Keine eigenen Tests des Gegenprüfers.
Ein anschließender Dateivergleich am Zusammenführungscommit bestätigt,
dass die beiden neuen Kotlin-Produktdateien und lib/GattsWriteAdmission
gegenüber dem geprüften ursprünglichen PR-Head unverändert sind.

ProfileExchange wird nur in Tests instanziiert. Der optionale Outputdienst
wird nur in Hosttests aktiviert. Originalverbindung, aktuelle GM-Authority,
Lifecycle und SDK-Security bleiben Voraussetzung privater Ausgabe. HMAC,
Peeranmeldung und Kopiererfolg verleihen keine Rolle oder Speicherbestätigung.
Die Standardwerte erhalten die bisherigen Dienstvarianten.

## Nachweise

Aktuelle lokale Hosttests und die neue vollständige PR-CI werden am finalen
Head ausgewertet. Historische Ergebnisse in der ursprünglichen Übergabe
bleiben ihren dort genannten Commits zugeordnet. Kein Geräteupload, keine
neue Geräteabnahme und kein Merge in den Integrationsbranch in diesem Auftrag.

Lokal am Zusammenführungscommit: PlatformIO host-regression und host-configured-control erfolgreich, 646/646 Fälle in 13 Suiten, Exit 0. Kein neuer lokaler Android-/SDK-/Kryptobuild; diese Nachweise liefert die neue PR-CI gesondert.
