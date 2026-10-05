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
