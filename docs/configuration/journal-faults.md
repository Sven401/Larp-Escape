# Profiljournal: gezielte modellierte Ausfallabnahme

**Stand:** 7. Oktober 2026. **Status:** 12 zusätzliche native Ausfall-Testfunktionen
gegen tatsächliche Firmwarepfade; physische Speicher-/Stromausfallabnahme offen.
Basis: vorhandener Zweirecord-/CRC-/Private-Controller-/Applicationvertrag,
Firmware-Framingschnitt c9654c3. Produktionscode bleibt unverändert.

## Pfad und Modell

Der Native-Testpfad verwendet ProfileApplication mit ihren privaten zwei Profilen,
ProfileJournal, tatsächlichen Profilimport/Serializer und tatsächliche
Eigentümer-/Aktivierungsprotokolle gegen kopierende Fakes. Nur IJournalStore ist
modelliert: zwei Bytefolgen, veränderter angeforderter Record vor fehlgeschlagenem
beginWrite, partial write mit geschriebenem Prefix, unklarer finishWrite,
deterministische Open-/Read-/Closefehler, Mutation zwischen Inspektion und
erneutem Import. Ein Modell-Confirmed behauptet keine echte Synchronisierung.

Neue BootFixture rekonstruiert eine **neue** Application aus Recordbytes und
richtet synthetische Besitzer ein. Keine privaten Profile/Controllerzustände
werden von außen gesetzt oder freigegeben. Native zusätzliche Modelle und
Strings sind kein zusätzlicher Profilheap in der Firmware.

## Gemessene Fälle

| Fall | Tatsächlich geprüfter Vertrag |
| --- | --- |
| ExistingOnly + Missing | Fault/Empty, keine Datensätze automatisch erzeugt |
| Corrupt + InitializeEmpty | Fault/Corrupt, vorhandene Bytes bleiben erhalten, kein Formatieren/Neuschreiben |
| Writeprefix 0/16/32/100 Byte oder beginWrite nach Truncate fehlgeschlagen | WriteUncertain/Fault; nur anderer Slot verändert, bestätigter Record erhalten; reboot stellt alten Stand her |
| Vollständig lesbarer, unklar abgeschlossener Record | gespeicherter Stand passt nicht zum neuen RAM; GM-Start NotReady; Rescan selektiert unbestätigt und schützt alten Slot; bestätigter Retry schreibt erneut den anderen Slot mit höherer Generation, erst dann Start Executed |
| Neuester Header-/Payload-/Footer-/CRCschaden, Truncate oder trailing bytes | älterer gültiger Record wiederhergestellt; kein automatischer Spielstart/neuer Schreibvorgang |
| Zwei gültige gleiche Generationen | Ambiguous/Fault, kein Überschreiben |
| Future Recordversion neben gültigem alten Record | alter lesbar/spielbereit, writable=false; versuchter neuer Save NeedsRestore/Fault, future Bytes erhalten |
| Open-/Read-/Inspectclosefehler | IoFailure/Fault, keine automatische initiale Speicherung |
| Importend-Closefehler | IoFailure, kein ready Draft/aktives Profil |
| Header/Payload erst beim tatsächlichen Import nach Scan geändert | Corrupt, kein ready Draft/aktives Profil |
| Größte uint32-Generation | bestehender Stand lesbar; versuchter Save CounterExhausted, kein Wrap/anderer Schreibslot |
| Semantisch ungültiges JSON mit gültiger Hülle/CRC/höherer Generation | tatsächlicher Import lehnt ab, ältere semantisch gültige Auswahl; maximumGeneration berücksichtigt höhere Hülle |
| Verlust bisher bestätigter Records im selben Boot | Corrupt statt frischem Empty, kein Neuschreiben/Spielstart |

Die erste Ausführung hatte 50 erfolgreiche Funktionen und eine falsche
Testannahme: draftRevision wurde nach vollständiger Aktivierung als2 erwartet,
der Controller verbraucht die reservierte Entwurfskennung beim Abschluss nochmals
und liefert3. Quellcode bestätigt dies. Jetzt getrennte Beobachtung vor
Aktivierung (zwei Importversuche=2) und nach Abschluss (=3); kein behaupteter
Produktionsfehler oder dafür geänderter Code.

## Nachprüfung und Grenzen

pio test -e host-configured-control: 51 Testfunktionen (vorher39 +12) erfolgreich
ohne Fixtureexport/Exit0, %TEMP%/larp-journal-fault-host-final.log. Initialer
Fehlannahmelauf: %TEMP%/larp-journal-fault-host-first.log. PlatformIO zeigt bei
abgebrochenem Testprozess zusätzlich einen Errored-Eintrag; tatsächliche
Funktionen waren50 erfolgreich/1 fehlgeschlagen, kein52-Funktionen-Nachweis.

Die vier erfolgreichen Firmware-/Werkzeugbuilds des vorherigen Framingschnitts
bleiben der letzte Buildbeleg; seitdem wurden **nur Native-Prüfdateien und
Dokumentation** geändert. Ressourcenmessungen in
[command-framing-v1.md](../companion/command-framing-v1.md) sind weiterhin
Buildmessungen von c9654c3, keine frisch ausgeführte Boardabnahme.
Format141 Dateien/clang19.1.7. Keine neue Remote-CI/Cppcheck-/Upload-/Boardprüfung.

Offen: physischer LittleFS-/Stromausfall-/Schreibhaltbarkeitstest, wechselnde
Backendbytes in allen Zuständen, Readfailure an jedem Offset, vollständige
Reentranz-/Zähler-/Rollback-/Ownerfaultmatrix, Ressourcen-/SMP-/Zeitmessung.
Gültige CRC zeigt Datenintegrität im Modell, keine Authentisierung oder sichere
Flashpersistenz. Gesamtziel und Produktionsanschluss bleiben offen.
