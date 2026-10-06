# Konfigurierter Companion: vertikaler Hostschnitt

Ausführen: `pio test -e host-configured-control` (Unity 2.6.1, C++11, Werror).

Tatsächliche reine Firmwarequellen, tatsächlicher FreeRtosGameControlChannel, ProfileApplication mit privaten Profilen, eigener MemoryStore und synthetische kopierte Gerätebesitzer (aus src/ProfileTool/Activation.cpp abgeleitet). RTOS-Stubs kopieren die statischen FIFO-/Mailboxbytes und protokollieren Warteargumente/Sperrtiefe. Sie prüfen keine SMP-Zeitplanung, ESP32-Sperren oder physische Wirkung. Keine Test-API in privaten Controllern.

Beobachtungshooks setzen Rechtewechsel vor/nach dem letzten synchronisierten Read bzw. vor Admission. Das prüft die definierte Entscheidungsreihenfolge und ausdrücklich auch die zulässige einzelne beanspruchte Operation nach späterem Entzug. Es ist keine atomare Abbruchgarantie. MemoryStore bestätigt ausschließlich modellierte Bytes, keine Stromausfall-/Dateisystem-Synchronisierungsabnahme.

21 gezielte Fälle; keine vollständige JSON-/Profil-Fuzzing-/Ausfallsuite, keine vollständige Lease-/Request-/Sampleerschöpfung. Queuevoll/Reconnect/Entzug/Retry/Replyverlust sowie Start durch Codec bis StoredReady-Engine/Status sind enthalten. Historische Nicht-Prüfungen bleiben im Projektgedächtnis datiert; dieser neue Nachweis ersetzt sie nur für die genannten Fälle.

Die [Firmware-Fixtures v2](../../docs/companion/fixtures/v2/README.md) entstehen aus dem tatsächlichen Hostpfad und werden im zusätzlichen Fixturefall bytegenau verglichen.
