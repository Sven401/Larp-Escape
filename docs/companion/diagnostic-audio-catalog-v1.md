# Diagnostic audio catalogue v1 — source-backed, not device-ready

**Status (2026-10-12):** software catalogue and bounded direct-track selector.
Neither the catalogue nor a successful firmware build proves that a file is on
the box SD card, that the DFPlayer accepted a command, or that sound was heard.
The diagnostic audio owner, protected request/reply route, Stop/Repeat/Volume,
and separate BUSY/UART/error observations are not implemented. Audio actions
remain blocked in the Android readiness view.

The firmware-side catalogue is `DiagnosticAudioCatalogV1` (42 entries). Its
Reichstagsgame entries are read from the current `defaultRoleDefinitions()`;
the source is `src/GameDomain/GameDefinitions.cpp` in this product branch.
Each role has its named closing track and three named crystal tracks:

| Role | Closing | Crystal tracks |
| --- | ---: | --- |
| Puppenspieler:in | 230 | Spinne 132; Marionette 131; Lila 130 |
| Krieger:in | 210 | Schild 112; Rot 111; Löwe 110 |
| Narr / Närrin | 220 | Käfig 120; Orange 121; Rabe 122 |
| Abenteurer:in | 200 | Seil 102; Grün 101; Fuchs 100 |
| Überlebende:r | 250 | Schwer 150; Weiß 151; Ratte 152 |
| Wissende:r | 240 | Buch 141; Blau 140; Pfau 142 |

The Star-Wars-Con entries come from the existing Git ref
`origin/Starwarscon` at `540aeec1362c2d29a3f28267a0abf3b794cf8c1b`,
`lib/Config/OptionDefinitions.cpp` and `lib/Config/OptionConfig.h`:

| Group | Tracks in source order |
| --- | --- |
| Quail | Button1 101; Button2 103; Button3 109; closing 110 |
| SecRinsui | Button1 204; Button2 206; unlabelled source item `undefined` 207; closing 210 |
| `|||` | Button1 302; Button2 305; Button3 308; closing 310 |
| Game | Bonus 900; error variants 995, 996, 997, 998, 999 |

Track numbers are namespaced by source and group. For example, track 101
occurs in both branches; a numeric ID alone does not identify a clip. The
direct diagnostic input is provisionally bounded to 1–999 rather than the
entire 16-bit driver type. This is a software selection rule, not a statement
about the module's full supported range or the SD inventory. The bound needs
an explicit device contract before a direct number can be sent.

Host tests compare all Reichstag entries to the live role definitions, check
the Star-Wars-Con track sequence and provenance, distinguish duplicate numbers
across sources, and reject out-of-range direct selections. Firmware compilation
checks that the catalogue source builds for the ESP32. No GATT permission,
diagnostic grant, DFPlayer operation, pin, or SD file is changed by this slice.
