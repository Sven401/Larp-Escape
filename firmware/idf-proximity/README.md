# Autonomer Näheglut-Prüfstand für zwei ESP32-S3

Diese isolierte native Testfirmware verbindet zwei fest eingerichtete
Waveshare ESP32-S3-Zero. Nach USB-Paarung und bewusster Freigabe arbeitet jedes
Gerät mit eigener Stromversorgung ohne Handy, Rechner, WLAN-Router oder Zentrale.
Es zeigt Näheglut aus frischen Funkantworten seines Partners. Dieser erste
Schnitt erzeugt keine Rätselerfolge oder dauerhaften Begegnungs-Spielaktionen.

Der gemeinsame [NearGlow-Kern](../led/LIVE-NEARGLOW.md) berechnet den Verlauf;
drei explizite `std::uint32_t`-Typangaben beheben nur die Portabilität des
Minimumvergleichs auf dem nativen S3-Compiler, ohne den Algorithmus zu ändern.
Ein lokaler Anwendungseigentümer führt Funkbewertung, Effekt und
genau einen nativen RMT-Ausgabetreiber. Die eingebaute einzelne RGB-LED liegt
an **GPIO21**, Reihenfolge RGB; Arbeitsfarbe `(24,8,0)`, Gain 1000,
Smoothstep über 1000 ms und maximal 32 je Kanal. Audio, Wake, NFC, Matrix
und BLE werden nicht gestartet. Die frühere Handy-Testfirmware bleibt ein
eigenes Ziel und wird durch einen Build nicht ersetzt.

## Verhalten ohne Handy

| Zustand | Wirkung |
| --- | --- |
| Ungepaart oder nicht freigegeben | Dunkel; keine automatische Effektfreigabe. |
| Eingerichtet und bewusst mit ARM freigegeben | Neue passende Funkantworten starten einen lokalen Näheglut-Lauf. |
| Mehr oder weniger gültige Funknähe | Der bestehende Lauf geht weich zum neuen Helligkeitsziel über. |
| Seit 3000 ms keine neue gültige Antwort | Eingang wird ungültig; Glut blendet innerhalb der eingestellten Sekunde nach null. |
| Partner kehrt zurück | Frische Antworten erholen den noch aktiven Lauf weich; kein neuer Start je Messprobe. |
| STOP | Weitere Updates gesperrt und Glut ausblendend; Erfolg der dauerhaften Sperre und tatsächliches Schwarz werden getrennt geprüft. |
| Neustart nach gespeichertem ARM | Alter Messwert bleibt unbekannt; erst neue geprüfte Antworten dürfen einen neuen Lauf starten. |
| Neustart nach erfolgreich gespeichertem STOP | Bleibt gesperrt, bis erneut bewusst ARM gegeben wird. |

Das vorhandene RSSI-Arbeitsmodell dient nur einer groben Funknähe:
Probe etwa alle 500 ms, Q8-Glättung mit 25 % neuem Anteil und
`level = clamp(20 × (RSSI + 90), 0, 1000)`. Keine Meterkalibrierung und kein
20-%-Funkeln-Fallback. Ausrichtung, Körper, Gehäuse und Hindernisse gehören in
den späteren Zwei-Geräte-Test; beide Richtungen können verschieden reagieren.

## Paarbindung und Daten

Beide Geräte verwenden dieselbe Firmware. Die USB-Einrichtung ordnet ihnen
gegenseitig die frisch festgestellten STA-MACs und ein neues gemeinsames
32-Byte-Paargeheimnis zu. Daraus werden getrennte Schlüssel abgeleitet.
Verschlüsselter ESP-NOW-Unicast auf festem Kanal 1 und eine zusätzliche Applikations-HMAC binden
Pakete an das Paar, Absender/Ziel und den Boot-/Challenge-Kontext. Nur eine
passende neue Antwort darf die Frische erneuern. Alte, doppelte oder fremde
Antworten sowie Queuedaten mit abgelaufener lokaler Empfangszeit tun dies nicht.

Dies ist ein begrenzter Paarprüfstand, keine Implementierung der allgemeinen
Veranstaltungsrechte, D0-Nachrichten oder P0-Spielpakete. MAC allein ist keine
Berechtigung. USB und physischer Flashzugriff bleiben vertrauenswürdige
Einrichtungswege; Geheimnisdatei und Flashabbilder müssen privat bleiben.

Die vorhandene [4-MiB-Partitionstabelle](../led/partitions.csv) wird unverändert
referenziert. Konfiguration liegt im eigenen NVS-Namespace `prox_lab`.
Es gibt kein automatisches Löschen oder Reparaturformatieren von NVS und
keine OTA-Rollback-Zusage. Unlesbare oder fehlerhafte Konfiguration wird nicht
durch eine neue Identität oder einen Standardschlüssel ersetzt.

**Speicherfehler beim Stop:** Die laufende Ausgabe wird sofort gesperrt.
Dauerhafter STOP wird nur nach bestätigtem Commit und Rücklesen gemeldet.
Scheitert die Speicherung, kann nach Stromverlust eine frühere gespeicherte
ARM-Freigabe wieder erscheinen; der Fehlerstatus bezeichnet die dauerhafte
Neustartberechtigung dann als unbekannt. Ein Fehler darf weder als erfolgreicher
dauerhafter Stop noch als bestätigte physische Dunkelheit ausgegeben werden.

## Build und Rechnerprüfungen

Hersteller-SDK: ESP-IDF 6.1 am bereits qualifizierten exakten Wartungscommit
`9a97f6c54ec638111ce55cd36581b3c192f15207`, einschließlich seiner Untermodul-
und Werkzeugversionen. Kein Arduino und kein neu gewählter SDK-Pin.
Nach Aktivierung dieser Umgebung aus dem Repository:

```powershell
python scripts/check_idf_sources.py "$env:IDF_PATH"
idf.py -C firmware/idf-proximity -DIDF_TARGET=esp32s3 build
python test/test_proximity/run.py
python -m unittest discover -s scripts/tests -p test_provision_s3_nearglow.py -v
```

Der Build beschreibt kein Gerät. Vor einem gesondert beauftragten Upload
beide Chips/Flashgrößen neu identifizieren, vollständige Sicherungen lesen
und am jeweiligen Chip verifizieren. Partitionen und Flashadressen mit dem
tatsächlichen Build abgleichen. Original-ESP32- oder ATmega-Abbilder nie auf
den S3 übertragen. Zwei S3 wurden inzwischen chipgebunden identifiziert,
gesichert und installiert: [Geräteeinrichtung vom 31.10.2026](../../docs/led/s3-nearglow-pair-2026-10-31.md).

## Einmalige USB-Einrichtung

Der Hosthelfer benötigt Python und für tatsächliche USB-Verwendung PySerial.
Er flasht und löscht nichts. Die hier verwendeten Platzhalter für Ports,
frisch bestätigte MACs und privaten Dateipfad sind bewusst zu ersetzen.
Die Paarungsdatei muss neu sein und außerhalb des Repositorys in einem
bereits vorhandenen privaten Verzeichnis liegen. Ihr Inhalt wird nicht angezeigt.

```powershell
python scripts/provision_s3_nearglow.py info --port PORT_A --mac MAC_A
python scripts/provision_s3_nearglow.py info --port PORT_B --mac MAC_B
python scripts/provision_s3_nearglow.py pair --port-a PORT_A --mac-a MAC_A --port-b PORT_B --mac-b MAC_B --output PRIVATE_PAIR_JSON
python scripts/provision_s3_nearglow.py arm --port-a PORT_A --port-b PORT_B --pair-file PRIVATE_PAIR_JSON
```

`pair` prüft beide Identitäten vor der Änderung, legt ein neues Paargeheimnis
privat ab, konfiguriert beide Geräte zunächst gesperrt und startet sie zum
Übernehmen der Paarung neu. Erst passende Statusantworten beider Geräte
schließen die Einrichtung ab. Eine Teilkonfiguration wird nicht durch
automatische Wiederpaarung oder neue Schlüssel verdeckt. `arm` ist ein
getrennter bewusster Bedienauftrag und speichert die Freigabe.

Bei nativem USB wird DTR eingeschaltet und RTS ausgeschaltet. Geräteantworten
sind begrenzte JSON-Zeilen; Befehle werden nicht zurückgespiegelt. `INFO`
enthält keine Schlüssel. Es unterscheidet Freigabe/Persistenz, Frische,
Funk-/Ausgabefehler sowie bekannte, ausstehende oder bestätigte Ausgabe.

Zum gezielten Stop beider Geräte:

```powershell
python scripts/provision_s3_nearglow.py stop --port-a PORT_A --port-b PORT_B --pair-file PRIVATE_PAIR_JSON
```

Eine STOP-Annahme allein bestätigt noch kein Licht. Der Helfer meldet sicheren
Abschluss erst nach dauerhaftem Disarm und bestätigtem Schwarz beider Stationen.
Rohbefehle `INFO`, `PAIR`, `ARM`, `STOP`, `REBOOT` sind ausschließlich lokale
USB-Einrichtung, keine Erweiterung des bisherigen BLE-V2-Vertrags.

## Gemeinsamer Zwei-S3-Gerätetest

1. Beide Geräte wie oben einrichten und freigeben; zunächst Schwarz und
   verständlichen Status prüfen. Danach getrennte USB-Stromversorgungen verwenden.
2. Stationen zueinander bewegen: warme Glut soll auf Annäherung und Entfernen
   weich reagieren. Keine bestimmte Entfernung aus einem RSSI-Wert ableiten.
3. Partner stromlos machen: ab 3000 ms ohne gültige Antwort ungültig, danach
   Ausblendung nach null. Das Licht tatsächlich beobachten; Status allein reicht nicht.
4. Partner wieder einschalten: nur frische Antworten dürfen die Glut weich
   zurückbringen. Keine Host- oder Handy-Steuerung darf dafür nötig sein.
5. Beide Rollen tauschen und Ausrichtung/Körperabschattung variieren.
6. Beide per Helfer stoppen. Trotz weiterem Funk und nach Neustart müssen
   erfolgreich gesperrte Stationen dunkel bleiben; erst ein neues ARM gibt frei.
7. Falsches Paargeheimnis, fremde Station, Queue-/TX-/Speicherfehler und alte
   Antworten gezielt prüfen. Fehler, Geräteantwort und Lichteindruck getrennt festhalten.

Ein neuer Stations-Spielzustand oder eine Begegnungs-Rätselaktion folgt erst
im getrennten Paketanschluss des [Proximity-Konzeptentwurfs](https://github.com/Sven401/Larp-Escape/pull/116).
Historischer S3-Handytest und Rückweg: [PR #115](https://github.com/Sven401/Larp-Escape/pull/115).

## Prüfstand

Softwareprüfung am 31.10.2026, Ausgangsbasis `ec1c52084e286892ada6540159e4c8036a907d04`:
Implementierungscommit `9f233e41e5c0517add3080f1bea078d3ca01db6f`.

- Nativer ESP-IDF-S3-Build am oben genannten SDK-Pin bestanden;
  Anwendung `0xbf3b0` Byte, 75 % der unveränderten Apppartition frei.
- Näheglut-Core: 400 Assertions mit realer Host-HMAC und unabhängigem
  Python-Paketvektor; native Adapter und Anwendungseigentümer:
  165 Assertions gegen SDK-Fakes, einschließlich erneuter Paarung mit alter Antwort.
- USB-Helfer: zunächst 19 Rechnerfälle ohne Gerätezugriff. CMake-Hostbuild und alle
  sechs registrierten CTest-Fälle bestanden; nach Compilerkorrektur wurde
  der zuvor fehlgeschlagene Core gezielt wiederholt.
- Gemeinsamer LED-Kern: Owner 16, NearGlow 7 mit 2002 Assertions,
  CrystalSeal 11 und RSSI 10 Regressionen bestanden. Architekturprüfung bestanden.

Zusatznachweis am Commit `f5914bb`: Die tatsächliche `PsaCrypto`- und
PMK-/LMK-Ableitungsimplementierung läuft gegen die echte PSA-Bibliothek aus
dem verifizierten SDK. 257 Assertions vergleichen unabhängige Python-Vektoren;
Entwickler, Hauptinstanz und anderer Reviewer haben den Lauf bestanden.
Dies ist Hostausführung der SDK-PSA, keine isolierte Messung des S3-Kryptobeschleunigers.
Die bisherigen Adapterfehlerinjektionen behalten ihren PSA-Fake.

Nach dem Geräteauftrag schützt `d38a2c5` die Bereitschaftsabfrage nach eigenem
Neustart vor begrenztem Boottext; 22 Helfertests unabhängig bestanden.
Normales INFO, Identitäten, JSON, Größen und Fristen bleiben streng geprüft.

Aufteilung: `s3_firmware` verantwortet Firmware, Plattform und Geräteadapter;
`proximity_concept` den USB-Helfer; `s3_review` prüft unabhängig Sicherheit,
Plattform und Nachweisgrenzen; die Hauptinstanz koordiniert und baut den
nativen Stand. Betroffene Variante ist ausschließlich Waveshare ESP32-S3-Zero
mit einer eingebauten RGB-LED; der gemeinsame Typfix wird zusätzlich durch
die bisherigen Hostregressionen abgedeckt.

Diese Rechnerergebnisse ersetzen weder eine elektrische noch eine optische
Geräteabnahme. Der nachfolgende tatsächliche Upload und Funklauf ist im
[separaten Gerätebericht](../../docs/led/s3-nearglow-pair-2026-10-31.md) abgegrenzt.
