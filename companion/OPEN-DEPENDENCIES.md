# Companion: offene Abhängigkeiten

Stand 2026-10-10. Der kuratierte Androidstand ist eine Entwicklungsbasis.
Quellenherkunft und Abgrenzung stehen in [SOURCE.md](SOURCE.md).

## Implementiert und am Rechner prüfbar

BLE-Suche und Dienstprüfung, Debug-Laborstatus mit strikt typisiertem Decoder,
Boot-/Samplefolge, vorläufiger 31-Sekunden-Aktualitätsgrenze, Berechtigungs-
und Vordergrundsitzungsregeln. Lokale JSON-v1-Szenarien unterstützen Editor,
Leseprobe, atomare Speicherung, Import und Export. Die eingebundene NFC-Seite
verwendet das vorläufige GM-Testformat und behandelt unklare Schreibausgänge.
JVM-Tests, Builds und Lint belegen ausschließlich den jeweils genannten Umfang.

Historische Handybelege sind in der Android-Anleitung mit Grenzen beschrieben;
sie werden mit diesem Paket weder kopiert noch erneut durchgeführt.

## Kanonischer Firmwarezugang

Produktiver Peer-Dienststart, Bond-Wiederverbindung, vertrauenswürdige Einrichtung,
gegenseitiger Gerätenachweis und private Schlüsselverwaltung sind noch offen.
Die App erkennt den Produkt-GATT-Dienst, führt dort bisher keine Anmeldung,
keinen aktuellen Produktstatus und keine Spielbefehle aus. Öffentliche Lease-Werte,
Frames und lokale Dokumente bestätigen weder Rechte noch ausgeführte Aktionen.

Ein späterer Befehlspfad muss boot/lease/request und Revisionen binden,
korrelierte Antworten prüfen und unklare Ergebnisse sichtbar lassen. Ein
Engine-Ergebnis allein bestätigt keine physischen Gerätewirkungen.

## Noch ausstehende Abnahmen

Echter ESP-Ausfall und Statusstillstand, alte native Rückrufe, weitere Android-
modelle und Dauerbetrieb. NFC: geänderte Daten und Fehlerfälle, anderer Tag,
Schreibschutz, Tagverlust, Prozessabbruch und Mehrrecord-Erhalt am echten Tag.
Der GM-Testvertrag ist kein produktiver ESP-Spielobjektvertrag.

Synthetischer Vite-Export/Android-Import wurde historisch geprüft. Tatsächlich
vorhandene Browserentwürfe sind noch nicht als migriert belegt. Vor Stilllegung
exportieren und mit IDs, Reihenfolge und Inhalten in Android prüfen.

Nutzergeführte Vollhardwarediagnose mit korreliertem Geräteergebnis,
Nutzerbeobachtung und exportierbarem Endbericht ist noch nicht implementiert.
Release-Signierung, Verteilung, Produktsteuerung und Hardwarefreigabe bleiben offen.
