# R1 drucken und Slicerprüfung reproduzieren

Stand: 31.10.2026. Digitale Druckvorbereitung mit **Bambu Studio 2.8.2.61**;
noch kein tatsächlicher Druck. Alle sechs STLs wurden mit Rückgabecode 0 und
ohne Slicerwarnung verarbeitet. `slice-report.json` bindet STL, 3MF und den
darin enthaltenen G-Code über SHA-256 an diesen Lauf.

## Direkt verwenden

Die sechs Projekte unter `print/` enthalten jeweils eine Druckplatte für
**Bambu Lab A1 mini, 0,4-mm-Düse, Textured PEI Plate**. In Bambu Studio öffnen,
Drucker und tatsächlich eingelegtes Material abgleichen, Vorschau prüfen und
zuerst `coupon.3mf` drucken. Anschließend gemäß [Montage](MONTAGE.md) vorgehen.
Bei anderem Drucker, anderem Material oder geänderter Geometrie neu slicen.
Die Projekte wurden nicht an einen Drucker geschickt.

| Teil | Material | Slicerzeit, gerundet | Material, gerundet |
| --- | --- | ---: | ---: |
| base | Bambu PETG HF | 6 h 48 min | 291,8 g |
| tray | Bambu PETG HF | 50 min | 21,4 g |
| lid | Bambu PETG HF | 1 h 30 min | 61,4 g |
| ring_holder | Bambu PETG HF | 26 min | 8,3 g |
| diffuser | Bambu PETG Translucent | 29 min | 8,3 g |
| coupon | Bambu PETG HF | 18 min | 5,2 g |

Zusammen etwa **10,4 Stunden und 396 g je Station**, einschließlich einer
Passprobe. Aufheizen, Handarbeit, Druckerabweichungen und Fehldrucke können
den realen Aufwand erhöhen. Vier Wände, 20 % Gyroid, fünf Boden-/Decklagen,
0,20 mm Schichthöhe, kein Support und kein Brim sind eingestellt.
Die transparente Scheibe ist optisch noch nicht erprobt.

## Reproduzierbarer Windows-Prüfweg

1. Offizielles [Bambu Studio 2.8.2.61](https://github.com/bambulab/BambuStudio/releases/tag/v02.08.02.61)
   als Windows-Portable entpacken nach `.tmp/cad-tools/bambu-studio/` im
   Repository. Darin müssen `BambuStudio.dll` und `resources/` liegen.
2. Für den hier geprüften Aufruf Python 3.12 x64 verwenden. Aus dessen
   Installationsordner `python.exe` als `python-cli.exe` und `python312.dll`
   neben die BambuStudio-DLL kopieren. Das ist ein lokaler Werkzeugaufbau;
   diese Binärdateien gehören nicht ins Repository.
3. Im Repository mit dem normalen Python 3.12 ausführen:

```powershell
python Hardware/naehe-glut/tools/prepare_slicer_profiles.py .tmp/cad-tools/bambu-studio Hardware/naehe-glut/mechanik/slicer
python Hardware/naehe-glut/tools/slice_enclosure.py
```

Der Wrapper ruft den unveränderten exportierten Einstieg `bambustu_main`
der offiziellen DLL auf; die GUI-Startdatei lieferte in dieser Umgebung
keine verwertbare CLI-Ausgabe. Das daneben liegende Python ist nötig, damit
die Engine ihre Ressourcen findet. `slice_enclosure.py` setzt `PYTHONHOME`
auf die Python-Installation und verarbeitet die Teile seriell. Laufprotokolle,
G-Code und native Ergebnisdateien liegen unter `.tmp/naehe-glut-sliced/`.
Keine Druckerbindung, Anmeldung oder Übertragung wird ausgeführt.

## Herkunft der Profile

Die JSON-Dateien unter `slicer/` stammen aus den offiziellen BBL-Profilen
von BambuStudio, Tag `v02.08.02.61`; Lizenz AGPL-3.0, siehe beiliegende
`LICENSE-AGPL-3.0.txt`. Die Originalautoren und Lizenz bleiben erhalten.
`profile-sources.json` nennt Quell-URL und SHA-256 jeder eingelesenen Datei.
Am 31.10.2026 wurden Vererbung/Includes für den CLI-Lader aufgelöst und die
oben genannten Druckeinstellungen durch `prepare_slicer_profiles.py` geändert.
Die bearbeitbaren JSON-Profile und der Transformationscode liegen vollständig
bei. Dies ändert keine Lizenz der unabhängigen CAD-Konstruktion.
