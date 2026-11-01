# Eigene Owner-Runtime-Smokes

Stand: unveröffentlichter Owner-Zwischenstand, 01.11.2026. Tatsächlich geprüft
werden Core-Owner, privater ManualOverrideOwner, Journal, Bundle, Codec, Planner
und SerializedOrdering mit echtem SHA-256 (Windows BCrypt).

Installations-, Control-, Eingangs-, Actor- und Clockquellen sind ausdrücklich
synthetisch. Der Eingangsbelegport akzeptiert ausschließlich Pläne OHNE neue
Fakten und OHNE Hardwareintents: keine fingierten T2-Signaturpositiven, keine
GM-/Event-/Gerätefreigabe. Das Ausgangsprogramm stammt aus dem gespeicherten
unabhängigen Katalog; sein Ersatzguard wird anschließend im Test typisiert auf
false geändert und vollständig gemeinsam validiert. Das ist keine erneute
Artefaktprüfung dieses geänderten Programms.

Die Fälle prüfen fehlende Herkunft/fehlenden Livefence vor jedem Write,
widersprüchlichen positiven Scope plus Fehler, tatsächlich dauerhafte
NoRelease-Receipts, kalte historische Nachrechnung ohne Currentannahme oder
Neusignierung, Historical im Livepfad und späte semantische Recoveryfehler ohne
Präfixstatus. Echte Unknownfälle verwenden einen neuen Durable-Backend-Session-
und Journalbestand: nur inerter Catalog ist kein Controlcommit; ein tatsächlich
bestätigter Applicationfinal löst die exakt überlebende MetadataLease auf.

Aufruf: `python test/fixtures/rules-progress-owner-own/run.py --compiler <g++>`
oder `--binary <exe>`. Die getrennte unabhängige Gesamtkampagne ergänzt echte
COSE-/P256-Bytes; auch dort bleiben nicht implementierte Produktquellen sichtbar
synthetisch. Kein SDK-/SMP-/Langzeit-/Geräte-Powerlossnachweis aus diesen Smokes.
