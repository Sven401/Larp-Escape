# Manueller Fachaudit-Pilot: ESP32/FreeRTOS und Adafruit

**Historischer Pilotstand: durchgeführt; damalige Laufzeitbefunde unten erhalten.**
Fortschreibung 24.10.2026: Fachmandate ausdrücklich übernommen und A1-01/A2-01
[in der Software korrigiert und geprüft](../../runtime-fixes-2026-10-24.md).
Die folgenden Befundbeschreibungen beziehen sich auf den damaligen Code,
nicht auf die korrigierte Implementation. Geräte-/Supportgrenzen bleiben offen.
Zeitraum 23.–24.10.2026, Europe/Berlin. Technische Integrationsbasis:
`e61d7642d035ccdcf8eca689d6c50a82bcf1560b`; Ausgangscheckout des Pilots:
`200eea6e0d7110b76065b86887b18bdfa1fb25ef` mit demselben Produktcode.
Der Nutzer beauftragte zuerst die Prüfung der Verantwortlichkeiten und dann
den A1/A2-Pilot. Keine Scheduleraktivierung, Firmwareinstallation oder Migration.

## Ergebnis für die Organisation

Die Verantwortungsprüfung ging den technischen Einzelaufträgen voraus.
Bestehende LED-/UI-Rollen sind konkret, SDK-/Task-/Bus-/Hardwarefragen werden
überwiegend ad hoc dem Maintainer zugeordnet. Technische Owner in den Verträgen
sind keine benannten fachlichen Prüfer. Der Pilot wurde deshalb in zwei
Fachmandate aufgeteilt: **Plattform und Systemintegration** und
**Geräteintegration und Hardwarevertrag**. Ein zusätzlicher Review prüft die
Nachweiskette. [Konkreter Rollenvertrag](../../verantwortlichkeiten.md).

Das benötigt keine eigene Hierarchie je Hersteller. FreeRTOS-Queue und
Authority-Gültigkeit gehören gemeinsam ins Plattformmandat; Adafruit-
Rückgabewert und Spielwirkung gemeinsam in die Geräteintegration. Der
Maintainer verantwortet Integration und Priorität. Der Audit-Koordinator
organisiert Recherche und Nachverfolgung innerhalb der Qualitätsfunktion.

## Auftrag, Umfang und Grenzen

Je Fachauftrag höchstens drei priorisierte Befunde, Primärquelle mit
Version, tatsächlicher Aufrufpfad, konkrete Folge und getrennte Belegstärke.
Die Fachrecherche erfolgte in zwei Subagenten; Rollenprüfung und neue
Parsertests in einem weiteren. Der Maintainer prüfte Ableitungen und führte
die Änderungen zusammen. Dies ist interne Gegenprüfung, keine externe
Unabhängigkeit oder Zertifizierung. Herstellerverträge bilden hier den
Maßstab; keine ISO-, MISRA- oder vollständige Security-Konformitätsprüfung.

| Tatsächlicher Eingang | Geltung |
| --- | --- |
| PlatformIO espressif32 6.6.0; Arduino-Paket 3.20014.231204 | Lokal gelesene Header: Arduino 2.0.14 / ESP-IDF 4.4.6; IDF-Port basiert auf modifiziertem FreeRTOS 10.4.3. Die alternative PlatformIO-IDF-Option 5.2.1 ist ein anderer Frameworkpfad. |
| Adafruit PN532 1.3.4 + Projektpatch; MCP23017 2.3.2; BusIO 1.17.4 | Root-Legacyfirmware. Neue Baugruppenadapter gesondert gelesen. Manifest und lokaler Quellbestand belegen keinen installierten Gerätebestand. |
| Quellen-/Paketpins | Herkunfts-/Versionsschutz; kein Nachweis von Support, Busfunktion oder Sicherheitsfreiheit. |

## Befunde und tatsächliche Übernahme

### A1-01 · P1 · Queue-APIs im Authority-Spinlock

**Belegt, fachlich geprüft; Laufzeitkorrektur offen.**
[IDF 4.4.6, Restrictions and Considerations](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-guides/freertos-smp.html#restrictions-and-considerations)
fordert kurze Kritische Sektionen ohne FreeRTOS-API-Aufrufe. Im tatsächlichen
[Kanal](../../../../lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.cpp)
liegen `xQueueSendToBack` in `admit`, `xQueueReceive` in `takeCurrentReply` und
`xQueuePeek` in `readCurrentStatus` unter `authorityMutex_`.

Die [ausgeführte Diagnose](rtos-probe/result.json) kompiliert die echte
Kanalimplementation gegen temporär instrumentierte vorhandene Hoststubs:
je **ein** Send-/Receive-/Peek-Aufruf unter Lock, gleichzeitig Nullwartezeit
und abschließende Locktiefe null; Send/Receive außerhalb als Kontrollfälle.
Für die Lock-/Queue-Grenze prüft der bisherige Hosttest Nullwartezeit,
aber nicht den Aufrufkontext. Nachgewiesen ist die
Regelabweichung, kein Deadlock oder gemessener Fristverstoß.
Geltung: `src/ProfileRuntimeProbe/main.cpp`, nicht die separate LED-Firmware.

**Übernommen:** Plattformreview trennt Wartezeit und zulässigen Aufrufkontext.
**Nächster Schritt:** Plattformmandat entwirft die Queue-/Authority-Grenze neu;
Widerruf, Epochwechsel, volle Queue und veraltete Antworten müssen weiterhin
korrekt bleiben. Nicht einfach den Lock entfernen. Danach Regeltest mit null
RTOS-Aufrufen unter Lock, gesamte konfigurierte Host-Suite und gesonderter
SMP-/Zeitnachweis. Wiedervorlage: vor produktiver Übernahme dieses Kanals.
Rückweg einer künftigen Korrektur: vorherigen Kanalstand im isolierten Branch
erhalten; diesen Befund dabei ausdrücklich wieder öffnen.

### A1-02 · P1 · SDK-Reihe ohne regulären Support

**Herstellerstatus belegt; konkrete neue Schwachstellenbetroffenheit offen.**
[Espressif AR2024-008, 02.07.2024](https://www.espressif.com/sites/default/files/advisory_downloads/AR2024-008%20End-of-Life%20Advisory%20for%20ESP-IDF%20v4.4%20Release%20Branch%20EN.pdf)
meldet EOL für IDF 4.4 seit Juli 2024, ohne weitere reguläre Fehler-/Securityfixes.
Die bestehende [Migrationsnotiz](../../../../handoff/ARDUINO-ESP32-3-MIGRATION.md)
begründete den späteren Wechsel insbesondere mit RSSI; eine Supportentscheidung
fehlte. Hashpins schließen diese Lücke nicht.

**Übernommen:** EOL und Zuständigkeit in derselben Migrationsnotiz ergänzt.
**Offen:** Plattformmandat bereitet dem Maintainer eine befristete
Weiterbenutzungsentscheidung oder die Einplanung des bestehenden
Migrationsauftrags vor. Wiedervorlage beim nächsten beauftragten
Planungs-/Migrationsabgleich, spätestens vor Produktfreigabe oder Pinänderung.
Keine Zielversion oder Ausnahme ohne Prüfung erfunden; keine Pakete geändert.

Gezielte Advisory-Frage: [GHSA-v335-fxwc-rc44](https://github.com/espressif/esp-idf/security/advisories/GHSA-v335-fxwc-rc44)
(03.09.2026, Classic-Bluetooth-L2CAP) nennt 4.4 nicht in der Versionsmatrix.
Das ergibt bei EOL weder Befall noch Entwarnung. Die lokale SDK-Konfiguration
aktiviert Classic/BTDM; Arduino 2.0.14 `BLEDevice::init` ruft über `btStart`
den entsprechend konfigurierten Controller auf. Nur „App nutzt BLE“ genügt
nicht zum Ausschluss. Binarypfad/Erreichbarkeit gezielt fachlich prüfen.
[BLE-Quelle](https://github.com/espressif/arduino-esp32/blob/2.0.14/libraries/BLE/src/BLEDevice.cpp),
[HAL-Quelle](https://github.com/espressif/arduino-esp32/blob/2.0.14/cores/esp32/esp32-hal-bt.c).
[GHSA-h7r3-gmg9-xjmg](https://github.com/espressif/esp-idf/security/advisories/GHSA-h7r3-gmg9-xjmg)
verlangt dagegen `protocomm_ble` mit `keep_ble_on`; kein solcher Projektaufruf
gefunden, aber kein vollständiger transitiver Linkausschluss durchgeführt.

### A2-01 · P1 · I²C-Fehler können als vier aktive Wake-Eingänge ankommen

**Fehlerkette am Code belegt, Spielwirkung abgeleitet; kein Geräteversuch.**
BusIO 1.17.4 gibt bei fehlgeschlagenem Registerlesen `0xFFFFFFFF` zurück;
Bitextraktion und MCP 2.3.2 `digitalRead` machen daraus HIGH.
[BusIO Register](https://github.com/adafruit/Adafruit_BusIO/blob/1.17.4/Adafruit_BusIO_Register.cpp#L179),
[MCP digitalRead](https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library/blob/2.3.2/src/Adafruit_MCP23XXX.cpp#L94).
`MCPHandler::readGPIO` reicht das weiter; `KeyMatrix::readWakeSignals`
wertet HIGH als aktiv, und `GameState` wechselt bei vier aktiven Eingängen
aus Standby. Ein Busausfall nach erfolgreichem Start kann somit den Übergang
auslösen. `writeGPIO` meldet außerdem Erfolg nach void-Bibliotheksaufrufen,
ohne einen erfolgreichen Transfer zu kennen.

Gilt für `az-delivery-devkit-v4`. Der eigene Baugruppenadapter `McpWakeBus`
prüft NACK, Kurzantwort und negative Reads und verwirft ungültige Wake-Messungen.
Seine Tests entlasten den anderen Adafruit-Pfad nicht.

**Übernommen:** Geräteintegration verantwortet den Fehlervertrag bis zum
Spielverbraucher. **Nächster Schritt:** Legacyadapter um ausdrücklich ungültige
Messung ergänzen; Test mit gepinnten Originalmethoden und injiziertem Transport:
gesunder Start, dann NACK, 0/kurze Antwort, Ausfall nach einem gesunden Read,
nur ein Expander betroffen. Erwartung: kein Standby-Austritt, kein falscher
Schreiberfolg; gesunde LOW-/HIGH-Fälle als Kontrolle. Ein Fake, der direkt HIGH
liefert, beweist die Bibliotheksfehlerkette nicht. Wiedervorlage vor nächster
Legacy-Wake-Freigabe oder Übernahme des Adapters. Rückweg: separater
Adapteränderungsschnitt; Fehlerstatus nicht still auf HIGH zurücksetzen.

### A2-02 · P2 · Parser- und Patchschutz ohne ausführbare Regression

**Prüflücke auf Ausgangsbasis belegt; im PR für Parser und Patchhelper geschlossen.**
Der PN532-Aufruf erreicht tatsächlich die gepatchte Methode; kein Bypass
gefunden. [Upstream-Aufrufkette 1.3.4](https://github.com/adafruit/Adafruit-PN532/blob/1.3.4/Adafruit_PN532.cpp#L512-L524).
Maßstab für Frames: [NXP UM0701-02, Rev.02, §6.2.1.1/§7.3.5](https://www.nxp.com/docs/en/user-guide/141520.pdf).
Die bereits dokumentierte Prüflücke wurde jetzt bearbeitet:

- 15 neue Unity-Tests im vorhandenen `host-regression`: feste 4-/7-Byte-Frames,
  NoTarget/ATS, 79 verkürzte Präfixe, Header/Prüfsummen, Ziel-/UID-/ATS-Grenzen,
  LEN57/58, andere Frametypen und vollständiges Leeren alter Ergebnisse.
  Kapazitätsfälle sind keine Aussage über physisch zulässige ATS-Inhalte.
- Patchschutztests: Anwendung, Wiederholung, Restore, falsche Identität,
  fremder Quelltext, beschädigte/fehlende Sicherung, geänderte Ersetzung,
  CRLF und Erhalt nicht betroffener Inhalte. Synthetische Hashpins prüfen den
  Schutzablauf, nicht den echten Upstream. Reale Hashprüfung separat.
- Vorhandener Quality-Workflow führt die Helfertests ausdrücklich aus;
  Parserfälle laufen automatisch in der bestehenden Host-Suite.

**Verbleibt offen:** tatsächlicher gepatchter Busadapter, reale Karten,
Transportfehler, Resynchronisation und Zeitmessung. Kein neuer Parserdefekt
festgestellt. Geräteintegration hält diese Unterscheidung im NFC-Vertrag;
Qualität prüft Testbezug und verhindert Gleichsetzung mit Geräteabnahme.
Rückweg: neue Testdateien, Aufrufe und CI-Schritt separat zurücknehmen;
dadurch wird die Prüflücke wieder geöffnet, der Produktpatch bleibt erhalten.

### A2-03 · P2 · SPI-Bool und Timeout sind schwächere Zusagen als ihr Name nahelegt

**API-Vertrag belegt; Dokumentation präzisiert.**
BusIO 1.17.4 `write_then_read` liefert auf SPI immer true.
[Versionsquelle](https://github.com/adafruit/Adafruit_BusIO/blob/1.17.4/Adafruit_SPIDevice.cpp#L403).
Der Software-SPI-Pfad erhält daraus keine Gerätequittung; relevant sind
ACK/Ready und vollständige Framevalidierung. Der [NFC-Vertrag](../../../nfc-packet-contract.md)
benennt dies jetzt ausdrücklich.

Die schon bekannte Zeitgrenze bleibt offen: `readCard(100U)` umfasst vorher
SAMConfig, getrennte ACK-/Antwortwartezeiten und anschließend AsTarget.
Das ist keine vollständige 100-ms-Deadline. Geräteintegration/Plattform müssen
den ganzen Pfad einschließlich Fehler-/Abbruchfällen messen. Weitere
Adaptertests mit erfolgreichem SPI-Bool und korrupten/Null-/FF-Antworten,
I²C-NACK und HSU-Teilread bleiben vor jeweiliger Adapterfreigabe erforderlich.

## Erfüllte Regeln, bekannte Fälle und Quellenlücken

- `GameLightEffects` verwendet Stackgrößen in Bytes, begrenzt die Arbeit pro
  Frame und gibt bei verpasster Frist Rechenzeit ab. Kein Vanilla-FreeRTOS-
  Wortmaßstab unterstellt; reale Stack-/Zeitreserve weiter offen.
- ESP-NOW-Callbacks geben begrenzt Arbeit an den Worker; kein pauschaler
  `FromISR`-Wechsel. RMT hält seinen Puffer bis TX-Abschluss.
  [ESP-NOW-Kontext](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/network/esp_now.html),
  [RMT-Vertrag](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/peripherals/rmt.html).
- Die geöffneten Releaseübersichten führen PN532 1.3.4, MCP23017 2.3.2 und
  BusIO 1.17.4 als neueste Versionen. Keine Updatepflicht allein aus Alter.
  [PN532](https://github.com/adafruit/Adafruit-PN532/releases),
  [MCP](https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library/releases),
  [BusIO](https://github.com/adafruit/Adafruit_BusIO/releases).
- B7/IO7 ist ein bekannter Hardwarefall, kein neuer Fund. Wake 0x20/B7 und
  Matrix 0x21/B7 getrennt weiterführen. Der vollständige aktuelle Microchip-
  Datenblattabruf scheiterte zweimal, Supportseite lud nicht vollständig.
  Keine neue Volltextprüfung behauptet; Chipmarking/physische Zuordnung offen.
- Alle verlinkten Primärquellen wurden am 23./24.10.2026 geöffnet, soweit
  kein Zugriffsfehler ausdrücklich genannt ist. Revisionsgebundene Quellen
  bevorzugt; Releaseübersichten sind nur ein datierter Abrufstand.

## Ausgeführte Prüfungen und nächste Entscheidung

Die [lokalen Ergebnisse](pruefungen.md) nennen die tatsächlich ausgeführten
Prüfungen samt Aussagegrenzen. Die RTOS-Probe ist ein datierter
**Reproduktionsbeleg des Fehlers**, kein grünes Compliance-Gate; sie wird
nicht als dauerhafter Test in die CI aufgenommen. Wiederholung:

```powershell
python docs/quality/audits/2026-10-24-pilot/rtos-probe/run_probe.py --repo . --compiler <g++-Pfad> --result <neuer-Ergebnispfad.json>
```

Die zwei Mandate waren im Pilot nützlich: zwei konkrete Grenzverletzungen
zugeordnet, eine bekannte Prüflücke ausführbar geschlossen, Supportstatus und
SPI-Erfolgssemantik in bestehende Fachreferenzen übernommen. Langfristiger
Nutzen, Aufwand und Schedulerzuverlässigkeit sind noch nicht gemessen.
Eine zweite unveränderte Recherche zur Dubletten-/Wiedervorlageprüfung wurde
nicht durchgeführt. Vor automatischem Betrieb bleibt dieses Erprobungstor
des ursprünglichen Entwurfs erhalten. Laufzeitkorrekturen erhalten getrennte
Arbeitspakete; Rollen und Prüfergänzungen sind in PR #95 reviewbar.
