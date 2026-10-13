# Konfigurierter Companion: vertikaler Hostschnitt

Ausführen: `pio test -e host-configured-control` (Unity 2.6.1, C++11, Werror).

Tatsächliche reine Firmwarequellen, tatsächlicher FreeRtosGameControlChannel, ProfileApplication mit privaten Profilen, eigener MemoryStore und synthetische kopierte Gerätebesitzer (aus src/ProfileTool/Activation.cpp abgeleitet). RTOS-Stubs kopieren die statischen FIFO-/Mailboxbytes und protokollieren Warteargumente/Sperrtiefe. Sie prüfen keine SMP-Zeitplanung, ESP32-Sperren oder physische Wirkung. Keine Test-API in privaten Controllern.

Beobachtungshooks setzen Rechtewechsel vor/nach dem letzten synchronisierten Read bzw. vor Admission. Das prüft die definierte Entscheidungsreihenfolge und ausdrücklich auch die zulässige einzelne beanspruchte Operation nach späterem Entzug. Es ist keine atomare Abbruchgarantie. MemoryStore bestätigt ausschließlich modellierte Bytes, keine Stromausfall-/Dateisystem-Synchronisierungsabnahme.

87 gezielte Fälle; keine vollständige JSON-/Profil-Fuzzing-/Ausfallsuite, keine vollständige Lease-/Request-/Sampleerschöpfung. Queuevoll/Reconnect/Entzug/Retry/Replyverlust sowie Start durch Codec bis StoredReady-Engine/Status sind enthalten. Historische Nicht-Prüfungen bleiben im Projektgedächtnis datiert; dieser neue Nachweis ersetzt sie nur für die genannten Fälle.

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

Acht Framequeuefälle verwenden auch die tatsächliche FreeRtosCommandFrameQueue
gegen dieselben kopierenden RTOS-Ports: begrenzte Callbackkopie/volle FIFO,
Outerwrite-/Frameprüfung, Quelle danach überschrieben, echte StoredGame/Reply,
Authoritywechsel, ursprüngliche Empfangsfrist mit Queuewartezeit/Uhrumlauf,
leerer Poll ohne Retry/GameTick und Entzug nach Copy-Beobachtungspunkt.
Kein SDKcallback-, RF-, GATT-Bestätigungs- oder SMP-Nachweis.

Neun Antwortframefälle prüfen Typ2/max511 gegen weiterhin strikte Command-Typ1-
Aufnahme, tatsächlichen aktuellen Replyqueuepfad, Goldenrekonstruktion,
Sender/Bearer-Busy/Bindung/Frist, Uhrumlauf und Message1..65535 plus Erschöpfung
und neue gültige Epoch. Fixed-Bearercopy ist ausdrücklich kein Peerempfang.
Erste zwei neue Reconnectfälle verwendeten falsch epoch1 aus dem alten
Fixturehelper; jetzt wird der aktuelle vertrauenswürdige Kanalstand verwendet.
\nAcht Aufnahmeantwortfälle: tatsächliche Framequeue/Stream bis Receiving/Queued,
Bestätigung vor Spielausführung, feste18Byte-Typ3-Referenzen, unabhängige
Gegenstelle, Busy-Verlust ohne Undo/Retry, volle Commandqueue und identisches
Fragmentretry, DecodeRejected ohne Requestverbrauch, Input vs andere
Busy-Teilnachricht, Frist/Uhrumlauf und alte Verbindung/Bearerentzug.
Erster Build sortierte die neue Prüfhilfe vor ihren Abhängigkeiten; Kommentar
erhält die erforderliche Include-Reihenfolge. Danach wurde der Code um
semantische Grenzen ergänzt; der Codec-Test benutzt nun passende komplette
und partielle Werte. Keine Firmwarefehler aus diesen Prüfhilfen behauptet.
Poll ohne Eingangsframe bietet keine Aufnahmeantwort an.

Vier Statusbindungscases: Producerstatus behält seine synchronisierte
Principal/Epoch, tatsächliche Mailbox filtert aktuellen GM ohne relabeln.
Reconnect bis neue Publikation, alte verzögerte Daten/wrong Principal,
False-Zielerhalt, Entzug vor Auswahl/Observer/Suspend/weiterlaufendes Spiel
und Entzug nach Producerobservation. Tatsächliche Statusfixtures lesen den
aktuellen Port, JSONbytes bleiben gleich. Kein physischer Sendebeweis.
