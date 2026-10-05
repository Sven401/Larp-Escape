# ESP32 ohne Zusatzhardware

Das Profil `esp32-runtime-probe` ist ausschließlich für ein ESP32-Board am USB vorgesehen. Es richtet keine externen GPIOs, I2C-/SPI-Geräte, LEDs, Audio oder Funkkommunikation ein. Die normale Spiel-Firmware startet ohne ihre Geräte nicht erfolgreich; deshalb ist dieses Profil getrennt.

```powershell
pio run -e esp32-runtime-probe
pio run -e esp32-runtime-probe -t upload --upload-port COM3
pio device monitor -p COM3 -b 115200
```

Der Upload ersetzt die bestehende Firmware. Vor dem ersten Upload wurde für das angeschlossene Board ein lokales vollständiges Flashbackup erstellt; dieses gehört nicht in Git und kann vorhandene Gerätekonfiguration enthalten. Der tatsächliche Upload muss ausdrücklich freigegeben sein.

## Prüfinhalt

- Chip, Revision, Kerne, Takt, Flash und freier/minimaler Heap.
- Sechs vollständige Rollenrunden und eine Aktivierung mit Fake-Ports auf dem echten ESP32-Prozessor. Es werden keine echten Karten gelesen und keine Funkpakete gesendet.
- Vollständige Wake-Zeitpolitik einschließlich Uhrüberlauf; keine elektrischen Wake-Impulse.
- Zwei FreeRTOS-Tasks auf unterschiedlichen Kernen: absichtliche Queueüberlast (Kapazität4), begrenztes Abarbeiten und atomare Zähler.
- Produktions-Statusmailbox unter parallelem Schreiben/Lesen. Verknüpfte Statusfelder prüfen, ob eine vollständige Wertkopie gelesen wurde.
- Produktions-JSON-Serialisierung auf dem Zielprozessor. Alle fünf Sekunden erscheinen Messwerte und synthetischer Status über USB.

**Erwartung:** Beide SELFTEST-Felder PASS; `failures=0`; erfolgreiche Queue-/Mailboxaktivität; Drops sind hier absichtlich erzeugt. Stack- und Heapreserve werden protokolliert. Ein längerer Lauf prüft Watchdog-/Neustartverhalten.

Dies ersetzt keine Prüfung echter LEDs, NFC-, MCP- oder Audioadapter, Funkzustellung oder elektrischen Timings. Die Queuebelastung ist synthetisch und liefert keine reale LED-Framerate. Der veröffentlichte Rollen-/Rundenstatus ist ein Testmuster.

## Verifizierter Erstlauf am 5. Oktober 2026

ESP32-D0WD-V3, Revision3, zwei Kerne, 240MHz, 4MB Flash; USB-UART COM3. Der Nutzer hat den Upload des Diagnoseprogramms ausdrücklich freigegeben. Es wurde erfolgreich geschrieben und die Upload-Hashes wurden vom Werkzeug geprüft. Quellstand: Companion-/Probe-Schnitt `bf23da0`; danach wurden nur Formatierung und Qualitätsdokumentation abgeschlossen.

- `SELFTEST game_six_roles=PASS wake_rollover=PASS`.
- Erfasste Laufzeit bis135000ms: 215456 Queueversuche, 16324 angenommen, 199132 absichtlich verworfen, 16320 verarbeitet, 4081 Statuskopien geprüft, **0 Fehler**.
- Freier Heap blieb über die erfassten Berichte bei343260Bytes; bisheriges Minimum336428Bytes.
- Minimale freie Taskstacks: Producer2540Bytes, Consumer2560Bytes, jeweils3072Bytes zugeteilt.
- Gültige v1-JSON-Dokumente wurden über USB ausgegeben. Kein Neustart innerhalb der erfassten Abschnitte; keine Langzeitfreigabe daraus ableiten.
- Probe-Build: RAM21480Bytes, Flash291673Bytes. Separater Spiel-Firmware-Build: RAM55156Bytes, Flash1224553/1310720Bytes (93,4%).

Diese Taskreserven gehören zum Probeprogramm, nicht zum Produktions-LED-Task. Die Vollhardwareprüfung bleibt offen. Das Board bleibt mit dem Diagnoseprogramm versehen.

## Lokale Rücksicherung

**Nur auf diesem Rechner:** `C:\Users\seide\AppData\Local\LarpEscape\ESP32-Backups\COM3-20261005\original-flash.bin`, exakt4194304Bytes. SHA256: `62E2B21B1C2A4024CE6AA95EA750A77C89624D20A3F288A0776D9D2A013C46DA`. Die Sicherung liegt dauerhaft lokal außerhalb von Git, Temp und OneDrive. Vor einem Restore Chip/Flashgröße und COM-Port erneut prüfen und den Nutzerauftrag beachten.

```powershell
& 'C:\Users\seide\.platformio\penv\Scripts\python.exe' 'C:\Users\seide\.platformio\packages\tool-esptoolpy\esptool.py' --chip esp32 --port COM3 --baud 460800 write_flash 0x0 'C:\Users\seide\AppData\Local\LarpEscape\ESP32-Backups\COM3-20261005\original-flash.bin'
```

Ein Restore wurde nicht ausgeführt. Die Referenzlogs der ersten beiden Erfassungen liegen lokal unter `AppData\Local\Temp\larp-usb-probe-first-run.log` und `larp-usb-probe-long-run.log`; die verifizierten Ergebnisse stehen dauerhaft hier.
