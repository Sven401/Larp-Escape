# Migrationsplan: native ESP-IDF-Basis und Projektaufbau prüfen

## Beauftragte Umsetzung · 28. Oktober 2026

**Aktiver Folgeauftrag:** Der Nutzer beauftragt in derselben Sitzung die Arbeit
an den Zielen dieses Plans. Bestandsprüfung, native Buildbasis und PSA-Portierung
sind begonnen. [Bestand, Zielentscheidung, Prüfungen und offene Tore](../docs/migration/esp-idf-qualification-2026-10-28.md)
sind der aktuelle Umsetzungsstand. Der ursprüngliche reine Dokumentationsumfang
unten ist historisch; er sperrt diesen Folgeauftrag nicht. Upload und gemeinsame
Geräteabnahme bleiben separat. Keine abgeschlossene Gesamtmigration behauptet.

## Verbindliches Ziel und Planungsstand · 28. Oktober 2026

**Entschieden:** Die neueste stabile ESP-IDF-Version ist die vorrangig zu
prüfende Zielbasis. Arduino ist keine Projektanforderung mehr. Arduino darf
nur als optionale IDF-Komponente übernommen werden, wenn ein konkreter,
erheblicher Nutzen für Larp-Escape gegenüber nativen IDF-Adaptern belegt ist.
Die bisherige Vorgabe Arduino-ESP32 3.x / ESP-IDF 5.x ist damit ersetzt.
Quelle: ausdrücklicher Nutzerauftrag in Sitzung
`01a1840c-a839-7ae3-9954-bb6ffc96d5e4` am 28.10.2026.

**Verifiziert, nur Releaseabgleich:** Espressif kennzeichnet am 28.10.2026
[ESP-IDF v6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1)
als neueste stabile Haupt-/Nebenversionslinie (Latest). Das ist der erste
Prüfkandidat, keine bereits abgenommene Projektbasis. Vor Umsetzung Latest,
Patchstände, Supportzeitraum, Known Issues und Security-Advisories erneut
an offiziellen Quellen prüfen. Ein späterer Patch einer älteren Linie ist
nicht automatisch die neueste Zielgeneration. Keine Vorabversion und kein
beweglicher master-/latest-Verweis als reproduzierbarer Buildpin.

**Offen:** Kompatibilität, konkrete Toolchainpins, Architekturentscheidungen,
Umsetzung und Geräteabnahme. Dieser Auftrag aktualisiert den Plan; er startet
keine Paketaktualisierung, Portierung oder Installation. Dokumentbasis:
BLE-Arbeitsbranch `codex/ble-rollen-profile`, HEAD vor dieser Änderung
`53bd24bee7b21b87cbb9781e1f53ad4d430da91c`; Plan und BLE-Übergabe gelesen,
keine neuen Produktbuilds oder Geräteprüfungen. Der historische Dateiname
bleibt zur Erhaltung vorhandener Links bestehen.

## Vorgehen und Entscheidungstore

### P1 · Bestand und Projektgrenzen erfassen

- Aktuellen Integrationshead und gesicherten BLE-Stand abgleichen; getrennte
  Arbeitsbasis für die spätere Migration verwenden. Historische LED-Branches
  nicht als aktuellen Integrationsstand behandeln.
- Tatsächliche Chips, Revisionen, Flash/PSRAM, Partitionen und Pinbelege erfassen;
  Boardkonfiguration und physisch bestätigte Geräteidentität unterscheiden.
- Für jede benötigte Firmwarevariante Quellpfad, Einstieg, Bibliotheken,
  Arduino-Aufrufe, direkte IDF-/FreeRTOS-Nutzung und private SDK-Eingriffe
  erfassen. Produkt, Labor, Proben und historische Quellen getrennt bewerten.
- Vorhandene GameDomain-/GamePorts-/GameExecution-Grenzen und Hosttests prüfen
  und erhalten. Fachlogik soll ohne Arduino, IDF und FreeRTOS testbar bleiben.
  Komponenten nach fachlichen oder technischen Grenzen schneiden; keine
  pauschale Neuarchitektur und kein zusätzlicher Task pro Baugruppe.

**Ergebnis:** Bestands- und Portierungsmatrix mit Quelle/Commit, Adapter,
Abhängigkeit, nativem Ersatz, Risiko und zugehörigem Nachweis. Offene reale
Hardwarebefunde bleiben ausdrücklich offen.

### P2 · Neueste stabile ESP-IDF-Version qualifizieren

- Mit dem dann neuesten stabilen Release beginnen; exakten Tag/Commit und
  Submodule, Toolchain, Python-/Buildwerkzeuge sowie Supportstatus dokumentieren.
  Migration Guides aller übersprungenen Hauptversionen auf betroffene APIs
  prüfen; Chipunterstützung, Known Issues und Advisories einschließen.
- Native Grundlage mit CMake, Ninja und idf.py planen. Versionierte
  Komponenten über IDF Component Manager und Lockfile; sdkconfig.defaults,
  Partitionierung und reproduzierbare CI-/Entwicklungsumgebung festlegen.
  PlatformIO ist eine zu bewertende Alt-Buildabhängigkeit, keine Zielvorgabe.
- Bluetooth-Stack und verfügbare Sicherheits-/Lifecycle-Schnittstellen anhand
  der bestehenden Verträge bewerten. Kein ungeprüfter Wechsel zu NimBLE oder
  Übernehmen alter Bluedroid-Offsets. Mbed-TLS-/PSA-API, Zufallsquelle,
  Schlüsselhaltung und TLS-Konfiguration am tatsächlichen Zielstand prüfen.
- Eine ältere unterstützte IDF-Linie nur bei belegtem Blocker als begründete
  Abweichung vorlegen: fehlende Fähigkeit, Quelle, Auswirkungen, Ersatzoption
  und Wiedervorlage nennen. Arduino-Kompatibilität allein ist kein Grund,
  die neueste IDF-Zielbasis aufzugeben. Zielwahl samt Restbefunden festhalten.

**Ergebnis:** begründete Versionsentscheidung und feste Abhängigkeitsliste;
Releaseverfügbarkeit allein gilt nicht als Kompatibilitäts- oder Securityfreigabe.

### P3 · Arduino ausschließlich nach Nutzenprüfung

Für jede erwogene Arduino-Komponente festhalten: benötigte Funktion/Bibliothek,
IDF-nativer Ersatz, eingesparter Portierungs- und Wartungsaufwand, zusätzliche
Abhängigkeiten, Ressourcenbedarf, Task-/Initialisierungsfolgen, Securitypflege
und nachgewiesene Kompatibilität mit der gewählten IDF-Version. Beibehalten
nur bei erheblichem belegtem Projektnutzen; reine Gewohnheit oder bestehende
Includes genügen nicht. Ohne solchen Nachweis native Adapter planen.

Espressif dokumentiert [Arduino als IDF-Komponente](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html).
Diese Möglichkeit ist kein Beleg der Kompatibilität mit der neuesten IDF-Linie;
konkretes Versionspaar bei der Entscheidung prüfen. Optionale Arduino-Abhängigkeit
an Adaptergrenzen halten, nicht in Domain oder öffentliche Fachverträge ziehen.

**Ergebnis:** ausdrückliche Entscheidung ohne Arduino oder eine begrenzte,
begründete Komponentenliste mit Versionen und Rückweg zur nativen Umsetzung.

### P4 · Schrittweise Portierung und Parität planen

1. Reproduzierbaren nativen Minimalbuild und CI-Build vorsehen; anschließend
   hardwareunabhängige Logik und ihre Hosttests auf der neuen Buildbasis prüfen.
2. BLE/Sicherheit und die tatsächlich benötigten Hardwareadapter in getrennten
   Schritten portieren. Die nachstehenden BLE-Prüftore sind verbindlich;
   private sdk446-Hooks benötigen neue Nachweise oder einen geprüften Ersatz.
3. ESP-NOW-Empfang/Koexistenz, RMT-Ausgabe und Besitzer, UART/Audio,
   I²C/Matrix/Wake sowie NFC jeweils gegen erhaltene Funktions-/Fehlerverträge
   prüfen. Zeitverhalten, Speicher und Last als gemeinsame Grenzen betrachten.
4. Persistenz und Partitionierung mit Identitäten, Bonds, Peerregister,
   Schlüsseln, Profilen und Spielständen planen. Datenformate, Upgrade und
   Rückweg ausdrücklich prüfen; kein stiller Werksreset als Migrationsersatz.
5. Betroffene Host-/Kryptografie-/SDK-/Androidtests und Firmwarevarianten neu
   ausführen. TLS-Gegenstellenfälle auf dem neuen Backend wiederholen und um
   zielbezogene Negativ-/Securityfälle ergänzen. Alte grüne Ergebnisse bleiben
   Herkunft und sind keine Nachweise für die neue Basis.
6. Nach gesondert beauftragtem Upload und chipgebundenem Rückweg ein eindeutig
   zugeordnetes App-/Firmwarepaar prüfen: BLE-Einrichtung, gegenseitiger Nachweis,
   Rollen/Entzug, Bond-Reconnect, geschützter Transfer, Datenbestand nach Neustart,
   alle betroffenen Baugruppen, Funkkoexistenz, Heap/Stacks, Timing und Dauerlast.

**Abschluss:** geprüfte Commits, reproduzierbarer sauberer Build, Portierungs-
und Paritätsmatrix sowie getrennte Software-/Gerätebelege. Verbleibende Grenzen
und Rückwege dokumentieren; Plattformmigration und noch offene BLE-/Rollen-/
Profilproduktintegration getrennt bewerten. Keine Produktabnahme aus bloßen
Builds oder synthetischen SDK-Modellen ableiten.

## Erhaltene BLE-Übergabe und Sicherheitsprüftore

Die folgenden Befunde und Prüfpflichten bleiben erhalten. Frühere Zielangaben
Arduino 3.x / IDF 5.x sind durch die Entscheidung oben ersetzt; historische
Abschnitte dokumentieren ihre damalige Begründung und sind kein Parallelauftrag.

## Neue Übergabeentscheidung vom 28.10.2026

**Vom Nutzer entschieden:** Die Plattformmigration wird ein eigener Plan.
Arbeit und Migrationsgrund werden im Projekt festgehalten, damit der Maintainer
die benötigte Migration getrennt durchführen kann. Das BLE-/Rollen-/Profilziel
dieser Sitzung wird mit dieser Übergabe geschlossen; die ursprüngliche
Produktintegration und Geräteabnahme sind dadurch nicht abgeschlossen.
Quelle: Nutzerentscheidung in Sitzung `01a183bd-0df7-72e3-a67c-ddde84798a0b`.
Keine Plattformpakete geändert, kein Geräteupload, kein neuer Chat/Zeitplan.

Erreichter Arbeitsstand: [Android-Transferkern, geschützter Firmware-Rückkanal,
Prüfungen und erhaltene Restarbeit](BLE-ROLLEN-PROFILE-2026-10-28.md),
[PR #100](https://github.com/Sven401/Larp-Escape/pull/100).
Geprüfte Produktquellen: Android `6242960`, Firmware `6d7edee`;
Arbeitsbranch vor dieser Übergabedokumentation `4ee93f0`.

### Konkreter zusätzlicher Migrationsgrund

Für den vollständigen Produktzugang müssen gegenseitiger Boxnachweis,
Rollenprüfung und sämtliche privaten Befehle/Profilantworten an denselben
geschützten Anwendungskanal gebunden sein. Ein signierter Servernachweis allein
schließt das Weiterreichen einer Anmeldung mit anschließender Klartextinjektion
nicht. Ein standardmäßiger gegenseitiger TLS-Kanal wurde deshalb technisch
untersucht; kein neues eigenes Kryptoprotokoll als Ersatz festgelegt.

Lokal aufgelöst: Arduino-ESP32 2.0.14 / ESP-IDF 4.4.6 mit Mbed TLS **2.28.4**.
Die [Hersteller-Advisory zu CVE-2025-27810](https://mbed-tls.readthedocs.io/en/latest/security-advisories/mbedtls-security-advisory-2025-03-2/)
nennt alle Versionen bis 2.28.9 bzw. 3.6.2 als betroffen: Bei Speicher- oder
Kryptografie-Hardwarefehlern kann das TLS-Finished fehlerhaft berechnet werden,
mit möglichem Authentisierungsverlust. Behoben laut Advisory in 2.28.10 bzw.
3.6.3. Der gelesene Versionsstand fällt darunter; ein passender SDK-Backport ist
nicht belegt. Keine Ausnutzung am Gerät behauptet. Eine neuere Arduino-
Hauptversion allein beweist noch keinen behobenen eingebetteten TLS-Stand.

Die lokale `dio_qspi`-SDK-Konfiguration aktiviert TLS 1.2, ECDHE-ECDSA, GCM,
Renegotiation und 16-KiB-Recordpuffer. Die neue Nutzung würde damit einen
tatsächlichen TLS-Aufrufpfad eröffnen. Die bisherigen P256-Hostnachweise sind
keine Sicherheitsfreigabe dieses TLS-Pfads. Ein isolierter normaler
[Mbed-TLS-/OpenSSL-Machbarkeitsversuch](quellarchiv/ble-tls-feasibility-2026-10-28/README.md)
bestand neun Fälle, prüft aber gerade keine Ausfälle der Advisory.

### Auftrag für den getrennten Migrationsplan

Federführung: Plattform/Systemintegration; unabhängige Qualität und gezielter
Security-Fachreview nach [Fachmandaten](../docs/quality/verantwortlichkeiten.md).
Geräteintegration verantwortet Pins, Bus-/Fehlerverträge und den Flash-Rückweg.
Der Maintainer besetzt die Funktionen vor Umsetzung. Nächster Schritt: eigene
Arbeitsbasis und genaue unterstützte Zielpakete festlegen, dann Auslöser und
Prüftore unten gegen tatsächlichen Code/SDK abgleichen.

1. Neueste stabile ESP-IDF-Version und ihr Kryptografiebackend nach P2 prüfen;
   optionale Arduino-Komponenten nach P3 bewerten. Support und einschlägige
   Securitykorrekturen verifizieren und pinnen. Keine Versions- oder Hashguards
   bloß entfernen, um Builds grün zu bekommen. Kein unbekannter Backport als Fix.
2. Originale GATT-/SMP-Callbacks, private Typen/Offsets, Aufgabenbesitz und
   Key-/Registry-Bindung neu prüfen. Die heutigen `sdk446`- und Binärpins sind
   keine Belege für die neue IDF-Zielbasis. Keine Ersetzung durch öffentliche
   GAP-auth_success-Flags.
3. ESP-NOW-Metadaten und Koexistenz, RMT/LED-Besitzer, UART/Audio, I²C/Matrix/Wake,
   PN532-Patch und Gerätefehlerverträge auf der neuen Basis erhalten und prüfen.
   Bestehende Pinbelege und B7-/Versorgungsgrenzen bleiben getrennt erhalten.
4. Gespeicherte Identitäten, Einrichtung/Peerregister, Profile und Spielstand
   schützen; klare Migrations-/Rückwegpolitik statt stiller Neuinitialisierung.
   Vor Upload neue Chip-/Flash-Zuordnung gegen den vorhandenen Rückweg prüfen.
5. Sämtliche betroffenen Host-, echten Kryptografie-, ABI-/SDK- und Androidwege
   auf neuem Stand ausführen; reale Geräte-, Stack-/Heap-/Timing- und gemeinsame
   Lastnachweise getrennt erbringen. TLS benötigt zusätzlich Record-/Zertifikat-/
   ECDHE-Puffer; die alten ABI-Größen sind kein gemeinsames Produktbudget.
6. Danach die erhaltene BLE-/Rollen-/Profilintegration fortsetzen: Produktstart,
   vertrauenswürdige Einrichtung und Serverpin, Bond-Reconnect, vollständiger
   geschützter Ein-/Ausgang ohne Klartextfallback, Android-GM-UI, echte
   Profilgeräteadapter und gemeinsamer Geräteablauf. Das sind weiterhin offene
   Produktnachweise, keine erledigten Migrationsnebenwirkungen.

Die folgenden Abschnitte sind historisch und durch P1–P4 oben ersetzt.
Sie bewahren den vorherigen Auftrag und seine damaligen
Grenzen. Eine konkrete Zielversion und eine Umsetzung werden hier nicht erfunden.

## Historischer Auftrag und bisherige Planung

**Historischer Status vom 18.10.2026: beschlossen und einzuplanen; Umsetzung damals offen.**
Datum: 18. Oktober 2026. Quelle: ausdrücklicher Nutzerauftrag in der
ESP-NOW-Sitzung `01a14c5f-f677-7403-a000-bf20e5ba93a3`: Aktualisierung als
zukünftige notwendige Projektarbeit dokumentieren und jetzt keinen weiteren
Aufwand in die Migration investieren. Kein Auftrag zur sofortigen Umsetzung,
zu einem Geräteupload oder zum Austausch der funktionierenden Installation.

## Anlass und Ausgangsbasis

Die damalige Planung sah **Arduino-ESP32 3.x mit der dazu passenden
ESP-IDF-5.x-Basis** vor; dieses Ziel ist inzwischen ersetzt. Konkrete Zielversion, kompatible
PlatformIO-Pakete, Termin und Aufwand werden erst bei Wiederaufnahme bestimmt.

Geprüfte Ausgangsbasis: ESP-NOW-Arbeitsbranch bei `9493bf2`,
`firmware/led/platformio.ini` und `firmware/espnow-probe/platformio.ini`:
Platform `espressif32@6.6.0`, Arduino-Paket `3.20014.231204` (Arduino-ESP32
2.0.14, ESP-IDF 4.4). Empfang in `firmware/led/src/EspNowRadio.cpp` verwendet
die ältere Schnittstelle ohne Empfangsmetadaten; der LED-Adapter
`firmware/led/src/RmtLedBus.cpp` verwendet die ältere SDK-RMT-Schnittstelle.
Prüfweg: Konfiguration und diese Quellen am 18. Oktober 2026 gelesen.

Motivation ist eine langfristig passende Frameworkbasis und insbesondere der
direkte Zugriff auf paketbezogene ESP-NOW-Signalstärke (RSSI) für die geplante
Näheanzeige und mögliche näheabhängige Funkelintensität. RSSI bleibt eine
Näherung; die Aktualisierung allein schafft keine genaue Entfernungsmessung.

## Supportbefund aus beauftragtem Audit · 24. Oktober 2026

Der spätere Nutzerauftrag zum [A1/A2-Pilot](../docs/quality/audits/2026-10-24-pilot/README.md)
erlaubte den Quellenabgleich, nicht die Umsetzung der Migration. Basis
`e61d7642`: lokal aufgelöst Arduino 2.0.14 / ESP-IDF **4.4.6**.
[Espressif AR2024-008 vom 02.07.2024](https://www.espressif.com/sites/default/files/advisory_downloads/AR2024-008%20End-of-Life%20Advisory%20for%20ESP-IDF%20v4.4%20Release%20Branch%20EN.pdf)
beendet den regulären Support der 4.4-Reihe seit Juli 2024, einschließlich
weiterer Fehler-/Securitykorrekturen. Hashpins ändern diesen Status nicht.

**Offen, zuständig Plattform und Systemintegration:** Für den nächsten
beauftragten Planungs-/Migrationsabgleich, spätestens vor Produktfreigabe
oder Pinänderung, eine ausdrückliche Weiterbenutzungsentscheidung mit Grund,
Verantwortlichem und Wiedervorlage oder die Einplanung der Migration vorbereiten.
Der Maintainer entscheidet technische Priorisierung im bestehenden Mandat;
grundlegende Produktentscheidungen bleiben beim Nutzer. Noch keine Ausnahme
beschlossen, kein neues Zieldatum und keine unterstützte Zielversion behauptet.
Die bisherige Festlegung, jetzt keine Pakete zu ändern, bleibt bestehen.

Konkrete Advisories mit einem Security-Fachreview auf tatsächliche Konfiguration,
Aufrufpfad und gepinnte Binaryversion prüfen. Eine EOL-Version, die in einer
Advisory-Matrix fehlt, ist nicht automatisch unbetroffen; alleinige BLE-Nutzung
der App schließt einen aktivierten BTDM-Controllerpfad nicht aus. Die offene
Classic-L2CAP-Frage ist im Pilot begrenzt beschrieben, keine bestätigte CVE-
Betroffenheit. Bei der späteren Zielwahl Supportstatus und private GATT-/Crypto-
ABI-Hooks neu auditieren; Hashes nicht ohne diesen Nachweis austauschen.

## Vorgemerkter Umfang und Abschlusskriterien

- Unterstützte, reproduzierbar gepinnte Buildumgebung auswählen; Migration
  zuerst getrennt vom funktionierenden Geräte-/Firmwarestand erproben.
- ESP-NOW-Empfang auf die neue Metadatenschnittstelle anpassen; LED-RMT,
  BLE-Schnittstellen und weitere tatsächlich betroffene Adapter prüfen und
  nötigenfalls migrieren. Keine pauschale Annahme unveränderter Kompatibilität.
- Betroffene Firmwarevarianten bauen und passende Host-/Protokolltests
  ausführen. App-Vertrag, Authentifizierung und vorhandene Schlüssel erhalten.
- Vor später ausdrücklich beauftragten Uploads chipgebundene Rückwege sichern;
  danach BLE-/ESP-NOW-Koexistenz, App-Zugang sowie LED, Audio, NFC und Matrix
  am jeweiligen Gerät erneut prüfen. Ergebnisse je Baugruppe getrennt belegen.

Die eigentliche RSSI-Anzeige, Kalibrierung und Effektsteuerung sind gesonderte
Funktionsarbeit; deren konkrete Ausgestaltung ist mit diesem Plan nicht
beschlossen. Damalige Auftragsgrenze: keine Pakete aktualisieren, Prototypen
erstellen oder zusätzliche Untersuchungen beginnen.

## Bereits geprüfte Referenzen

- [Espressif: Migration Arduino-ESP32 2.x → 3.0](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html)
  beschreibt den Wechsel von ESP-IDF 4.4 auf 5.1 und inkompatible Änderungen.
- [Espressif: RSSI bei ESP-NOW](https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html#how-do-i-obtain-the-corresponding-rssi-when-transferring-data-using-esp-now)
  beschreibt direkten Metadatenzugriff in neueren Versionen und den bisherigen
  Mitschnittweg. Referenzen am 18. Oktober 2026 geprüft; Zielversionen bei
  Wiederaufnahme erneut bewerten.
