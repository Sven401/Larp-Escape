# BLE, Rollen und Profile: abgeschlossene Arbeitsuebergabe

Stand: **28.10.2026**, Sitzung `01a183bd-0df7-72e3-a67c-ddde84798a0b`.
Status: **auf Nutzerauftrag mit Uebergabe abgeschlossen; Produktintegration und Geraeteabnahme offen**.
Basis: `f81956faffa4d4c6b0198fdbd0c626b21e137cd4`, nach frischem Fetch.
Zielbranch bleibt `plan/projektuebergabe-kuratierter-stand`.

**Abschlussentscheidung, 28.10.2026:** Der Nutzer bestimmt die Plattformmigration
als eigenen Plan und schliesst dieses Ziel nach Sicherung von Arbeit und Grund.
[Verbindliche Migrationsuebergabe und naechster Maintainerauftrag](ARDUINO-ESP32-3-MIGRATION.md).
Das ist keine Abnahme des urspruenglich angestrebten gesamten Produktablaufs.
Keine weitere Implementierung, Installation oder Migration in dieser Sitzung.

## Folgeauftrag: bedingte Wiederaufnahme

Am 28.10.2026 beauftragt der Nutzer einen
[Maintainer-Fortsetzungsplan](BLE-FORTSETZUNGSPLAN-2026-10-28.md).
Er ergaenzt die obige Abschlussentscheidung: Nach belegter Erfuellung der
Softwaretore S1 bis S6 darf die vollstaendige BLE-Produktintegration weitergehen,
ohne erneute allgemeine Umfangsfrage. Jetzt nur Plan; keine Plattform- oder
Produktumsetzung, kein Upload und keine automatische Wiederaufnahme.
Die urspruenglichen Nachweis- und Abnahmegrenzen bleiben erhalten.

## Auftrag, Verantwortliche und Grenzen

Nutzerziel: Android verbindet sich authentifiziert mit einer V2-Box, erkennt
bzw. ordnet eine autorisierte Rolle zu, laedt ein gueltiges Spielprofil und
fuehrt ausschliesslich freigegebene Bedienaktionen aus. Host-, App- und echter
Geraetenachweis einschliesslich Fehler und Wiederverbindung sind erforderlich.
Nutzerantwort zuerst: Geraete spaeter, zunaechst Software fertigstellen.
Spaetere Nutzerfreigabe derselben Sitzung: Android und ESP verfuegbar;
Moto G14 per ADB erkannt, GM 0.2/versionCode2 vom 18.10. installiert; ESP an COM3.
Keine Installation oder funktionale Geraeteabnahme in diesem Zwischenstand.

Maintainer und fuehrendes Plattform-/Integrationsmandat: Hauptagent `/root`.
Unabhaengige Security-/Qualitaetsgegenpruefung des begrenzten Aenderungsumfangs:
Subagent `/root/security_review`, eigener lesender Durchgang. Keine externe
Zertifizierung. Varianten: gemeinsamer Android-Protokollkern, Dev und GM;
Zweiter Baustein: allgemeiner Firmware-Peer-Rueckkanal,
Host-Konfiguration/Kryptografie und SDK-ABI-Ziel. Baugruppenfirmware,
Produktstart, UI und Geraeteadapter noch nicht geaendert.

## Implementierter erster Baustein

- `ProfileTransferV1.kt`: vorhandene C++-Formate fuer Metadata/Data/Control/
  Response v1, 20/9..128/12/20 Byte. Unveraenderte Rohbytes und CRC32,
  uint32-Grenzen, maximal 128 KiB; Bounds vor Kopie/CRC.
- `ProfileSourceValues`: eine begrenzte unveraenderliche Quelle, owned Werte,
  Standard 20-Byte-ATT-Werte; groessere Werte nur nach MTU-Abgleich.
- `ProfileExchange`: genau eine urspruengliche Verbindungsgeneration und ein
  monotoner Controlzaehler fuer deren Lebensdauer, keine Transfer-ID-Wiederverwendung.
  Kein automatisches Apply. Importiert, eingereiht und gespeichert getrennt.
  Verbindungsabbruch, Zeitablauf, Zugriffsentzug oder unklarer Controlzaehler
  sperren Fortsetzung. Keine Wiederholung eines Datenframes oder Apply-Auftrags.
- Dieser Baustein ist **noch nicht mit dem produktiven BLE-Transport verbunden**.
  Er erteilt keine Rolle, authentifiziert keinen Transport und validiert kein
  Spielschema. Ein erzeugtes Metadatenpaket bedeutet kein gueltiges Profil.
  Erst die vorhandene Firmwarevalidierung kann DraftReady liefern.

Goldenquelle: `docs/companion/fixtures/profile-source/` direkt gelesen,
keine zweite Kopie unter Android. Vertraege:
[Frames](../docs/companion/profile-frames-v1.md),
[Controls](../docs/companion/profile-controls-v1.md),
[Apply](../docs/companion/profile-apply.md).
Exakte Draftzaehler: DraftReady = expectedDraft+1, Queued = acceptedDraft,
Saved = acceptedDraft+1; bei Saved Konfiguration = expectedConfiguration+1.
Das sind Bearbeitungszaehler, nicht die Revision im Profildokument.

Reviewkorrekturen: vorgezogene Speichergrenzen; exakte finale Revisionsbindung;
UNKNOWN statt Weiterzaehlen nach nichtverbrauchenden/mehrdeutigen Controlfehlern.
Der Gegenpruefer hat diese konkreten Befunde nach Quellnachpruefung geschlossen.

## Bestaetigte Anschlussluecken an der Basis

Quellenpruefung durch Hauptagent und Gegenpruefer, keine neuen Funkmessungen:

1. Baugruppen-V2-HMAC (`StationProof`/`LedWire`) ist kein allgemeines GM-Recht.
   Kein HMAC-zu-TrustedAuthority-Bridge. PeerAuth AUTHENTICATED ist kein GRANTED.
   Siehe [Identitaet/Rechte](../docs/companion/identity-access.md) und
   [Veranstaltungszugang](../docs/companion/veranstaltungszugang.md).
2. `GattsServiceDefinition`/`SdkGattsServiceDriver` registrieren bisher nur
   Command/Profile/Challenge/Issue/Proof/Result. Es fehlen ein echtes
   Status-/Antwortattribut und produktiver `IReplyFrameBearer`.
   Nur `ProbeReplyBearer` in `src/ProfileRuntimeProbe/main.cpp` war vorhanden.
   **Fortschreibung:** optionaler echter Lesebearer jetzt in `6d7edee` implementiert
   und geprueft; der Produktstart ist weiterhin nicht angeschlossen.
3. Allgemeiner Peerzugang verlangt Original-SMP, SC, MITM und Bond.
   Bond-Reconnect-Evidenz, gegenseitiger Servernachweis und vertrauenswuerdiger
   produktiver Einrichtungs-/Rechteweg sind offen. Nicht durch GAP-auth_success,
   gleiche MAC oder zwei voneinander getrennte erfolgreiche Handshakes ersetzen.
4. `src/main.cpp` startet weiterhin Legacy; die Profilruntimeprobe verwendet
   synthetische Geraeteeigentuemer. Profilimport allein ist noch kein realer
   profilgesteuerter Boxbetrieb.
5. `codex/nachladbare-spielpakete` enthaelt Dokumentation, keine benoetigte
   Implementierung. SP-01 ist fachlicher Kontext, keine ungeprueft zu uebernehmende
   Mergekette. Generische Spielrollen aus Profilen nicht in Android hartcodieren.

## Getrennte Folgearbeit fuer den Maintainer

**Zuerst:** den [eigenen Migrationsplan](ARDUINO-ESP32-3-MIGRATION.md) aufnehmen.
Die folgenden Produktarbeiten bleiben als anschliessende Integration erhalten.

1. Geschlossenes Produkt-Integrationsziel mit echten Registry-/Security-/
   Profilbesitzern und dem implementierten Rueckkanal zusammensetzen. Keine
   Probe- oder Fake-GM-Eigentuemer in das Produkt uebernehmen.
2. Gegenseitigen Servernachweis und vertrauenswuerdige Einrichtung an EINEM
   urspruenglich gebundenen BLE-Aktor anschliessen; keinen HMAC-Grant-Bridge bauen.
3. Bond-Wiederverbindungsnachweis implementieren und gegen aktuelle Registry,
   Credential, Securitymodus und frischen Originalverbindungstoken pruefen.
4. Android-Transport, Rollengate und GM-Produktbedienung anschliessen; die
   vorhandenen UI/UX-Vertraege beachten. Aktionen nur aus aktueller autorisierter
   Sitzung und bestaetigtem gueltigem Profil anbieten; unbekannte Rollen ablehnen.
5. Reale Profilgeraeteadapter, Betrieb und Ressourcen pruefen. Danach passendes
   APK-/Firmwarepaar eindeutig zuordnen und den Geraeteablauf ausfuehren.
   Vor Installation Verfuegbarkeit, Chip und vorhandenen Flash-Rueckweg neu pruefen.
   Keine Geraeteabnahme aus Hostmodellen ableiten.

## Arbeitsbereich und Fortsetzung

`C:/Users/seide/.codex/worktrees/ble-rollen-profile/Larp Escape`,
Branch `codex/ble-rollen-profile`. Vom App-Worktreewerkzeug angelegt/angehaengt.
Der Hauptcheckout mit fremder `.vscode/extensions.json` und `.tmp/` bleibt erhalten.
Aktuellen Status und HEAD vor Fortsetzung in genau diesem Worktree pruefen.
Die lokale Berechtigungsgrenze erfordert bei dortigen Git-/Build-/Schreibbefehlen
gegebenenfalls gezielte Freigabe; kein Defekt im Produktcode.

## Frischer Softwarepruefstand

Gepruefter Codecommit: `6242960d34338acba557c1d93c59761f65e0eed4`.
Diese nachfolgende Dokumentfortschreibung aendert keine Produktquellen.

- `platformio test -e host-configured-control -e host-regression`: **635/635**
  erfolgreich, 13 Suiten, Exit0. Unveraenderte Firmwarequellen an obiger Basis;
  bestehende Rollen-, Profil-, Entzugs- und Fehlerfaelle, keine neue Funkpruefung.
- Android mit JDK21.0.8, vorhandenem SDK35 und Wrapper8.9, offline/no-daemon:
  `:core:protocol:test :app:testDebugUnitTest :gamemaster:testDebugUnitTest
  :app:assembleDebug :gamemaster:assembleDebug :app:lintDebug :gamemaster:lintDebug`:
  **BUILD SUCCESSFUL**, final 1m6s, 109 Aufgaben. Protokoll **105**, Dev **76**,
  GM **26** JVM-Faelle, jeweils 0 Fehler/Fehlschlaege/ausgelassene Faelle.
  Darunter **18 neue** Profilcodec-/Transaktionsfaelle; echte C++-Goldenwerte,
  CRC-Known-Vector, Grenzen, Quellbesitz, bewusste Uebernahme, Generationen,
  exakte Saved-Zaehler, verlorene/verspaetete Antworten und absolute Fristen.
- Beide Lintberichte: null Fehler; je drei Warnungen OldTargetApi,
  DataExtractionRules, ObsoleteSdkInt; Dev zusaetzlich eine Information
  AutoboxingStateCreation. Keine Ziel-SDK-/UI-Aenderung in diesem Paket.
- `git diff --check`: bestanden. Gegenpruefung der konkreten Korrekturen
  abgeschlossen; keine weiteren Befunde im begrenzten Diff. Der Gegenpruefer
  hat selbst keine Tests ausgefuehrt.

JUnit-/Lintberichte und APKs bleiben unter den jeweiligen ignorierten
`companion/android/**/build/`-Pfaden. Keine Releasebuild-, Installations-,
GATT-Funk-, Pairing-, Last- oder Geraetefunktionspruefung aus diesen Ergebnissen
ableiten. Der Nutzer hat den Arbeitsauftrag spaeter mit der Migrationsuebergabe geschlossen.

## Frisch abgeglichener privater Flash-Rueckweg

ESP32 an COM3: MAC `A8:42:E3:CD:4B:18`, 4 MiB, entspricht der dokumentierten Box.
Neue Vollsicherung gelesen und per `esptool verify_flash` am selben Chip
verifiziert, Ergebnis `verify OK (digest matched)`. SHA-256:
`b703f4b13aaaaa3e8c3bc68d8e8134b1cec0a77df957ea06175ac147d1d03c71`.
Damit bytegleich zur frueheren Sicherung vom 18.10.; keine Installation.
Identitaet/Read/Verify setzen die Box per RTS zurueck, schreiben aber nichts
in ihren Flash. Kein neuer BLE-/Spielgeraetenachweis.

Private Dateien: `C:/Dev/Larp Escape/private/ble-rollen-profile-20261028/`
mit `manifest.json`, `before-full-flash.bin`, `identity.log`, `read.log`,
`verify.log`. Nicht versionieren. Historischen Restorevertrag aus Issue #39
beachten: keine automatische Ruecksicherung, Chip/Hash/Laenge vorher neu pruefen.

## Zweiter Softwarebaustein: echter geschuetzter Firmware-Rueckkanal

Gepruefter Codecommit: `6d7edeebabbe281e06f8ca855827580e95aa0375`.
[Vertrag und Quellen](../docs/companion/gatts-output-v1.md). Root fuehrt
Plattform/Systemintegration; anderer Bearbeiter `/root/security_review` pruefte
Security, Originalbindung und Nachweisgrenzen. Kein eigener Testlauf des Reviewers.

`Esp32GattsSource` ist jetzt ein echter `IReplyFrameBearer`: ein fester
20-Byte-Mailboxplatz, nur der urspruengliche SDK-Aktor beantwortet geschuetzte
Reads. Eine aktuelle ausdrueckliche GM-Zuordnung und frische originale
SC/MITM/Bond-Security bleiben erforderlich. Rechte, Verbindungstoken,
Registrierung und Lifecycle werden nicht aus einer V2-HMAC-Anmeldung abgeleitet.
Ein leerer Read bestaetigt keinen Auftrag; Copied/SDK-Erfolg bedeuten keine
Peerzustellung. Frist <1000ms nach letzten SDKports, keine Blindwiederholung.
Die zusaetzliche 15-Handle-Variante ist optional; 5/7/9/13 bleiben erhalten.

Frische Ergebnisse am obigen Code:

- `platformio test -e host-configured-control`: **524/524**, Exit0, zuletzt15.40s.
  Darunter11 neue Outputfaelle mit kontrollierten Source-/SDK-Interleavings.
- `platformio test -e host-peer-proof` mit archiv-/quellengeprueftem echtem
  Mbed TLS2.28.4: **536/536**, Exit0,16.81s. Zwei zusaetzliche neue Nativefaelle
  fuer echte P256-Anmeldung, GM/kein GM, raw Profilantwort-Ausgabe und Entzug.
  Die Profilantwort ist synthetisch vom echten Encoder erzeugt: kein neuer
  Profilimport-/Apply-/Speicher- oder Geraetenachweis.
- `platformio run -e esp32-gatts-abi-probe -j 2`: **SUCCESS**. SDK-/Cryptopins
  bestaetigt. RAM50.904Bytes, Flash1.095.129/1.310.720Bytes. Dieses ABI-Ziel
  startet weder Bluetooth noch echte Profilgeraete; kein Produktbudgetnachweis.
- Architektur145Dateien/302Includes,86unabhaengige C++11-Header, Formatbaseline
  322Dateien mit clang-format19.1.7, `git diff --check`: bestanden.
- Unabhaengige Pythongegenstellen: Security53 und Dienst58 negative Beispiele
  abgewiesen; weiterhin explizite SDKmodelle, keine Funkmessungen.
- Eigener separater Xtensa-Compile mit tatsaechlichen PlatformIO-Argumenten und
  `-fstack-usage`: readOutputNow1024, copyReplyFrame464, outputScopeCurrent272,
  dessen Lambda80Bytes eigene statische Frames. Das sind KEINE Gesamttiefe
  und KEINE gemessene freie BTU-/Taskstackreserve.

Reviewkorrektur: letzter Alterscheck nach allen externen SDK-Securityports.
Ergaenzte Haertung: rekursiver Readguard und Ablehnung ueberlappender Output-
handles auch bei direkt konfigurierten unsortierten Authhandles. Alle konkreten
Reviewbefunde nach Quellnachpruefung geschlossen. Der erste zusaetzliche
Nativeversuch verwendete faelschlich einen festen signierten Nachweis in zwei
Verbindungsepochen innerhalb derselben Testfixture; getrennte frische Testfaelle
bestehen. Kein daraus abgeleiteter Firmwarefehler.

Lokale Werkzeuge/Kryptografiequellen unter `private/quality-tools` und
`private/peer-mbedtls`; Compilerargumente, .su-Datei, Object und Quellenhash unter
`private/output-stack`. Alles ignoriert; keine Schluessel oder Flashbytes versioniert.
Produktstart, Servernachweis, Einrichtung, Bond-Reconnect, Android-Polling,
reale Profilgeraete und das gemeinsame V2-Geraetepaar bleiben offen.

## Abschluss und erhaltene Machbarkeit

Nutzerentscheidung: Plattformmigration als eigener Plan; Arbeit und Grund fuer
den Maintainer im Projekt festhalten; dieses Ziel damit abschliessen. Keine
Migration, kein neuer Hintergrundauftrag oder neuer Chat und kein Merge/Release.
Der PR bleibt als begrenzte Vorbereitung ein Entwurf.

Zusaetzlicher Befund aus unabhaengiger Securitygegenpruefung und gepruefter
Hersteller-Advisory: Mbed TLS2.28.4 faellt in die Versionsmatrix von
CVE-2025-27810. Kein geeigneter SDK-Backport belegt. Der geplante TLS-Produktpfad
wird auf diesem Stand nicht freigegeben. Details und Prueftore stehen kanonisch
in der Migrationsuebergabe, nicht in einem persoenlichen globalen Skill.

Ein isolierter echter Mbed-TLS-/Python-OpenSSL-Hostversuch mit TLS1.2,
ECDHE-ECDSA-AES128-GCM-SHA256 und gepinnten vorbereiteten Zertifikaten bestand
neun Faelle: 1/7/20-Byte-Eingangsstuecke, falscher Client, falscher Serverpin,
fehlendes Clientzertifikat, manipuliertes Record, Klartexteinschleusung und Replay.
Bearbeitbare Quellen, Herkunft und Ergebnis sind im
[Quellarchiv](quellarchiv/ble-tls-feasibility-2026-10-28/README.md) gesichert.
Nur Machbarkeit auf dem alten Backend, keine Pruefung der CVE-Fehlerausloeser,
kein SMP/Bond-, Rollen-, Profil-, Speicher- oder Geraetenachweis.

Die geprueften Produktquellen 6242960/6d7edee bleiben unveraendert. Zuletzt
veroeffentlichter Stand vor dieser Abschlussdoku:4ee93f0, PR#100. Der Zielbranch
wurde separat nach ab7de9b aktualisiert (nur fuenf Audit-Dokumentdateien). Unser
Link wurde versetzt; `git merge-tree --write-tree` bestaetigte konfliktfreie
Kombinierbarkeit, ohne einen Branch zusammenzufuehren. Keine neue Produkt-CI
oder Geraeteabnahme aus diesem Dokumentvergleich ableiten.

Vor Abschluss erneut ausgelesen: Am PR-Head `4ee93f0` sind alle drei
Workflowlaeufe erfolgreich abgeschlossen:
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/40752621303),
[LED product](https://github.com/Sven401/Larp-Escape/actions/runs/40752621317),
[NFC probe](https://github.com/Sven401/Larp-Escape/actions/runs/40752621398).
Die nachfolgende Abschlussdokumentation hat ihren eigenen CI-Stand; diese
Ergebnisse werden nicht auf einen anderen Commit uebertragen.

Abschlussgegenpruefung durch `/root/security_review`: keine konkreten Falsch-
oder Ueberbehauptungen in Migrationsgrund, CVE-Abgrenzung, neun Hostfaellen,
offener Produktabnahme oder Nutzerentscheidung gefunden; drei Archivhashes
unabhaengig abgeglichen. Keine erneuten Tests oder Builds durch den Reviewer.
