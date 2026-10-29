# Maintainer-Plan: Voraussetzungen und Fortsetzung der BLE-Produktintegration

Stand: **28.10.2026**. Status: **beauftragter Fortsetzungsplan; Voraussetzungen offen**.
Quelle: Nutzerauftrag in Sitzung `01a183bd-0df7-72e3-a67c-ddde84798a0b`:
„Schreib einen Plan für den Maintainer der dich nach Erfüllüng weiter arbeiten lässt.“
Dokumentbasis: BLE-Branch `8ac927f38cb4e46ec04c2ba05da589fc999bcd09`;
[PR #101](https://github.com/Sven401/Larp-Escape/pull/101) zuletzt ausgelesen
an `f26e87efc1d6ed9d1e73bceb3a8a2aab2009ca04`. Das sind Bezugsstände,
keine Vorabfreigabe späterer Commits.

## Auftrag und Verhältnis zum bisherigen Abschluss

Die bisherige Sitzung endete auf Nutzerauftrag mit einer Migrationsübergabe.
Die vollständige Produktintegration wurde dabei ausdrücklich nicht abgenommen.
Dieser Folgeauftrag ergänzt einen **bedingten Fortsetzungsauftrag**:
Sobald der Maintainer die unten genannten Softwarevoraussetzungen am tatsächlichen
Arbeitsstand nachgewiesen und dokumentiert hat, darf Codex die verbleibende
BLE-/Rollen-/Profilintegration ohne erneute allgemeine Umfangsfrage fortsetzen.
Bis dahin bleiben Umsetzung und Qualifikation der Plattformmigration im
separaten Migrationsauftrag. Jetzt wird nur dieser Plan erstellt.

Die Freigabe umfasst eigene Arbeitsbranches, Implementierung, Tests,
Dokumentation und Pull Requests gegen `plan/projektuebergabe-kuratierter-stand`.
Merges, Releases und grundlegende Produktentscheidungen bleiben beim Nutzer.
Geräteinstallation und Flashen benötigen weiterhin den konkreten Geräteauftrag
und den neu abgeglichenen chipgebundenen Rückweg. Der Plan richtet weder
Hintergrundüberwachung noch eine automatische Wiederaufnahme ein.

## Vorhandene Ergebnisse und ihre Grenzen

- [BLE-Übergabe](BLE-ROLLEN-PROFILE-2026-10-28.md) und
  [Entwurfs-PR #100](https://github.com/Sven401/Larp-Escape/pull/100):
  Android-Profilcodec/Transaktionskern sowie originalgebundener Firmware-Rückkanal.
  Produktstart, vollständiger Zugang und gemeinsame Geräteabnahme bleiben offen.
- [Migrationsplan](ARDUINO-ESP32-3-MIGRATION.md) und
  [Qualifikationsbericht in PR #101](https://github.com/Sven401/Larp-Escape/blob/f26e87efc1d6ed9d1e73bceb3a8a2aab2009ca04/docs/migration/esp-idf-qualification-2026-10-28.md):
  native IDF-Buildbasis, PSA-P256-Adapter und native Host-/SDK-Qualifikation.
  Der Bericht nennt Originalverbindung/SMP, TLS, reale Adapter und Persistenz
  weiterhin offen. Vier erfolgreiche CI-Workflows am ausgelesenen PR-Head
  belegen deren jeweiligen Umfang, keine vollständige Produktbereitschaft.
- Der archivierte TLS-Hostversuch auf dem alten Backend bleibt eine
  Machbarkeitsquelle. Er ersetzt keinen Nachweis auf dem neuen Backend.

## Verantwortung und Lieferform

Der Maintainer führt Plattform/Systemintegration und benennt je Paket
Verantwortlichen, anderen Gegenprüfer, Varianten und Nachweise nach dem
[Fachmandat](../docs/quality/verantwortlichkeiten.md).
Security-Review muss Zugang, Kryptografie und Vertrauensgrenzen ausdrücklich
abdecken; der eigene Umsetzungsdurchgang genügt nicht.
Geräteintegration verantwortet Adapter, Fehlerweitergabe, Speicher und Rückweg.
Für spätere konkrete Bedienabläufe gilt der bestehende
[UI/UX-Rollenvertrag](../docs/ui-ux/ROLLE.md).

Der Maintainer liefert einen versionierten Bereitschaftsbericht mit exaktem
Codecommit, SDK-/Komponentenpins, betroffenen Varianten, Testbefehlen,
Ergebnissen, CI-/Artefaktlinks, Reviewbefunden und verbleibenden Grenzen.
Für jedes Tor unten steht dort „erfüllt“ oder „offen“ samt konkretem Beleg.
Ein grüner Build allein schließt kein Laufzeit- oder Securitytor.

## Softwarevoraussetzungen für die Wiederaufnahme

| Tor | Vom Maintainer zu liefern | Prüfkriterium |
| --- | --- | --- |
| S1: reproduzierbare Plattform | Qualifizierter nativer SDK-/Kryptostand mit exakten Quellen, Komponentenauflösung und betroffenen Buildvarianten; bekannte Advisories für den tatsächlich genutzten Zugangspfad bewertet | Frische Host-/ESP32-Builds und SDK-Ablehnungsfälle erfolgreich; keine offene relevante Securitysperre. Alte Versions-/ABI-Guards werden nicht bloß abgeschaltet. |
| S2: ursprüngliche BLE-Verbindung | Tragfähiger Anschluss für Lifecycle, aktuelle SMP-Sicherheit, Verbindungsepoche und Task-/Callbackbesitzer auf dem neuen SDK; sichere Übergabe kopierter Daten | Gleiche Adresse/Handle nach Reconnect, späte Callbacks/Antworten, Entzug zwischen Prüfung und Ausgabe, schwache Sicherheit, volle Queues sowie Posting-/Registrierungsfehler geprüft. Keine Autorität allein aus Bondliste oder öffentlichem auth_success. |
| S3: geschützter Transportanschluss | Auf dem neuen Backend geprüfter Standard-TLS-Pfad mit fachlich geprüftem Transportvertrag; gegenseitige Gegenstellenprüfung, feste Grenzen und Fehlerverhalten | Echte Gegenstelle sowie falscher Pin/Client, fehlender Nachweis, Manipulation, Replay, Klartexteinschleusung, Fragmentierung und Abbruch geprüft. Kein ungeschützter Ausweichpfad. Der Transport verleiht allein keine Spielrolle. |
| S4: anschließbare Produktlaufzeit | Native Einstiegspunkte, Besitzer und Schnittstellen für Profilaktivierung und die im ersten vollständigen Spielprofil benötigten realen Geräteadapter; Fristen und Fehlervertrag | SDK-Build des tatsächlichen Integrationsziels; Fehler erreichen Verbraucher ohne positive Spielaktion. Synthetische Proben sind als solche ausgewiesen. Noch fehlende Zusammenführung in den Produktstart ist explizite Folgearbeit, kein verstecktes abgeschlossenes Tor. |
| S5: Daten und Rückweg | Vergleich von NVS-/LittleFS-/Bond-/Schlüssel-/Profilformaten und Partitionierung; sichere Behandlung inkompatibler Daten, Herkunft der Sicherungen und Wiederherstellungsverfahren | Kein stilles Autoformat/Erase; Schema-/Journalfehler werden abgelehnt. Historische Sicherung ist referenziert, vor Upload sind Chip, Inhalt/Hash und Rückweg erneut abzugleichen. Kein OTA-Rollback behauptet, wenn die Partitionierung es nicht erlaubt. |
| S6: geprüfte Arbeitsbasis | Gemeinsamer, konfliktgeprüfter Stand aus benötigten Teilen von #100/#101 mit Review und aktuellen Regressionen | Code-/Basiscommit und Herkunft jedes Teils genannt; unabhängige Befunde geschlossen oder ausdrücklich sperrend. Kein notwendiger Merge durch den Maintainer vorausgesetzt. |

S2 und S3 dürfen benötigte isolierte Proben verlangen. Wo die konkrete
SDK-Sicherheitsaussage nur am Gerät prüfbar ist, gehört dieser begrenzte
Gerätenachweis zum jeweiligen Tor; er ist nicht die spätere Gesamtspielabnahme.
Dafür notwendige Uploads werden konkret vorbereitet und gesondert beauftragt.

Nicht jede unabhängige Labor- oder Legacyvariante muss vor der ersten
BLE-Softwarearbeit vollständig migriert sein. Der Bereitschaftsbericht ordnet
jedoch jede betroffene Variante und ihre offenen Paritätstore zu.
Eine für den vorgesehenen Produktablauf benötigte Variante darf nicht als
unabhängig ausgeklammert werden. Fehlende Gesamtabnahme bleibt sichtbar.

## Konkreter Neustart nach erfüllten Toren

1. Maintainer dokumentiert S1–S6 am tatsächlichen Code und verlinkt den Bericht
   in der BLE-Übergabe. Offene Sperren verhindern die Freigabe; offene spätere
   Abnahmepunkte erhalten Verantwortlichen und nächsten Prüfschritt.
2. In diesem Chat beziehungsweise einer vom Nutzer beauftragten Fortsetzung
   den Bericht und diesen Plan übergeben. Falls die Zielsteuerung weiterhin
   „blockiert“ meldet, das unveränderte ursprüngliche Ziel wieder aufnehmen;
   dieses Dokument selbst startet keine Sitzung und ändert keinen Zielstatus.
3. Codex prüft Arbeitsbaum, Branch, aktuelle Belege und den Anschlussvertrag.
   Sind die Tore erfüllt, beginnt die Umsetzung direkt. Neu festgestellte
   Lücken werden konkret benannt; die frühere Abschlussentscheidung ist dann
   kein pauschales Hindernis mehr.

## Danach von Codex zu erledigen

1. Android-Zugang an den tatsächlichen BLE-/TLS-Anschluss anbinden:
   Einrichtung/Gegenstellenprüfung, aktueller Peer-Nachweis, explizite
   Rollenzuordnung aus dem autorisierten Register und sichere Wiederverbindung.
   Baugruppen-HMAC, TLS-Erfolg oder bloß AUTHENTICATED gewähren keine GM-Rechte.
2. Profiltransfer in den Produktablauf integrieren: echte Validierung,
   explizite Übernahme, reale Gerätebesitzer, dauerhafte Speicherung und
   korrelierte Rückmeldung. Kopiert, Entwurf, eingereiht und gespeichert
   bleiben unterscheidbar; kein blindes Wiederholen bei unklarem Ausgang.
3. Bedienaktionen konsequent anhand aktueller Rolle, Verbindung und
   Profilfreigabe ausführen. Entzug und Verbindungsverlust sperren neue sowie
   noch nicht autorisiert ausgewählte Aufträge; alte Antworten dürfen keine
   neue Autorität oder falsche Bestätigung erzeugen. Bereits autorisiert
   ausgewählte Hardware-/Apply-/Journaltransaktionen folgen ihrer bestehenden
   Abschluss- oder Rückwegpolitik. Das autonome Spiel läuft weiter; kein
   pauschaler Stop/Reset oder Rückgängigmachen. Manuelle Baugruppenausgaben
   behalten ihre eigenen Disconnectregeln. Maßgeblich sind
   [VA-01](../docs/veranstaltungsarchitektur.md) und die bestehenden
   Profil-/Geräteverträge. Bedienzustände nach UI/UX-Vertrag prüfen.
4. Passende Host-, Android- und SDK-Prüfungen auf dem gemeinsamen Commit
   ausführen; unabhängigen Security-/Integrationsreview durchführen.
5. Eindeutiges V2-App-/Firmware-Paar samt Prüfanleitung, Versions-/Hashbelegen
   und chipgebundenem Rückweg vorbereiten. Nach konkretem Installationsauftrag
   Geräteablauf durchführen und verbleibende gemeinsame Betriebs-/Lasttore prüfen.

## Vollständige Abnahme des ursprünglichen Ziels

Der Abschlussbericht enthält eine Nachweismatrix für mindestens:

- Android verbindet sich mit der vorgesehenen Box über den authentifizierten
  Produktweg; falsche Gegenstelle und Zugriffsfehler werden sicher abgelehnt.
- Autorisierte Rollen werden korrekt erkannt beziehungsweise explizit
  zugeordnet; unbekannte, nicht unterstützte und nicht berechtigte Rollen
  erhalten keine gesperrten Aktionen. Rechteentzug wird wirksam.
- Ein gültiges Spielprofil wird geladen, validiert, übernommen, gespeichert
  und mit realen Geräten wirksam; ungültige/abgebrochene Transfers verändern
  den letzten gültigen Zustand nicht.
- Freigegebene Bedienaktionen wirken im vorgesehenen Profil, gesperrte bleiben
  unwirksam. Antwort, tatsächliche Gerätewirkung und Nutzerbeobachtung sind
  getrennt zugeordnet.
- Trennung, Wiederverbindung, Wiederanlauf und verspätete Werte erzeugen
  weder alte Autorität noch doppelte Aktionen oder falsche Speicherbestätigung.
- Hosttests, App-Tests und dokumentierter realer Ablauf nennen denselben
  zugeordneten App-/Firmwarestand, Geräteidentität, Schritte, erwartete und
  beobachtete Ergebnisse sowie Fehlerfälle und verbleibende Grenzen.

Erst diese Nachweise erlauben den Abschluss des vollständigen BLE-Ziels.
Plattformqualifikation oder Erfüllung von S1–S6 erlauben die Weiterarbeit;
sie ersetzen nicht die Produktabnahme.


## Prüfung dieses Plans

Unabhängiger Quellenreview durch `/root/security_review` am 28.10.2026:
Ein Befund zur pauschalen Sperre laufender Transaktionen bei Verbindungsverlust
wurde nach VA-01 korrigiert und gegengeprüft; keine weiteren konkreten Befunde.
Lokale Dateiverweise, UTF-8 und Diff-Whitespace geprüft. Reine Dokumentänderung;
keine neuen Produkt-, SDK- oder Gerätetests und keine neue Abnahmebehauptung.
