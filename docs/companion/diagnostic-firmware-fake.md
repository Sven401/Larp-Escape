# Diagnose v1: schmaler C++-Owner mit Fake-Ports

Stand: 10. Oktober 2026. Status: hostprüfbare Firmwareausführung auf
Vertragsbasis `3ef678167c21d34f88aadebdcb1c2d1d94cc7b30` (F0 `e973818`).
Maßgeblich bleibt [diagnostic-v1.md](diagnostic-v1.md). Dieses Paket enthält
keinen Geräteadapter, JSON-Decoder, BLE-Endpunkt oder Upload.

`CompanionIntegration/DiagnosticControl` führt einen Wake-Erfassungsschritt auf
logischem Kanal 1 und einen LED-Schritt auf logischem Index 0/RGB `[16,0,0]` für
500 ms aus. `IPort` ist die einzige Komponentenverbindung; der Hosttest stellt
ein synthetisches Portmodell bereit. Es gibt keine GPIO-, Bus-, LED-Chip- oder
physisch bestätigte Polaritätsannahme. Der Owner hat keine Verbindung zur
Spiel-Engine und erzeugt keine Spielbefehle oder Nutzerurteile.

## Rechte und Besitz

Die Freigabe benötigt ausdrückliche Zustimmung und den aktuellen tatsächlichen
`IdentityAccess.route`-Beleg. Der Owner kopiert ursprüngliche Verbindung und
volle Authority; er kontrolliert beides bei jedem Fortschritt erneut. Die
Freigabe umfasst Lauf, geräteseitig erzeugte UUID-v4-Freigabekennung und Boot;
die Lebenszeit ist 1..60000 ms. Ein neuer Lauf benötigt eine neue Kennung.
Nach 128 Läufen pro Boot verweigert der Owner weitere Freigaben, statt seine
begrenzte Historie zu vergessen. Das Objekt darf innerhalb desselben Boots
nicht neu angelegt werden, um diese Sperre zu umgehen.

Der spätere Komponentenport muss das ruhende Spiel und exklusive Besitzerrechte
atomar bestätigen, Spielstart sperren und Testeingaben vom Spiel fernhalten.
Der Caller muss Diagnoseowner, IdentityAccess, Komponentenaktionen und
Lebenszyklusänderungen gemeinsam serialisieren. Die reine `route()`-Prüfung
belegt keine atomare Produktionsaufnahme. Der Port wird nach der Aufnahme auf
einen unveränderten Rechtebeleg geprüft; dieser Hostschutz ersetzt keine
produktive Synchronisation. Eine sichtbare Geräte-/App-Testanzeige bleibt offen.

## Durchführung und Grenzen

Nur ein Schritt und eine noch nicht abgeholte Endantwort sind gleichzeitig
zulässig. Eine angenommene Schrittkennung wird vor dem Komponentenaufruf
verbraucht. Sie wird auch bei Portfehler, verlorener Antwort oder Unterbrechung
nicht erneut ausgeführt. Nach UINT32_MAX gibt es keinen Wrap. Der Schritt muss
vollständig in die verbleibende Freigabe passen; Zeitangaben sind monotone
Bootmillisekunden bis 2^53−1. Rückwärtszeit oder Überschreitung sperrt den Owner
bis zum Neustart.

Wake-Flanken enthalten Rohpegel, deklarierte aktive Polarität, Kanal und Zeit.
Alle fremden Kanäle 1..4 bleiben erhalten; der feste Puffer fasst 64 Flanken
und meldet weiteren Anfall mit `overflow=true`. Ein unbekannter Kanal ist
ein Ownerfehler. `finishWake` ist ein **vertrauenswürdiges Komponentenereignis**,
kein zusätzlicher Clientbefehl. Die reale Entscheidung zum Erfassungsabschluss
muss im nächsten Adapterpaket festgelegt werden; Fristablauf ersetzt dieses
Ereignis nicht durch Erfolg. Ein `completed`-Ergebnis kann falsche oder keine
Flanken enthalten; die App bewertet den Gerätebeleg und die Nutzerbeobachtung.

LED-Ausgabe und Aus-Rückkehr werden getrennt aufgerufen und gezählt. `tick`
beendet den Reiz nach 500 ms; Lebenszyklusende beendet ihn sofort. Eine
fehlgeschlagene Aus-Bestätigung meldet `failed/owner_error`, sperrt weitere
Diagnosen und **hält den Komponentenbesitz gegen Spielstart fest**. Der Owner
versucht keine automatische Wiederholung. Sichere Wiederherstellung und
Entsperrung gehören in eine separat belegte Produkt-Recovery. Spannung ist in
diesem Port nicht verfügbar und wird später als JSON `null` abgebildet.

Abbruch, Disconnect, Retirement, Rechte-/Verbindungsgenerationswechsel oder
Freigabeablauf schließen die Freigabe und erzeugen für einen aktiven Schritt
eine korrelierte terminale Antwort. Schrittfristablauf ist `rejected/expired`;
die App hält ihren getrennten Unterbrechungszustand `timeout` und ein unbekanntes
Ausführungsergebnis. Nach verlorenem Link wird kein lokaler Reply als empfangener
Gerätebeleg behauptet. Die Antwortablage wird nur für die ursprüngliche
Verbindung abgeholt; vor Transportausgabe ist deren aktuelle Authority nochmals
zu prüfen. Ein vertrauenswürdiger Adapter muss eine auf einem verlorenen Link
verbliebene Antwort ausdrücklich abholen/verwerfen, bevor er einen neuen Lauf
freigibt. Er darf sie keinem Ersatzlink zustellen.

## Reproduzierbare Prüfung

`python scripts/check_diagnostic_control.py` kompiliert den tatsächlichen Owner,
`IdentityAccess`, `Identity` und `AuthorityState` mit C++11, `-Wall -Wextra
-Wpedantic -Werror` und führt zehn Host-Szenarien aus. Der Runner nutzt den
vorhandenen MinGW-Compiler wie der öffentliche Headercheck. Die Testfälle
prüfen Zustimmung/Rolle/Spielruhe, Kennungen und Historienbudget, Korrelation,
Fremdflanken/Polarität/Überlauf, LED-Dauer und Doppelanforderungen, ungültige
Parameter, Abbruch/Disconnect/Entzug/Ablauf, Owner-/Uhrenfehler,
Verbindungsgeneration, maximale Schrittkennung und Rechtewechsel in acquire.

Zusätzlich gelten `check_architecture.py`, `check_headers.py`, `check_format.py`,
`check_diagnostic_contract.py` und der Produktions-Zielbuild. Das Testverzeichnis
ist Teil der Formatbaseline. Der Produktionsbuild kompiliert den neuen Owner,
instanziiert oder verbindet ihn jedoch nicht mit echter Hardware.

Gemessener Stand am 10. Oktober 2026: zehn C++-Szenarien/358 Checks bestanden;
Ownergröße im 32-Bit-MinGW-Host 5376 Byte (kein gemessener ESP32-Taskstack).
Architekturcheck 147 Dateien/307 Includes; 87 unabhängige öffentliche C++11-Header;
Formatbaseline 312 Dateien mit clang-format 19.1.7, zuletzt ergänzte Testdatei
anschließend nochmals gezielt geprüft. Alle 22 v1-Vertragsfälle inklusive acht
Goldenberichten bestanden mit vorhandenem Python 3.11.7/jsonschema 4.23.0.
Produktionsbuild `az-delivery-devkit-v4` erfolgreich: Flash 1229077/1310720 Byte,
RAM 55148/532480 Byte. Dabei bestehen unveränderte Upstream-Warnungen in
FastLED und Arduino-UART. Kein Upload, Gerätetest oder physischer Befund.

## Nächste Anschlussbedingungen

Der auf dieser F0-Basis vorhandene Produktionsstart `src/main.cpp` initialisiert
`ReichstagGame/LarpEscapeApp` und die Hardwareadapter. Er instanziiert keinen
aktiven `IdentityAccess`-/GATT-Owner. Die direkte BLE-/Auth-Verkettung liegt in
`src/GattsAbiProbe/main.cpp` als ausdrücklich synthetische, ungeöffnete Probe.
`GattsServiceDefinition` kennt Command, Profile und Peer-Auth-Endpunkte;
`CommandFrame` reserviert Framekind 1 für GameCommand v2 mit 8 Byte Header und
120 Byte Payload. Es gibt dort keine Diagnoseaufnahme oder Diagnosefreigabe.
Ein Anschluss an diese Probe würde keinen App-bedienbaren Produktpfad schaffen.

Der nächste gemeinsame Firmware-/Android-Schritt benötigt daher eine getrennte
Diagnose-Characteristic oder ein ausdrücklich neu reserviertes Framekind mit
festen Bytebudgets und Freigabe-/Abbruchsemantik. Die vorhandene 128-Byte-Grenze
darf nicht durch unbegrenztes JSON ersetzt werden. Anschließend ist derselbe
originalgebundene Auth-/Lifecycle-Owner im Produktionsstart einzurichten und mit
dem atomaren Diagnose-/Spielbesitz zu verbinden. Solange Hardwareprofil und
Besitzübergabe fehlen, darf der Transport ausschließlich als klar synthetischer
Fakepfad angeboten werden und keine reale LED-Fähigkeit behaupten.

- Ein begrenztes Diagnoseframing samt Parserbudgets, Fähigkeiten-, Freigabe- und
  Abbruchantwort sowie originalgebundener Aufnahme/Antwort; GameCommand v2
  bekommt keine umgedeuteten Opcodes.
- Serialisierter Identity-/Lifecycle-Fence und atomare Besitzübergabe aus
  `DeviceRuntime/Plan`, inklusive garantiert bedientem Timer, sichtbarer
  Testanzeige und ungeklärter Aus-/Transportfehlerbehandlung. `tick` im Host
  beweist weder reale Fristbedienung noch den physischen LED-Zustand.
- Geprüftes Hardwareprofil und Hardwarezuständigkeit für Wake-Zuordnung,
  Polarität, LED-Typ/Index, Versorgung und elektrisches Reizbudget; bestehende
  GPIO15/WS2811 versus G4/DATA-Befunde bleiben offen.
- Android muss Auftrag vor Sendung persistieren, Benutzeraktion und Beobachtung
  eigenständig führen sowie den v1-Bericht verlustfrei exportieren. Der separate
  Android-Fakekern und dieser C++-Owner sind noch kein gemeinsamer GATT-Pfad.

Die physische Abnahme bleibt unterbrochen. Ein nächster Hardwarelauf wird erst
nach geprüfter eigenständig bedienbarer App-/Firmwarediagnose vom Nutzer in
der App ausgeführt und anschließend in einer neuen Sitzung ausgewertet.
