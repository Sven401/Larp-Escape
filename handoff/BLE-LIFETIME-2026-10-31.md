# BLE-Verbindungsfrist und nebenläufiges Lebenszeichen

Status: Softwareteilschritt umgesetzt, 31.10.2026 (Europe/Berlin). Auftrag: begrenzter
Softwareteilschritt aus #27 bis zum geprüften PR; kein Merge oder Gerätezugriff.
Basis: `b8472542230c5834d9664e98a89eb46c1cba6e57` auf dem kuratierten
Integrationsbranch, einschließlich #101/#103. Sitzung
`01a192e0-a9a0-7660-a92f-2eb537d56ead`.

Federführung Firmware/Plattform: `/root`; unabhängiger Security-, Plattform-
und Qualitätsreview: `/root/ble_review`. `/root/native_environment` prüft
lokale SDK-Herkunft und Werkzeuge lesend. Betroffen sind nur der native
`NimblePeerSource` und sein CMake-Hostmodell; der bestehende TLS-/Grantpfad
wird nicht ersetzt. Pflichtnachweise: Rot/Grün-Modellfälle, echte native
PSA-/TLS-Suiten mit gepinntem SDK, nativer BLE-SDK-Build, Quellenwächter,
Format-/Architekturprüfungen und finale PR-CI. Geräteordnung bleibt offen.

## Konkrete Lücken und begrenzte Korrektur

Der bisherige Quelladapter hat keine eigene absolute Frist zwischen der
Annahme des erfolgreichen CONNECT-Callbacks und `ingress.open`. Insbesondere
ein bereits sicherer Link ohne Subscribe hält die einzige Verbindung auch
bei funktionierendem Hostpoll unbegrenzt. Das Hostmodell zeigt außerdem,
dass der Adapter ohne Securityrückmeldung selbst keine absolute Frist setzt.
Die Numeric-Comparison-Frist beginnt erst bei PASSKEY_ACTION, die TLS-Frist
erst beim späteren Workerstart. Keine davon begrenzt diesen früheren Abschnitt.

Die Korrektur setzt ab dem angenommenen CONNECT-Callback der Originalepoche
ein festes Budget von 30.000 ms bis zur Übergabe an den TLS-Worker. Das folgt den bestehenden
30-s-Budgets für Numeric Comparison und TLS, ist keine gemessene Funklatenz.
Security-/Subscribe-/Comparison-Ereignisse verlängern die Frist nicht.
Der gepinnte SDK besitzt bereits eine 30-s-SMP-Inaktivitätsfrist, erneuert sie
aber bei Verfahrensfortschritt; sie deckt einen sicheren Link ohne Subscribe
nicht mehr ab (`ble_sm.c`: `ble_sm_proc_set_timer`, `ble_sm_process_result`).
Hostpoll und ereignisgetriebene Öffnung prüfen sie, bevor ein positiver
Übergang oder eine Numeric-Comparison-Bestätigung ausgewählt wird. Ablauf
sperrt diese Epoche dauerhaft und fordert Disconnect an. Erst eine neue
Verbindung bekommt eine neue Epoche und ein neues Budget. Eine rechtzeitig
geöffnete Verbindung fällt anschließend unter die vorhandene TLS-Frist.
Die Zulassungsfrist wird vor/nach dem Descriptorlesen und nach `open` erneut
geprüft. Während dieser Übergabe bleibt sie auch für `current` unter derselben
Sperre sichtbar; ein überfälliger kopierter Workerauftrag bleibt gesperrt.

Zweite Lücke: `current`/`ready` lesen die Uhr vor dem Spinlock. Erneuert der
Host dazwischen sein Lebenszeichen, subtrahiert der Worker einen neueren
Zeitpunkt von einer älteren Beobachtung. Der bisherige unsigned Unterlauf
sperrt die Quelle fälschlich dauerhaft. Ein monotoner 64-Bit-Zeitpunkt und
explizite Reihenfolgeprüfung vermeiden diesen Unterlauf. Die Uhr bleibt
außerhalb des Spinlocks; echte beobachtete 100-ms-Ausfälle bleiben dauerhaft
gesperrt. Eine präemptierte Zeitablesung ist weiterhin ein Snapshot, keine
Zusage harter Echtzeit. Das 64-Bit-Budget beginnt am Callback, nicht an einer
behaupteten physischen HCI-/ACL-Geburt.

## Grenzen und bestehende Bindungen

Dies ist ein Anschluss vor dem TLS-Worker, kein neuer Bondbeweis oder Grant.
Epoche/Registrierung/Handle bleiben in allen kopierten Workerwerten erhalten.
Boot-, Credential- und Rollenbindungen des bestehenden Grantpfads bleiben
unverändert. Weder Linkverschlüsselung, Bond noch TLS gewähren GM-Rechte.
Der inaktive Firmware-Prüfeinstieg startet weiterhin weder Radio noch Enrollment.

Der Adapter erhält öffentliche GAP-Rückmeldungen mit einem Handle, keinen
vom Controller belegten Anwendungsepochentoken. Das Modell kann die
Abweisung alter *kopierter* Workerwerte bei gleichem Handle belegen; es kann
keinen beliebig verspäteten nackten SMP/HCI/ACL-Callback anhand derselben
wiederverwendeten Adresse/Handle sicher einer alten Verbindung zuweisen.
Diese tatsächliche Ordnung sowie Bondrestauration, Android-/Rollenanschluss,
Ressourcen und Funklauf bleiben S2/S3 und #27 offen. Keine synthetischen
Adressfelder werden als Gerätebeweis ausgegeben.

[Bestehende Tore S1–S6](BLE-BEREITSCHAFT-2026-10-28.md),
[TLS-Prüfvertrag](../test/test_native_tls/README.md),
[historischer Originalaktor-/Grantvertrag](../docs/companion/peer-security-sdk-v1.md).

## Prüfstand

Lokal am finalen Codebaum dieses Beitrags, 31.10.2026:

- **Rotprobe:** zehn neue Fälle gegen die unveränderte Produktquelle von
  `b847254`: neun erwartete Fehler (sieben Fristfälle, zwei Lebenszeichen-
  Interleavings); der erlaubte Zugang bei 29.999 ms bestand bereits.
- **Grünprobe:** 53/53 CTest-Einträge bestanden, darunter 44 BLE-Modellfälle
  (15 neue), 18 reale TLS-Gegenstellenfälle als Unterfälle eines Eintrags,
  echte PSA-/Grantregression und ECC-Guardpage sowie die bestehenden Kernsuiten.
  Fristgrenzen, verspätete Callbackauswahl, Handledoppelverwendung, alte
  Bestätigung/Ausgabe/Entzug, Disconnect, echte Lebenszeichenlücke und Ablauf
  während Descriptor-/Open-Übergabe sind getrennt enthalten. Die absichtliche
  Ausführung eines unbekannten Admission-Testnamens wird ebenfalls abgelehnt.
- **Herkunft:** echter SDK-Quellenwächter erfolgreich, auch unabhängig vom
  Reviewer ausgeführt; exakter ESP-IDF-Pin
  `9a97f6c54ec638111ce55cd36581b3c192f15207`, vollständige rekursive Submodule,
  keine tracked SDK-Änderung. NimBLE
  `b3c38720cecb56d6a36adde7be95fa0c70caa4e0`, Mbed TLS
  `ce3f3485a121c100f58f36d700cb35b060f6e866`. Sieben Quellenwächtertests bestanden.
- **Werkzeuge:** GCC 16.2.0, Python 3.12.6, explizit Cryptography 45.0.7;
  CMake mit `LARP_PSA_IDF_PATH`, anschließend Build und `ctest --output-on-failure`.
  Keine Kryptografiestubs als PSA-/TLS-Nachweis.
- **Hygiene:** Formatbaseline 341 Dateien mit clang-format 19.1.7,
  Architektur 151 Dateien/313 Includes, 89 unabhängige C++11-Header erfolgreich.
- **Andere Gegenprüfung:** Security-/Plattform-/Qualitätsreview des Diffs
  ohne verbleibenden P1/P2-Befund. SDK-Guard und gezielte Modellgegenfälle
  wurden vom Reviewer selbst ausgeführt. Seine Rückfrage zum Fristwechsel
  während `open` führte zu erneuter Kontrolle und zusätzlichen Regressionen.

Der zugehörige PR ordnet finalen Head, SDK-Imagebuilds und alle zwölf
PR-Pflichtchecks der tatsächlich geprüften Zielbasis zu. Die lokalen
CMake-/CTest-Protokolle liegen regenerierbar unter `private/host-build` und
`private/*tests.log`; die native CI bewahrt CTest-XML und SDK-Buildbelege.
Hostmodell und erzwungene Ablaufverschachtelung sind keine gemessene
Mehrkern-, SMP-/HCI-/ACL-, Bond- oder Funkabnahme. #27 und S1–S6 bleiben offen.
