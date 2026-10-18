# Verbindliche Entscheidung: dezentrale Veranstaltungsarchitektur

Technische Ausarbeitung auf Integrationsbasis `5f90d10`:
[D0 v1: Identitäten, Ereignisse und Offline-Regeln](veranstaltung/d0-vertrag-v1.md),
[Format, Ursprungsvertrauen und Migration](veranstaltung/d0-sicherheit-migration-v1.md).
Status: Vertragsvorschlag zu #59; keine Implementierung. Die nachfolgenden
Beschluss-/Basisangaben bewahren die Herkunft von VA-01; #58/#64 sind inzwischen
über #69 integriert, der Integrationsabschluss über #70.

**Status: bestätigt, verbindliches Architektur-Soll; Umsetzung und Abnahme offen.**
Datum: **18. Oktober 2026**. Quelle: Nutzerimperativ „Infrastruktur ist
unzuverlässig“, anschließende Architekturberatung und ausdrücklicher Auftrag
„Halte den Infrastrukturgrundsatz und die empfohlene Architektur als verbindliche
Projektentscheidung fest“. Codex-Sitzung `01a14c2b-9779-7d03-a625-c11ca97197a2`.
Kennung der Entscheidung: **VA-01**.

Code-/Dokumentbasis: `d61f77fe753c1a837c13a67edb282d6a79610a30` auf
`plan/projektuebergabe-kuratierter-stand`, am 18.10.2026 nach Fetch geprüft.
Bestehender Zugangsbeschluss: [PR #58](https://github.com/Sven401/Larp-Escape/pull/58),
Commit `2f5fba6c574716b0764f680a0e4c06839dc973e3`,
[Veranstaltungszugang](https://github.com/Sven401/Larp-Escape/blob/2f5fba6c574716b0764f680a0e4c06839dc973e3/docs/companion/veranstaltungszugang.md).
Dieser Beschluss ergänzt ihn; die Zentrale erhält dadurch keine zusätzlichen
Administrationsrechte. PR #58 ist auf dieser Basis noch nicht integriert.

## 1. Infrastrukturgrundsatz und Zuständigkeiten

1. Eine Veranstaltung umfasst mehrere eigenständig laufende Boxen und mehrere
   Spielleiter-Handys. Gemeinsames flächendeckendes WLAN, Internet, erreichbare
   Zentrale und kontinuierliche Box-zu-Box-Verbindungen sind keine Voraussetzung
   für den vorbereiteten lokalen Spielbetrieb.
2. Jede Box besitzt ihren verbindlichen lokalen Spielzustand und entscheidet
   allein über dessen Änderungen. Ein serialisierter Anwendungseigentümer
   verarbeitet Sensoren, berechtigte BLE-Aufträge und geprüfte externe Ereignisse.
   Funkcallbacks führen weder Spiellogik noch Hardwarewirkungen direkt aus.
3. Handy–Box bleibt BLE. Mehrere Spielleitungen bedienen gleichzeitig verschiedene
   Boxen. Zunächst genau eine aktive steuernde BLE-Sitzung je Box, mit bewusster
   Übergabe. Gleichzeitige Steuerung derselben Box durch mehrere Handys ist kein
   Ziel dieses Ausbaus. Die Zahl gespeicherter Berechtigungen ist davon unabhängig.
4. Box–Box verwendet ESP-NOW und, wenn verfügbar, WLAN/IP. Beide transportieren
   denselben fachlichen Nachrichtenvertrag. Es gibt keine zwingende Route über
   Zentrale, Router oder ein führendes Handy.
5. Die gemeinsame Veranstaltungszentrale ist ein eigener dauerhaft speichernder
   Knoten. Sie verwaltet veröffentlichte Konfigurationsstände, sammelt Beobachtungen,
   vermittelt Nachrichten und verfolgt Aufträge. Sie ist keine Laufzeitinstanz des
   Boxspiels und kein globaler Sperr- oder Berechtigungsdienst. Ihre Hardware und
   Bedienoberfläche sind noch festzulegen.

## 2. Fachliche Identität und lokale Autorität

Veranstaltung, physische Box, logische Station, Spieldurchlauf, Spielleiter und
Handy/Zugang sind getrennte Begriffe. Eine Box gehört genau einer aktiven
Veranstaltung an. Station–Box-Zuordnungen müssen versioniert sein; ein Ersatzgerät
erbt weder stillschweigend Rechte noch ungeprüft den Spielstand. Konkreter
Ersatz-/Migrationsablauf bleibt zu spezifizieren.

Komponenten-IDs im bisherigen Profil bezeichnen Hardware innerhalb einer Box.
Sie sind keine veranstaltungsweiten Box- oder Stationskennungen. Bootkennung,
BLE-Lease, Requestnummer, Profilrevision, Zuordnungsgeneration und Spieldurchlauf
bleiben unterschiedliche Werte.

Jede fachliche Tatsache hat einen zuständigen Ursprung: A bestätigt ihren
Rätselerfolg; B entscheidet anhand vorbereiteter Regeln über ihre eigene Freigabe.
Es gibt keine überall beschreibbare gemeinsame Zustandsmaschine, keine globale
Reihenfolge aller Ereignisse und keinen automatischen Konfliktentscheid anhand
von Handy-/ESP-Uhrzeiten. Lokale konkurrierende Bedienaufträge bleiben durch
Sitzung, Revision und geordnete Verarbeitung abgesichert.

## 3. Ereignisse, Zustandsbeobachtungen und Aufträge

| Art | Verbindliche Bedeutung |
| --- | --- |
| Dauerhafte Tatsache | Beispielsweise „A hat Aufgabe X in Durchlauf R gelöst“; dauerhaft festhalten, später übertragen, mehrfachen Empfang vertragen. |
| Zustandsbeobachtung | Momentaufnahme mit Quelle, Quellenfolge und lokalem Empfangsalter; neuerer Stand ersetzt älteren. Empfang über Vermittler beweist keine aktuelle Erreichbarkeit des Ursprungs. |
| Zeitkritischer Auftrag | Beispielsweise unmittelbarer Toneffekt; braucht Zielbindung, Ausführungsbedingungen und Gültigkeitsregel. Keine beliebige spätere Ausführung nach einer Funklücke. |

Jedes Ereignis erhält eine stabile Kennung aus Veranstaltung, Ursprungsbox,
Ursprungsepoche und Sequenz sowie den fachlich passenden Durchlauf-, Zuordnungs-
und Versionsbezug. Kennungen dürfen nach Neustart, Speicherbereinigung oder
Wiederherstellung nicht versehentlich wiederverwendet werden. Das genaue Format
und dessen dauerhafte Vergabe werden im Folge-Vertrag festgelegt; dies ist noch
kein neues Wireformat.

Wenn B ein neues Ereignis von A nicht empfangen hat, darf B dessen Inhalt nicht
erraten. Jede boxübergreifende Regel benennt ihr Verhalten bei fehlender Nachricht:
weiter lokal spielen, ausdrücklich warten oder vorbereiteten Ersatzweg beziehungsweise
bewusste lokale Spielleiterfreigabe anbieten. Die jeweilige Freigabe wird als eigener
Vorgang nachvollziehbar; Wiederverbindung darf sie nicht doppelt anwenden.

## 4. Speicherung und Abgleich nach Unterbrechungen

Jede Box benötigt ein begrenztes dauerhaftes Ereignisjournal, eine Ausgangsablage
und Wiederholungserkennung. Relevante lokale Ergebnisse werden vor ihrer bestätigten
Veröffentlichung dauerhaft festgehalten. Bei erneutem Kontakt vergleichen Knoten
ihre Empfangsstände je Ursprung und übertragen fehlende Inhalte. Ein bloßer
Höchstzähler reicht bei Lücken nicht als Vollständigkeitsnachweis.

Die Zustellung erfolgt mit Wiederholungen; es wird keine garantierte Zustellung
ohne späteren geeigneten Kontakt versprochen. Dauerhaft angenommene fachliche
Ereignisse dürfen je Ziel und Durchlauf nur einmal wirksam werden, auch nach
Neustart oder Wechsel des Transports. Eingangsbeleg, lokale Zustandsänderung und
ausgehende Folgeereignisse benötigen einen abgestimmten Wiederanlaufvertrag.
Für physische Effekte wird keine allgemeine Exactly-once-Garantie behauptet;
Absturz zwischen Wirkung und Speicherung kann einen unklaren Ausgang erzeugen.

Bestätigung unterscheidet lokale Aufnahme, dauerhafte Annahme, fachliche
Verarbeitung und beobachtete physische Wirkung. Der Sender löscht auf Grundlage
einer bloßen Funkquittung kein einziges notwendiges Wiederanlauf-/Übertragungsdatum.
Quittierung, Aufbewahrungsdauer, Zusammenfassung, Lückenerkennung, volle Speicher,
Flashverschleiß und Wiederanlauf nach beschädigtem Speicher sind zu spezifizieren.
Unbegrenzte Offlinezeit bei endlichem Speicher wird nicht zugesagt. Erschöpfung
muss sichtbar sein; notwendige Tatsachen dürfen nicht still verschwinden.

## 5. Transport, Weiterleitung und Funkbetrieb

ESP-NOW dient kleinen Ereignissen/Abgleichen; verfügbares WLAN/IP ergänzt größere
Abgleiche und Ressourcenübertragung. Beide Wege verwenden identische fachliche
Kennungen und gemeinsame Wiederholungserkennung. Transportaufträge sind begrenzt,
nicht blockierend und vom Spieltakt getrennt. Priorisierung, Flusskontrolle und
Wiederholungsabstände verhindern Verdrängung lokaler Bedienung/Hardware.

Zunächst direkte Kontakte; Weiterleitung anschließend gezielt und begrenzt mit
Kennungserkennung, Weiterleitungsgrenze und Speicherbudget. ESP-NOW wird nicht
als bereits implementiertes Mesh oder garantierte Reichweite behandelt.

Vorbereiteter Veranstaltungskanal, kontrollierte WLAN-Kontaktphasen und geprüfte
Koexistenz mit BLE sind Planungsgrundlage. Der konkrete Kanal, Such-/Kontaktplan,
unterstützte Peerzahl, Rahmen-/Transfergrenzen und Budgets sind anhand des
gepinnten SDKs und der Hardware festzulegen. Anschluss an beliebige WLANs darf
nicht unbemerkt ESP-NOW-Nachbarschaften zerstören.

Technischer Quellenbezug: [ESP-NOW im gepinnten ESP-IDF 4.4.6](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/network/esp_now.html)
(MAC-Erfolg ist keine Anwendungsbestätigung),
[ESP-NOW/WLAN-Kanalbindung](https://github.com/espressif/esp-faq/blob/master/docs/en/application-solution/esp-now.rst),
[BLE/WLAN-Koexistenz als allgemeine Hardwaregrundlage](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-guides/coexist.html).
Der Koexistenzüberblick ist keine Freigabe einer SDK-Migration.
Funkkapazität und Latenzen benötigen reale Messungen; Quellenlektüre ersetzt sie nicht.

## 6. Zentrale, Konfiguration und Rechte

Die Zentrale zeigt pro Box bestätigten Stand, Quelle und Beobachtungsalter sowie
ausstehende/unklare Ergebnisse. Sie besitzt während einer Netztrennung keinen
garantiert aktuellen Gesamtüberblick. Zentrale Sammelaufträge zerfallen in separat
bestätigte Zielaufträge. Ein atomarer globaler Sofortstart/-stop ist nicht zugesagt.
Zeitkritische Aufträge werden nicht ohne aktuelle Gültigkeitsprüfung nachgeliefert.
Ein allgemeiner Not-Aus über unzuverlässigen Funk wird daraus nicht abgeleitet.

Profile, Regeln und benötigte Ressourcen werden vorab lokal verfügbar gemacht.
Veröffentlichtes Veranstaltungspaket, lokale Entwürfe und tatsächlich aktivierter
Boxstand bleiben getrennt. Veröffentlichungsrevision allein beweist keinen gleichen
Inhalt; Inhaltsbindung und Kompatibilität sind zu prüfen. Eine neue Konfiguration
verdrängt keinen noch benötigten Durchlauf-/Ereignisbezug. Automatische gemeinsame
Aktivierung über getrennte Knoten ist nicht vorgesehen.

Rechte werden lokal geprüft. Die Regeln aus PR #58 zu Einrichtungsschlüssel,
Vergabe/Entzug je Zielbox, Schutz des letzten Zugangs, Wechsel und Wiederherstellung
bleiben gültig. Weiterleitung eines Verwaltungswunsches ist keine Zielbestätigung
und keine automatische Rechtevergabe. Eine getrennte Box kann unbekannte neue
Entzüge nicht anwenden; der Überblick bleibt entsprechend ausstehend.

Boxereignisse benötigen verifizierbaren Ursprung, Veranstaltungs-/Durchlaufbindung
und Wiederholungsschutz auch über Vermittler. Ein Transportpeer darf keine
fremde Freigabe erfinden. Schlüsselverfahren, Einführung/Entzug von Boxvertrauen,
Offline-Prüfung und Prüfung alter nachgelieferter Ereignisse bleiben technische
Folgeentscheidungen. Es werden keine universellen Veranstaltungsschlüssel oder
neuen zentralen Administratorrollen festgelegt.

## 7. Konsequenzen und Änderungsweg

Verbindlich sind die Zuständigkeiten und Ausfallregeln oben. Die
[Vertrags- und Arbeitspaketmatrix](veranstaltungsarchitektur-umsetzung.md)
ordnet bestehende Verträge, Backlog und offene Abnahmen zu. Vorhandene Wire-/JSON-
Versionen werden durch diesen Beschluss nicht still verändert. Neue Formate
benötigen eigene Versions-/Fähigkeits-/Migrationsverträge und negative Fixtures.

Die vorhandenen Baugruppenbelege bleiben erhalten. BLE-Abbruch beendet weiterhin
die ausdrücklich manuell bedienten Ausgaben gemäß Baugruppenvertrag; daraus folgt
kein automatischer Stop des eigenständigen Spiels. Das Profiljournal speichert
Profile und ist noch kein dauerhaftes Spielfortschritts-/Ereignisjournal.

Die Entscheidung wurde dokumentiert und gegen Quellen/Backlog abgeglichen.
Keine neue Implementierung, Produktsoftwareausführung, Installation, Funk-, Geräte- oder
Stromausfallabnahme. Konkrete Speicherformate, Kryptoverfahren, Zentrale-Hardware,
Funklimits und lokale Ersatzregeln je Spiel müssen in den Folgepaketen geklärt
werden. Änderungen an VA-01 verlangen eine ausdrücklich dokumentierte neue
Produktentscheidung; eine Implementierungsgrenze hebt sie nicht still auf.
