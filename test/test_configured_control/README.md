# Konfigurierter Companion: vertikaler Hostschnitt

Ausführen: `pio test -e host-configured-control` (Unity 2.6.1, C++11, Werror).

Tatsächliche reine Firmwarequellen, tatsächlicher FreeRtosGameControlChannel, ProfileApplication mit privaten Profilen, eigener MemoryStore und synthetische kopierte Gerätebesitzer (aus src/ProfileTool/Activation.cpp abgeleitet). RTOS-Stubs kopieren die statischen FIFO-/Mailboxbytes und protokollieren Warteargumente/Sperrtiefe. Sie prüfen keine SMP-Zeitplanung, ESP32-Sperren oder physische Wirkung. Keine Test-API in privaten Controllern.

Beobachtungshooks setzen Rechtewechsel vor/nach dem letzten synchronisierten Read bzw. vor Admission. Das prüft die definierte Entscheidungsreihenfolge und ausdrücklich auch die zulässige einzelne beanspruchte Operation nach späterem Entzug. Es ist keine atomare Abbruchgarantie. MemoryStore bestätigt ausschließlich modellierte Bytes, keine Stromausfall-/Dateisystem-Synchronisierungsabnahme.

57 gezielte Fälle; keine vollständige JSON-/Profil-Fuzzing-/Ausfallsuite, keine vollständige Lease-/Request-/Sampleerschöpfung. Queuevoll/Reconnect/Entzug/Retry/Replyverlust sowie Start durch Codec bis StoredReady-Engine/Status sind enthalten. Historische Nicht-Prüfungen bleiben im Projektgedächtnis datiert; dieser neue Nachweis ersetzt sie nur für die genannten Fälle.

Die [Firmware-Fixtures v2](../../docs/companion/fixtures/v2/README.md) entstehen aus dem tatsächlichen Hostpfad und werden im zusätzlichen Fixturefall bytegenau verglichen.


Framing-Schnitt: tatsächlicher CommandFrame-Encoder/Decoder und CommandStream,
feste 1024-Byte-Nachricht, eine Verbindung, 2000-ms-Absolutfrist, Gap/Overlap/
Duplicate/Conflict/Busy, Reconnect/Entzug, QueueFull/Retry und höchste Länge.
Framing ist kein Pairing oder BLE-Adapter. Die 20-Byte-Probenwerte sind eine
lokale Testwahl, keine gemessene GATT-/Funkfähigkeit. Vollständiger Framingvertrag:
[command-framing-v1.md](../../docs/companion/command-framing-v1.md).


12 zusätzliche Journal-Ausfallfunktionen verwenden nur native Backendbytes und
deterministische Fehlerpunkte (MemoryJournalStore). Tatsächliche private
Firmwareprofile, Application, Journal, Importer und Eigentümerablauf bleiben
Produktionsquellen. Abgebrochene Schreibprefixe/Truncate/unklarer Commit,
alte bestätigte Auswahl, beschädigte/höhere/future/gleiche Generationen,
Read-/Close-/TOCTOU-Fehler und Generationmax werden geprüft. Keine physische
Dateisystem-/Stromausfall-/Flashhaltbarkeitsabnahme. Revisionsannahme des ersten
Laufs korrigiert: Importversuche erhöhen draftRevision, Abschluss der Aktivierung
verbraucht sie nochmals; beide Beobachtungspunkte jetzt getrennt geprüft.


Vier Antwortauswahlfälle: echte Queue, aktuelle Principal+Epoch unter kurzer
Authoritysperre, alte/wrong Metadaten verwerfen, False erhält Ziel, volle Queue
bei Entzug abräumen, kopierte ausgewählte Epoche nach späterem Entzug unverändert.
Keine Garantie einer nachträglich abbrechbaren physischen Übertragung.


Profilfähigkeiten: tatsächliche Firmwarelimits und Reader-/Writer-Vokabulare,
begrenzter Encoder mit NUL-/Exactfit-/Guardprüfungen, kanonisches Golden;
Rechnerwerkzeug --capabilities konsumiert denselben Encoder. Katalogschema und
Abgleich mit Profil-/Commandschemata in scripts/check_companion_fixtures.py.
Kompilierte Profilsprache ist getrennt von realer Geräteverfügbarkeit.
