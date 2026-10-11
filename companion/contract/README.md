# Companion-Verträge und Referenzfixtures

Die native Android-App verwendet den v2-Erwartungsstand unter
[firmware-v2/](firmware-v2/SNAPSHOT.md). Die zwölf Dateien stammen unverändert
von Firmwarecommit `c9654c3c1365f4ada2dd5b6ce82bc5ac9373459b`;
Herkunft und Prüfsummen stehen im Snapshotdokument.

```powershell
node companion/contract/scripts/verify-firmware-v2-snapshot.mjs
```

Die Prüfung bestätigt die Kopie, keine Gerätefreigabe oder Hardwarefunktion.
Vor Produktintegration aktuelle Firmware und Fixtureinhalt erneut vergleichen.
Der [Laborrahmen](lab-v2/README.md) ist eine eigene Debug-Schnittstelle.
Das [NFC-v1-Testformat](nfc-v1/README.md) ist ein Vorschlag für Tests;
ein gemeinsamer ESP-Spielobjektvertrag bleibt offen.

Historische v0-Mockbefehle sind nicht der v2-Android-Wirevertrag und gehören
nicht zu diesem Android-Paket. Szenarioregeln bleiben gezielte
[Referenzfunktionen](../app/README.md). Weitere Grenzen stehen in
[OPEN-DEPENDENCIES.md](../OPEN-DEPENDENCIES.md).
