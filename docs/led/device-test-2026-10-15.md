# Geräteprüfung der eigenständigen LED-Funktion · 15. Oktober 2026

Status: **belegt für den unten bezeichneten Installationsstand und Prüfumfang**.
Quelle: Installation am angeschlossenen Handy/ESP32, Geräteantworten in der echten
Companion-App und getrennte Sichtbestätigungen des Nutzers in der gemeinsamen
Sitzung. Der frühere Architekturversuch ist ausschließlich im
[historischen Bericht](install-2026-10-15.md) erfasst; seine Beobachtungen werden
hier nicht übernommen.

## Installierter Stand

App und Firmware stammen aus `7d1f45bfc7f8204e5f96c820a8d569ac8180051d`.
Die anschließende Korrektur `964810238ba55ec4e19a0ea5c4d70e7e11064ba9`
betrifft nur die Testauswahl der alten Hostsuite, nicht die installierten Quellen.
Die LED-Firmware besitzt eigene Effektberechnung und direkte ESP32-SDK-RMT-Ausgabe.
Sie meldet `LED_ONLY`; eine Spielintegration ist nicht enthalten.

| Artefakt | SHA-256 |
|---|---|
| Installiertes Debug-APK | `8bfe7b433319c7700730df7e689939650a7fc29cdea8b1a937660f5272d88b15` |
| Firmware, 1.121.360 Byte | `f14d9453d1f75554c1c6914798d72b306fcd6ea19b828965abc4dcac669feaa1` |
| ELF | `493fc47986dca90ed4932640ad25395a53cd214971025047b3d50ec83befbc2b` |

Handy: moto g14, Android 14. App: `de.larpescape.companion`, Installation mit
Erhalt der Appdaten erfolgreich. Das anschließend vom Handy zurückgelesene
installierte APK stimmt bytegenau per SHA-256 mit dem geprüften Build überein.

Box: ESP32-D0WD-V3, Revision 3, 4 MiB Flash, Anschluss COM3; vorhandener Streifen
mit 23 LEDs an GPIO 15. Vor dem Upload wurde der chipgebundene Rückweg abgeglichen,
ein vollständiger 4-MiB-Flashabzug erstellt und auf demselben Chip verifiziert.
Sein SHA-256 ist `4c2be120da4db93e5c4f54ceb0eae9d3050d7a1876096a436b5d61f82542c9bf`.
Der Abzug enthält Zugangsdaten und bleibt privat außerhalb des Repositories.

Nur die Anwendung bei `0x10000` wurde geschrieben; der Upload meldete erfolgreiche
Hashprüfung. Bootloader, Partitionstabelle und gespeicherter Geräteschlüssel
blieben erhalten. Im anschließenden 30-Sekunden-Startprotokoll erschien genau eine
Bereitmeldung und kein beobachteter Panic-, Abort-, Brownout- oder Neustartfehler.
Das ist kein Dauerlaufnachweis.

Der über USB erneut exportierte Zugang wurde ohne Ausgabe seines Inhalts mit dem
vorherigen Zugang verglichen: Geräte-ID und Schlüssel unverändert. Der Import
über die echte Android-Dateiauswahl und die gegenseitige Anmeldung waren
erfolgreich; die App meldete „Box-Zugang bestätigt · LEDs bereit“. Die
Transportkopie auf dem Handy wurde anschließend entfernt.

## Gemeinsame Sichtprüfung

Jeder folgende Befehl wurde über die echte App und ihren geschützten BLE-Weg
aufgerufen. Ausführung/Endstatus der Box und sichtbare LEDs wurden getrennt
beobachtet. Zwischen den Schritten wurde die Nutzerantwort abgewartet.

| Schritt | Geräteantwort bzw. Folgestatus | Sichtbestätigung des Nutzers |
|---|---|---|
| Funkeln starten | „Funkeln gestartet“, aktiver Status Funkeln | „Ja, der Streifen funkelt fortlaufend.“ |
| Stop während Funkeln | „LEDs aus“, Status LEDs aus | „Ja, alle LEDs am Streifen sind aus.“ |
| Lichtimpuls starten | „Lichtimpuls gestartet“, anschließend „Effekt beendet, LEDs aus“ | Der erste kurze Durchlauf wurde verpasst. Auf ausdrücklichen Wunsch bewusst erneut gestartet: „Ja, einmal weiß aufgeblendet und danach vollständig aus.“ |
| Ripple starten | „Ripple gestartet“, anschließend „Effekt beendet, LEDs aus“ | „Ja, blaue Lichtwelle und danach vollständig aus.“ |

Ergebnis: **Funkeln, Stop, Lichtimpuls und Ripple sind in diesem Umfang am neuen
Paar sichtbar bestätigt.** Die beiden Einmaleffekte endeten selbstständig dunkel.
Am Ende meldete die App LEDs aus; der Nutzer bestätigte den vollständig dunklen
Streifen. Die Firmwarevorgaben 1,6 Sekunden für Lichtimpuls und 1,4 Sekunden für
Ripple wurden nicht mit einer Uhr oder einem Messgerät nachgemessen.

## Getrennte Softwareprüfungen

- Eigener BLE-Wiretest: 996 Assertions mit echten HMAC-Implementierungen erfolgreich.
- Eigene LED-Ausgabe: 16 Enginefälle und 4 Tests des tatsächlichen SDK-Adapters
  gegen SDK-Stubs erfolgreich.
- Android: 57 JVM-Fälle erfolgreich; Debug-/Release-Builds und Lint erfolgreich.
  Lint: null Fehler, 18 Debug- beziehungsweise 13 Release-Warnungen.
- Eigenständiger ESP32-Build erfolgreich: 1.115.061 von 3.145.728 Byte
  Anwendungsspeicher und 42.056 von 532.480 Byte statischer RAM laut Buildbericht.
- Nach Korrektur der Testauswahl am Stand `9648102`: alte Hostregression
  105/105 Fälle erfolgreich. Ihr vorheriger CI-Fehler auf `7d1f45b` entstand,
  weil die eigenständige Wiretestsuite zusätzlich vom Unity-Lauf eingesammelt wurde.

Aktuelle CI-Ergebnisse sind am jeweiligen Kopfstand von [PR #45](https://github.com/Sven401/Larp-Escape/pull/45)
zu prüfen. Ein früherer grüner Lauf wird nicht auf spätere Commits übertragen.

## Verbleibende Grenzen und Belege

Nicht Teil dieses Sichttests: Auswahl ohne Wirkung während eines laufenden
Effekts, Wechsel eines laufenden Effekts durch Start, Stop/Neustart jedes einzelnen
Effekts, erkannter Funkabbruch mit anschließendem dunklem Wiederverbinden,
falscher Zugang, Fehler-/Timeoutfälle und Dauerbetrieb. Frühere Labor- oder
Architekturtests ersetzen diese Prüfungen am neuen Paar nicht.

Der LED-Zugang ersetzt keine allgemeinen GM-/Spielerrollen und keine
SMP-Bondabnahme. Es gibt hier keinen gemessenen Spielzustand; die spätere
Spielsperre und der kombinierte Spielbuild bleiben getrennte Arbeit. Ebenso
fehlen Strom-/Signalmessung und vollständige Hardwareabnahme.

Private Rohbelege liegen unter `%LOCALAPPDATA%/LarpEscape/LabRuns/LED-Product-20261015-Independent`:
Installationsmanifest, Build-/Upload-/Startprotokoll, installierte APK-Kopie,
UI-Abzüge der Start-/Endzustände und Sichtprotokoll. Schlüsseldatei und Flashabzug
werden nicht veröffentlicht. Aufbau, Reproduktion und weitere Abnahmefälle stehen
in [firmware/led/README.md](../../firmware/led/README.md).
