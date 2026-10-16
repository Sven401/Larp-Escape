# Gesicherter Fortschritt aus Baugruppentests

Stand: **16. Oktober 2026**. **Bestätigter Projektauftrag:** Belegte beobachtete
Ergebnisse als nutzbaren Projektfortschritt erhalten, auch außerhalb der Zielanwendung.
Quelle: Nutzerauftrag in Codex-Sitzung `01a1468c-3ad1-7840-9439-076cf817ca52`.
Die folgenden Geräteergebnisse sind **aus den benannten Originalberichten ausgelesen**;
in dieser Sicherungssitzung fand kein neuer Gerätetest statt.

## Gesicherter Bestand

Die Originalberichte, bereinigten Messdaten, Testpläne, Quellen und Tests bleiben
in ihren jeweiligen Git-Ständen erhalten. Für sechs Stände sind eigene
Sicherungsreferenzen `evidence/2026-10-16/<gruppe>` angelegt und veröffentlicht.
Sie bezeichnen keine Releases. Der vollständige Quellbaum samt Vorfahren bleibt
dadurch auch unabhängig vom weiteren Verlauf der Arbeitsbranches erreichbar.

[Das Manifest](baugruppen-nachweise.json) nennt vollständige Commits und Bäume,
die Referenzen, Originalpfade, Git-Blobs und SHA-256 der vorhandenen Dokumente.
Es indexiert auch ältere Fehlschläge, Korrekturen, CSV-/JSON-Messdaten und
Prüfvorbereitungen. Die Originalberichte werden verlinkt statt dupliziert.
Artefakthashes und private Rohbelegorte stehen dort; private APKs, Flashabzüge
und Schlüssel wurden hier weder kopiert noch neu geprüft. Hashangaben allein
sind keine Sicherung einer Binärdatei. Der gesicherte Git-Quellstand erlaubt
die weitere Arbeit auch dann, wenn ein damaliges Buildartefakt nicht vorliegt.

## Anerkannte Teilergebnisse und ihr Nutzen

Jede Zeile hat einen eigenen Beobachtungsumfang. **Belegt am Prüfstand** und
**in die Zielanwendung integriert** sind getrennte Aussagen. Alle nachstehenden
Einträge sind derzeit nicht als gemeinsamer Zielanwendungsstand abgenommen.

| ID / Prüfstand | Belegter Fortschritt | Verwendung im Projekt / verbleibender Nachweis |
| --- | --- | --- |
| **LED-01**, 15.10.; Code `7d1f45b`; Sicherung `led` | Echter USB-Zugangsimport und gegenseitige Anmeldung. Funkeln sichtbar fortlaufend; Stop vollständig dunkel. Weißer Lichtimpuls nach bewusstem Wiederholen und blauer Ripple endeten jeweils dunkel. APK zurückgelesen/hashzugeordnet. | Start-/Stop-/Endzustandsverhalten und geschützter App-/Boxpfad als vorhandene Grundlage für #29/#37. Auswahl ohne Wirkung, Wechsel, Stop/Neustart je Effekt und Funkfehler separat offen; keine GM-/Spielintegration. |
| **WAKE-01**, 15.10.; Code `ca86b93`; Sicherung `wake` | Vier reale Positionen funktional zugeordnet; damals 2/1/4/3. Untere Sensoren teils durch Nutzerdynamik statt zeitgleichem HIGH-Abzug bestätigt. Zwei gleichzeitige Eingänge, alle sechs Tausche, Abbruch außerhalb/in Lücken, Datenerhalt nach App-Neustart und Trennen/Wiederverbinden geprüft. | Livewerte, räumliche Bedienzuordnung und Kachel-/Speicherverhalten erhalten (#39/#53). Spätere benutzerdefinierte Anordnung 2/3/4/1 ist kein Verlust dieser Messung. Chip/B7, elektrische Eignung, Barrierefreiheit und weitere Gesten-/Fehlerfälle bleiben offen. |
| **NFC-01**, 16.10.; Code `d4892bf`; Sicherung `nfc-usb` | Zwei PN532-/Android-HCE-Zyklen mit Flags 1 → 2, vollständigen 59 Bytes und Rücklesen. Wiederholtes CHANGE auf Flags 2 abgewiesen. HCE-Fristablauf/manuelles Ende räumen Routing auf. Software-SPI am Aufbau funktional beobachtet. | PN532-Transaktion, Sollbytes, Wiederholschutz und HCE als unabhängige Gegenstelle nutzbar (#35/#53). HCE ist kein physischer Type-2-Tag und kein elektrischer Pinbeleg. |
| **NFC-02**, 16.10.; Code `d4892bf`; Sicherung `nfc-usb` | Ein physischer NTAG213-Rundlauf App → Tag → Box per USB → Tag → App bestanden. Erstschreiben zunächst unklar; neuer Lesekontakt bestätigte denselben Inhalt ohne erneutes Schreiben. Box änderte nur Flags/CRC; frische Handylesung bestätigte alle Nachherwerte. | UID-/Payloadbindung und getrennte Box-/Handybestätigung als Referenzfall erhalten (#35/#53). Historische zusätzliche Lesung bleibt Teil dieses bestandenen Ablaufs; kein damals direkter Erfolgsweg behauptet. |
| **NFC-03**, 16.10.; Appcode `c0f76ee`, Box Wake unverändert; Sicherung `nfc-usb` | Auf zusätzlichem leerem NDEF-Tag: ein Schreibaufruf, zunächst veraltete Leerantwort, neue Erkennung, gleiche UID und vollständige 59 Byte 310 ms nach Schreibabschluss bestätigt. Kein zweites Schreiben; spätere Kontakte lesen Inhalt erneut. | Belegte Android-Erstbefüllungsfalle und ReaderMode-Korrektur mit Regressionstests erhalten (#35/#53). Gilt für dieses Handy/Tag; Herstellerursache teilweise abgeleitet. Frühere IOException und rotationsbedingt verworfene Aufträge bleiben eigene Befunde. |
| **MATRIX-01**, 16.10.; Erfassung App `20ac696`, spätere Auswertung bei `724f0bd`; Sicherung `matrix` | Lauf mit 68 erkannten Koordinaten/110 registrierten Kontaktfolgen. Neun Felder nutzerbestätigt unbestückt; C3/G3/H3/A8 bleiben Probleme. Klassifikation CONFIRMED, Treffer/Zähler/Zeiten unverändert erhalten; nach App-Neustart geprüft. | Bestückung und gespeicherten Lauf für #54 weiterverwenden; keine vollständige Wiederholung nötig. Deaktivierung betrifft Appanzeige, keinen Firmwarefilter/Spielprofil. Mechanische Betätigungszahl, räumliche Position und elektrische Fehlerursache nicht aus Zählern ableiten. |
| **AUDIO-01**, 16.10.; installiert `243f00c`, Produktquellen `958d6dc`; Sicherung `audio` | Nach Zuschalten zusätzlicher Audioversorgung Track 101 hörbar. Titelauswahl/Play, Pause/Fortsetzen, Lautstärke in Wiedergabe/Pause, manueller Stop, Neustart nach Stop/während Play sowie R1/R2/R4–R6 nutzerbestätigt: etwa 30-s-Stop, Disconnect und App-Verlassen ohne Autostart. BUSY LOW bei Wiedergabe/HIGH bei Stille berichtigt bestätigt. | Audiofunktionsvertrag, Stopregeln und Bediensemantik für #31/#53 erhalten. Fehlgeschlagener erster Hörversuch bleibt erhalten. R3 natürliches kurzes Titelende ausdrücklich ausgenommen, kein offener Pflichtversuch dieses abgeschlossenen Laufs; keine gemessenen Pegel/Latenzen oder elektrische Freigabe. |
| **NFC-04**, 16.10.; Code `dc746ec`; Sicherung `nfc-app` | Ein vollständiger Rundlauf mit sämtlichen Boxaufträgen in echter App bestanden. Appschreibung Flags 1 bestätigt; Box änderte 1 → 2 und las zurück; neue reine Handylesung bestätigte dieselbe UID und komplette Nachher-Fixture. Alte Handyanzeige genügte ausdrücklich nicht. | Durchgängiges Interaktionsmuster und Worker-/UID-/Rücklesevertrag für #35/#53. App schrieb bereits vorhandene Flags 1 erneut. Zweiter Lauf/falscher Tag/Abbrüche offen; Chiptyp dieses Tags nicht gesondert bestimmt. |

## Originalbelege mit festen Quellständen

- LED-01: [Gerätebericht](https://github.com/Sven401/Larp-Escape/blob/20518f52de82845635e8590f1440dbfbb268dcb2/docs/led/device-test-2026-10-15.md).
- WAKE-01: [Gerätebericht und getrennte Beobachtungsquellen](https://github.com/Sven401/Larp-Escape/blob/10400350f31883b4e7b36df1d6875388a2d8953f/docs/wake/device-test-2026-10-15.md).
- NFC-01: [HCE-Bericht](https://github.com/Sven401/Larp-Escape/blob/33bf31bcd082ad62c14bd9ffaa2b547e17e919bc/docs/nfc/device-preparation-2026-10-16.md).
- NFC-02: [Physischer NTAG213-Rundlauf](https://github.com/Sven401/Larp-Escape/blob/33bf31bcd082ad62c14bd9ffaa2b547e17e919bc/docs/nfc/physical-tag-test-2026-10-16.md).
- NFC-03: [Erstbefüllung, Korrektur und reale Rücklesung](https://github.com/Sven401/Larp-Escape/blob/33bf31bcd082ad62c14bd9ffaa2b547e17e919bc/docs/nfc/first-write-readback-2026-10-16.md).
- MATRIX-01: [Ergebnisbericht](https://github.com/Sven401/Larp-Escape/blob/724f0bdabed2bd51c170942f3e45bc2cecc34932/docs/matrix/collection-result-2026-10-16.md), [vollständige CSV](https://github.com/Sven401/Larp-Escape/blob/724f0bdabed2bd51c170942f3e45bc2cecc34932/docs/matrix/evidence/collection-result-2026-10-16.csv), [bereinigte Messdaten](https://github.com/Sven401/Larp-Escape/blob/724f0bdabed2bd51c170942f3e45bc2cecc34932/docs/matrix/evidence/collection-result-2026-10-16.json).
- AUDIO-01: [Gerätebericht einschließlich R1–R6 und Ausnahme](https://github.com/Sven401/Larp-Escape/blob/cff0c998cde05715c2117c17ecf42681b69d1ad4/docs/audio/device-test-2026-10-16.md).
- NFC-04: [App-/Box-Rundlauf](https://github.com/Sven401/Larp-Escape/blob/14d7a0dae4ae0e697ff609bb0e86996e9864a62d/docs/nfc/app-device-test-2026-10-16.md).

## Übergang in die nächsten Arbeitspakete

Für die jetzt anstehende Konsolidierung #53 wird jede obige ID als Ausgangsbeleg
geführt. Der spätere Code-PR benennt pro ID **übernommen**, **als eigenständiger
Prüfpfad erhalten**, **gezielte Nachprüfung erforderlich** oder **bewusst zurückgestellt**,
mit Quell-/Zielcommit und Begründung. Zurückstellung einer Funktion ändert den
belegten historischen Status nicht. Ein bewusst abweichendes Produktverhalten
braucht einen dokumentierten fachlichen Entscheid; der ursprüngliche Beleg bleibt.

Die Übernahme darf insbesondere folgende Ergebnisse nicht verlieren:

- LED-Endzustände und bewusste Bedienung; Wake-Livewerte und gespeicherte Anordnung.
- Matrixlauf mit 68/110, neun unbestückten Feldern und vier getrennten Problemstellen.
- Audio-Pause/Fortsetzen/Lautstärke, Stopgrenzen und kein Autostart nach Rückkehr.
- NFC-Erstschreibkorrektur ohne Wiederholung, UID-Bindung und frische unabhängige Abschlusslesung.

Neue gemeinsame Softwaretests prüfen die übernommenen Verträge; sie verwandeln
historische Gerätebeobachtung nicht in eine neue Geräteabnahme. Ein neuer
Hardwarelauf ist gezielt nötig, wenn geänderte Quellen, Verdrahtung, Laufzeit-
besitzer oder Transport-/Statusverträge die Aussage berühren. Unberührte bestätigte
Teilergebnisse werden nicht pauschal neu aufgerollt. Konflikte oder Regressionen
bekommen einen eigenen Befund mit Bezug auf die ID, statt den früheren Erfolg zu löschen.

#54 arbeitet am erhaltenen Matrixlauf weiter. #29/#37 und #35 bearbeiten die
benannten Restfälle. #31 übernimmt die Ergebnisse in den gemeinsamen Spielbetrieb.
Allgemeine GM-Rollen, elektrische Eignung, Langzeitbetrieb und Profilfreigabe
bleiben eigenständige Abnahmen. Die bloße Anzahl offener Gesamtissues ist daher
kein Maß für den bereits bestätigten Fortschritt.

## Sicherungsprüfung und Grenzen

Bei Erstellung: Existenz aller Manifest-Blobs, SHA-256 und Quellzuordnung
gegen Git geprüft; Sicherungsreferenzen lokal und im Remote abgeglichen.
Das sichert den veröffentlichten Quellen-/Dokumentenbestand. Existenz und
Lesbarkeit sämtlicher privater Rohartefakte sind hier nicht neu auditiert.
Fehlende Rohdateien wären als Lücke nachzutragen; veröffentlichte bereinigte
Belege und der historische Beobachtungsstatus bleiben erhalten.
Kein Merge, Gerätewechsel, Upload oder Restore.
