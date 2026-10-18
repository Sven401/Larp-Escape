# Geräteprüfung: Dev-Update und parallele Spielleiter-App

**Bestätigt am 18. Oktober 2026**, Nutzerauftrag in Sitzung
`01a150b6-981d-7791-a37d-34fac8dd18a3`. Moto G14, Android 14 / API 34.
Geprüfter Quellstand: `c7c0c57d1e30ddc8bc61a6cf32e0638ef302686f`
(inhaltlich identischer Buildbaum vor dem Commit).
Grundlage: [technische Apptrennung](android-app-trennung.md).

## Ergebnis

Dev wurde am 18.10.2026 mit `adb install -r`
aktualisiert. Es gab keine Deinstallation, Datenlöschung oder Änderung der
Paketkennung. Anschließend wurde GM zusätzlich installiert und separat erneut
aktualisiert. Beide Apps starten und lassen sich abwechselnd in den Vordergrund
holen; beide Prozesse waren danach gleichzeitig vorhanden. Das bestätigt den
parallelen Appbetrieb, keine gleichzeitige Steuerung derselben Box.

| Merkmal | Dev | Spielleitung |
| --- | --- | --- |
| Name in der App-Auswahl | **Larp Dev** | **Larp GM** |
| Überschrift in der App | Larp Escape Dev | Larp Escape Spielleitung |
| Paket | `de.larpescape.companion` | `de.larpescape.gamemaster` |
| Version nach Installation | Code 2 / 0.2, vorher Code 1 / 0.1 | Code 1 / 0.1 |
| App-UID | Vor/nach Update gleich | Abweichende UID, eigener Appbereich |
| Erstinstallation | Ursprünglicher Zeitpunkt erhalten | Eigene Neuinstallation |
| Sichtbarer Startzustand | Bisherige Testoberfläche und gespeicherte Matrix | Keine Veranstaltung eingerichtet |

Die tatsächlich installierten APKs wurden abschließend zurückgelesen; ihre
SHA-256 stimmen mit den geprüften lokalen Debug-APKs überein:

- Dev: `2f4fa0aaa1e766e42a000a1173352f5942b2d52828563ec938cdd0db7946de2d`.
- GM: `9ebe898c2b068a81fd4f614bf7bb0c0f6d5fc324e5567b62309004f15b888e69`.

## Nachweis des Datenerhalts

Vor dem Update wurde Dev angehalten und der zugängliche Bestand aus `files`,
`no_backup` und `shared_prefs` lokal gesichert: zehn Dateien, mit individuellen
Größen und SHA-256. APK-Baseline und Datenarchive bleiben im Git-ignorierten
lokalen Buildbereich; private Appdaten und Schlüssel werden nicht veröffentlicht.
Der nicht exportierbare Android-Keystore-Schlüssel wurde nicht ausgelesen.

| Prüfung | Beobachtung |
| --- | --- |
| Direkt nach Dev-Update, vor erstem Start | Alle zehn Dateien bytegleich; App-UID und Erstinstallationsdatum erhalten |
| Nach zusätzlicher GM-Installation | Alle zehn Dev-Dateien weiterhin bytegleich |
| Nach Dev-Start, Szenarioansicht und Appwechseln | Alle neun übrigen Dateien bytegleich; ausschließlich `files/profileInstalled` erneuert |
| Nach separatem GM-Update | Alle zehn aktuellen Dev-Dateien unverändert gegenüber dem Zustand vor diesem GM-Update |
| Verschlüsselter Boxzugang | `led-access-v1.bin` bytegleich; Appstart liest/entschlüsselt den Zugang und wählt damit den gespeicherten Matrixbestand; kein Zugangslesefehler |
| Szenario | Datei bytegleich; sichtbare Szenarioinhalte vor/nach Update bis auf Appüberschrift identisch, zwei Szenen |
| Matrix | Beide Dateien bytegleich; 68 von 81 Koordinaten, 110 Betätigungen, neun unbestückte Felder und vier offene Problemstellen erneut angezeigt |
| RSSI-Messungen | Beide Dateien, zusammen 22571 Bytes, bytegleich |
| Wake-Zuordnung und NFC-Auftragsjournal | Bestehende SharedPreferences bytegleich |
| Trennung der GM-Daten | Eigene UID; keines der Dev-Datenspeicherformate im neuen Appbereich vorhanden |

Die 24-Byte-Datei `files/profileInstalled` ist der Cache des eingebundenen
AndroidX `ProfileVerifier` (Version 1.3.1). Das wurde am lokalen Bibliotheksbytecode
geprüft: Schema, Ergebniscode, Paket-Updatezeit und Profilgröße. Die gespeicherte
Updatezeit änderte sich passend von der vorherigen auf die neue Paket-Updatezeit.
Diese Laufzeitdatei wurde nicht als verlorener Projektinhalt gewertet; die
Abweichung wird ausdrücklich ausgewiesen statt pauschal alle Dateien auszunehmen.

Der Boxzugang wurde durch den normalen App-Ladepfad geprüft. Eine erneute
BLE-Authentifizierung an einer eingeschalteten Box ist nicht Bestandteil dieses
Tests. Kein Szenario wurde bearbeitet, kein neuer Matrixlauf begonnen, kein
NFC-Schreibauftrag ausgelöst und keine Firmware geflasht.

## Iconquelle und Unterscheidbarkeit

Auf ausdrücklichen Nutzerauftrag wurde das Icon der auf diesem Handy vorhandenen
**GM Companion** (`com.companyname.mauiapp1`) übernommen, nicht das einer fremden
ähnlich benannten Anwendung. Quelle: dessen installierte APK, SHA-256
`ebdf23806c3f7237095e6c26482aaf1097722af6aa33e2e11a57fb55b12ba9d6`.

Die vorhandene Vordergrundebene `res/mipmap-xxxhdpi-v4/appicon_foreground.png`
(432 × 432) wird unverändert in beiden Modulen als
`drawable-nodpi/gm_companion_foreground.png` geführt. SHA-256 der PNG-Datei:
`9d16f0ac05a25325dfbd0adfe154345b137ef5eb004d3a388752255a700082b7`.
Der adaptive Hintergrund behält die aus dem Original gelesene Farbe `#E2D6D4`.
Eine ursprüngliche SVG-Quelldatei lag im zugänglichen aktuellen Projekt nicht vor;
die extrahierte PNG-Ebene bleibt als Herkunftsressource erhalten.

Dev ergänzt darüber einen editierbaren Android-Vektorbadge mit weißem **DEV**
auf dunklem Grund. GM zeigt das unveränderte Originalmotiv. Die kurzen Launcher-
Namen vermeiden gleiche abgeschnittene Titel. Ein separater Sichtdurchgang auf
Screenshots bestätigt lesbare Namen und unterscheidbare Icons in derselben
App-Suche. Die ältere Diagnose-Vorschau und die historische GM-Companion-App
wurden weder verändert noch entfernt.

Die Screenshotbelege bleiben ausschließlich lokal unter
`companion/android/app/build/device-verification/` (Git-ignoriert). Sie werden
wegen möglicher privater UI-Inhalte nicht mit dem Pull Request veröffentlicht.

## Native Sicht- und Ablaufprüfung

Dev zeigt die erwartete Überschrift und den erhaltenen Matrixbestand; der
Szenarioreiter zeigt die gespeicherten Inhalte. GM öffnet seinen eigenen leeren
Veranstaltungseinstieg ohne Dev-Navigation. Unterbrechung und Rückkehr durch
mehrfachen Appwechsel funktionieren im geprüften Zustand.

Lokale Belege: `dev-after.png`, `scenario-after.png`, `gm-after.png` und
`launcher-after.png` im oben genannten ignorierten Prüfverzeichnis.
Der öffentliche Bericht enthält keine Szenariobezeichnungen oder privaten Inhalte.

Beide Apps wurden erneut als Debug und unsigniertes Release gebaut; vier
Lintläufe ohne Fehler mit den bereits dokumentierten drei Warnungen je Lauf
(bei Dev zusätzlich ein Informationshinweis). Die acht Tests des Trennungsprüfers
und die Prüfung aller vier APKs bestanden. Der Versions-/Signaturabgleich gegen
das unmittelbar zuvor installierte Dev-APK bestand ebenfalls. Kotlin-Funktionscode
wurde in diesem Folgeschritt nicht geändert.

Offen bleiben vollständige Accessibility-/Schriftgrößenabnahme, Langzeitbetrieb,
BLE-/NFC-Gerätefunktionen und die fachliche Umsetzung der Spielleitungsabläufe.
Die Abnahme gilt für das angeschlossene Moto G14 und die genannten APKs.
