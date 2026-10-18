# UI/UX-Pilot: Box auswählen, verbinden und wieder aufnehmen

**Status: Designrichtung bestätigt, Figma und interaktiver Webprototyp ausgearbeitet; native Produktumsetzung und Geräteabnahme offen.**
Datum: 18. Oktober 2026. Auftrag und Sitzung: siehe [Rollenvertrag](ROLLE.md).
Geprüfte Quellbasis: `7e72411bb1c416b5864036c79a1e17d9cf46b5ac`.

## Aktuelle Entwurfsfortschreibung

Die Nutzerkorrektur zum Veranstaltungskontext und der überarbeitete Bildentwurf
stehen in [Entwurf v2](REVISION-VERANSTALTUNG.md). Veranstaltung und Station
stehen in der Produktbedienung im Vordergrund; die generischen Bestandsbegriffe
unten bleiben technische Herkunft. Die Richtung wurde bestätigt; [Figma und interaktiver Prototyp](../../design/box-pilot/README.md) sind ausgearbeitet. Native Abnahme bleibt offen.

## Aktueller Bediengrundsatz · ersetzt manuelle Verbindung im Entwurf

**Bestätigt, 18.10.2026:** Nutzerfeedback nach Ausprobieren des Prototyps.
Im konkreten Veranstaltungskontext wählt die Spielleitung die Station; bei
funktionierender Vorbereitung laufen Gerätesuche, Auswahl des eindeutig
zugeordneten Geräts, Zugang und Wiederverbindung automatisch. Technische
Hinweistexte über Versand, Geräteantwort und physische Sichtprüfung gehören
in Diagnose/Abnahme, nicht in die Spielleitungsbedienung.

Aktueller Normalweg: Station auswählen → automatisch verbinden → bedienen.
Bei Unterbrechung: Zustand entwerten → automatisch neu verbinden und prüfen →
weiter bedienen. Lichtaufträge werden nicht wiederholt. Fehlende/mehrdeutige
Zuordnung und fehlender Zugang bleiben gezielte Ausnahmen mit konkreter Hilfe.

## Brief

Nutzer: Spielleitung mit Android-Handy an einer lokalen Station. Ziel: die Station
auswählen, automatisch verbinden, Licht bewusst bedienen und nach Funkverlust
mit frisch geprüftem Zustand weiterarbeiten. Android/Compose und Material 3
sind die vorhandene Plattform. Der Pilot benötigt weder Internet noch Zentrale.
Demodaten bleiben ausdrücklich als Entwurf gekennzeichnet.

Umfang: Stationsauswahl → automatische Verbindung/Zugangsprüfung → Lichtbedienung →
Unterbrechung → automatische Wiederverbindung und frischen Stand prüfen. Allgemeine
Veranstaltungsverwaltung, neue Rechtevergabe, NFC-Schreiben, Boxwechsel mit
Schlüsselverwaltung und echte Spielsteuerung gehören nicht in diesen Pilot.
Als vorhandene Bedienaktion dient **Funkeln**, mit bewusstem Start und Stop.
Die bestehende App bezeichnet diesen Pfad als LED-Bedienung mit offener
Spielintegration. Keine neue Spielstart-Funktion im Entwurf.

Gestalterische Ausgangslage aus Quellcode: `MaterialTheme(lightColorScheme())`,
Material-3-Typografie, Hauptabstände 24 dp, Gruppenabstand 16 dp, volle
Buttonbreite. Diese Werte sind Bestandsbeobachtungen, kein vollständig
abgenommenes Designsystem. Für die erste Exploration helle Material-Oberfläche,
ruhiger violetter Akzent, klare deutsche Texte und Status zusätzlich zur Farbe.
Violett ist eine Gestaltungsvorgabe für den Pilot, kein ausgelesener Farbwert.
Neue visuelle Optionen variieren vor allem Hierarchie und Interaktion.

## Geprüfter Bestand und Grenzen

| Aussage | Status und Quelle |
| --- | --- |
| ESP/NFC/Szenario sowie Verbindung und Baugruppen stehen in einer scrollbaren Hauptspalte | Im Code bestätigt: `MainActivity.kt`, `onCreate` |
| Aktueller Scan dauert zehn Sekunden und sammelt höchstens 16 Geräte | Im Code bestätigt: `AndroidBleRepository.scan` |
| Werbename und Bluetoothadresse sind kein geprüfter Identitätsnachweis | Bestehender Vertrag und `AndroidBleRepository.onServicesDiscovered` |
| Ein importierter Box-Zugang ist gespeichert; der Import trennt die bisherige Sitzung | README und `AndroidBleRepository.importLedAccess` |
| Authentisierung, frischer Status, Bereitschaft und passender Betriebsmodus sperren/erlauben LED-Befehle | `LedUiState.canStart/canStop` |
| Reconnect und Rückkehr benötigen frische Zustandsprüfung; kein automatischer Start | `LedBleSession`, `DeviceViewModel.foregroundEnded`, README |
| Box bleibt nach VA-01 lokale Spielautorität; BLE-Verlust bedeutet keinen behaupteten Spielstopp | Verbindliches Architektur-Soll; kein Nachweis allgemeiner Spielimplementierung |

Quellpfade relativ zum Repository:
`companion/android/app/src/main/java/de/larpescape/companion/` mit
`MainActivity.kt`, `data/device/{DeviceRepository,LedUiState}.kt`,
`ui/device/{DeviceViewModel,LedControls}.kt`,
`platform/ble/{AndroidBleRepository,LedBleSession}.kt`.
Weitere Verträge: [App-README](../../companion/android/README.md),
[BLE v2](../led/ble-wire-v2.md), [VA-01](../veranstaltungsarchitektur.md),
[Zugang](../companion/veranstaltungszugang.md).

Am 18.10.2026 ergab `adb devices` nach erfolgreichem Serverzugriff **kein Gerät**.
Keine aktuelle Appansicht aufgenommen, keine Bildschirmbedienung, Installation
oder Funkprobe. Die folgende Bewertung ist eine Quellcodeanalyse mit
Gestaltungshypothesen. Vorhandene Hardwarediagramme sind keine UI-Referenzen.
Product-Design-Kontextprüfung: keine gespeicherte Kontextdatei vorhanden.
Projektkontext bleibt in diesem Repository.

## Verbindlicher Zustands- und Übergangsvertrag

Stand: PO-Korrektur vom 18.10.2026 auf Basis `c04a540`.
Diese Tabelle und der folgende Prüfplan sind die aktuelle Umsetzungsvorgabe.
Die [frühere Analyse](BOX-PILOT-HISTORIE.md) bleibt ausschließlich Herkunft.

| Zustand | Sichtbare Rückmeldung / Aktion | Übergang und Sperren |
| --- | --- | --- |
| Stationsliste | Veranstaltung, Station; Station antippen | Automatische Suche und eindeutige Zuordnung, keine Kandidatenwahl |
| Verbinden / Prüfen | „Station wird verbunden …“, Abbrechen | Bedienung gesperrt bis Zugang und frischer Gerätestand bestätigt; Abbrechen pausiert |
| Zugriff fehlt / Bluetooth aus | Ursache und passende Systemaktion | Nach Erlaubnis/Einschalten automatisch prüfen; keine Lichtaktion |
| Nicht erreichbar | Näher an Station gehen; Erneut versuchen | Ein neuer Verbindungsversuch; kein endloser automatischer Suchlauf |
| Zuordnung unklar | Verantwortliche Person um Prüfung der Gerätezuordnung bitten; Hilfe zur Einrichtung, Erneut prüfen | Keine beliebige Gerätewahl; erneute Prüfung verändert die Konfiguration nicht |
| Stationszugang fehlt | Verantwortliche Person um Prüfung des Stationszugangs bitten; Hilfe zur Einrichtung, Erneut prüfen | Kein erfundener Import oder Rechtegewinn durch Wiederholen |
| Einrichtungshilfe | Veranstaltung, Station, konkreter Fehler, zuständige Person und Prüfschritte; Zur Stationsbedienung | Nur Anleitung; außerhalb liegende Einrichtung explizit benannt; Rückkehr behält den Fehler |
| Bereit | „Bereit“, „Licht aus“ oder bestätigter Effekt; Auswahl, Start, Stop | Auswahl lokal; nur bewusster Start/Stop sendet einen Auftrag |
| Auftrag offen | „… wird gestartet“ / „Licht wird gestoppt“ | Auswahl und Befehle gesperrt; tatsächlicher Gerätezustand noch nicht behauptet |
| Unterbrochen | „Aktueller Zustand unbekannt“, bei offenem Auftrag Ergebnis offen | Alte Anzeige entwertet; Auswahl bleibt; automatischer Verbindungsversuch ohne Lichtauftrag |
| Status veraltet / Rückkehr | Ursache; automatische erneute Prüfung | Zugang und frischen Gerätestand erneut prüfen; keine Befehlswiederholung |
| Wieder verbunden | Frisch gelesener Gerätestand | Ausgeführter Auftrag sichtbar oder unveränderter Zustand bei nicht angekommenem Auftrag; kein Erfolgsbeleg für den alten Auftrag allein aus Reconnect |
| Bewusst pausiert / zurück zur Liste | Erneut verbinden / Station wählen | Kein automatischer Verbindungsstart nach bewusstem Abbrechen; Stationsöffnung prüft neu |
| Prozessneustart | Stationsliste, keine wiederhergestellte Sitzung behaupten | Native Umsetzung separat prüfen; der Pilot speichert keinen Auftrag zum Wiederholen |

Verbindung ist keine Licht-/Spielaktion. „Zustand unbekannt“ bedeutet weder
„Licht aus“ noch „Spiel gestoppt“. Der Prototyp verwendet verkürzte Simulationszeiten;
Protokollfristen und Android-Lebenszyklus bleiben durch ihre Verträge festgelegt.

## Vorgeschlagene Designregeln

Ein eindeutiger Hauptschritt je Screen; Boxbezug und Sitzungsstatus im selben
Bereich. Zustände durch Text und Form, zusätzlich Farbe. Keine unbestätigten
Erfolgsanzeigen. Sekundäre Einrichtung getrennt von Start/Stop. Bedienziele
mindestens 48 dp als Entwurfsziel, Körpertext ungefähr 16 sp; 200 % Schriftgröße,
kleine Breite und Screenreader werden im nativen Review geprüft. Platz für
systemseitige Insets, keine schwebenden Elemente über wichtigen Aktionen.
Designziele sind noch keine gemessene Zugänglichkeitsabnahme.

## Verbindlicher Akzeptanz- und Prüfplan

| Fall | Erwartung | Nachweis |
| --- | --- | --- |
| Normalweg | Ein Stationsklick bis Bereit; keine Suche-/Kandidatenwahl | Browser, später Android |
| Einrichtungsfehler, jeweils Zuordnung und Zugang | Hilfe nennt zuständige Person, Kontext und passende Schritte; Rückkehr erhält Fehler | Browser |
| Erneut prüfen ohne externe Korrektur | Derselbe Fehler bleibt; Start/Stop gesperrt | Browser + Modelltest |
| Externe Korrektur | Nur Teststeuerung simuliert Korrektur; erst erneute Suche/Auth/Status ermöglicht Bedienung | Browser + Modelltest |
| Auftrag ausgeführt, Antwort verloren | Zunächst unbekannt, nach automatischer Prüfung tatsächlicher neuer Zustand; kein Retry | Browser + Modelltest, später BLE |
| Auftrag nicht angekommen | Zunächst unbekannt, danach alter Gerätestand; kein Retry, neuer Auftrag nur bewusst | Browser + Modelltest, später BLE |
| Stop mit / ohne Zustellung | Analog beide Ausgänge; bei nicht zugestelltem Stop läuft der Effekt weiter | Modelltest + Browser, später BLE |
| Status veraltet / Hintergrund und Rückkehr | Alte Anzeige entwertet, automatisch neu prüfen, keine Lichtaktion | Modell + Browser; echter Lebenszyklus separat Android |
| Abbrechen / Stationsliste | Keine späten Erfolgsmeldungen, keine selbstständige neue Lichtaktion | Modell + Browser |
| Große Schrift | 200 %, Inhalt scrollbar, Hilfe/Rückkehr/Start/Stop erreichbar | Browser; 320 dp und native Schrift separat |
| Screenreader / Rotation / Neustart | Semantik, Fokus und Sitzungsregeln tatsächlich prüfen | Android/TalkBack offen |

## Umsetzung und Abnahmegrenze

Rolle, visuelle Auswahl, editierbarer Figma-Entwurf und interaktiver Prototyp sind vorhanden.
Einrichtungshilfe und beide Auftragsausgänge sind auch in Figma nachgeführt.
Maßgeblich sind diese Tabelle, der aktuelle Browser und die
[Figma-/PR-Nachprüfung](../../design/box-pilot/figma-sync-review.md).
Die [erste PO-Nachprüfung](../../design/box-pilot/po-review.md) bleibt historischer Beleg.
Die frühere manuelle Verbindung ist ausschließlich [historisch](BOX-PILOT-HISTORIE.md).
Native Android-Umsetzung, mehrere Stationen, reale Einrichtung, BLE, TalkBack und
Nutzerabnahme bleiben separate Schritte.

## Historischer Prüfstand der ersten Rollenfassung

Am 18.10.2026: Skill-Creator-Validator bestanden, Agenten-TOML syntaktisch und
auf Pflichtfelder geprüft, lokale Markdownverweise und alle drei Bildhashes
geprüft; `git diff --check` ohne Whitespacefehler. Der Validator benötigte eine
lokal bereitgestellte YAML-Abhängigkeit; diese wurde anschließend entfernt.
Keine App-/Firmwareänderung, daher kein neuer Build oder Produkttest.
Keine Behauptung automatischer Agentenregistrierung im laufenden Desktop.

## Historische erste Ausarbeitung nach Nutzerfreigabe

Am 18.10.2026 vom Nutzer zur Figma-/Prototypausarbeitung beauftragt.
[Entwurf und Anleitung](../../design/box-pilot/README.md),
[Designprüfung](../../design/box-pilot/design-qa.md),
[Figma](https://www.figma.com/design/3uQpZfwREhQ9cHk80jMtRw?node-id=3-20).
Neun Modellfälle, Typprüfung, Build, Runtimeintegrität und vier Packagingfälle
erfolgreich. Tatsächliche Browserbedienung der Haupt- und Fehlerwege; kein
Deployment und keine Änderung an Android-App oder Firmware.
