# Veranstaltungsbezogener Zugang, Schlüsselwechsel und Wiederherstellung

**Status: abgestimmtes Sollkonzept, nicht implementiert oder abgenommen.**
Datum: **18. Oktober 2026**. Quelle: ausdrückliche Nutzerentscheidungen im
Planungsdialog dieser Sitzung, Bestätigung der Fehlerregeln und des ausgearbeiteten
Wechsel-/Wiederherstellungsablaufs sowie Auftrag zur Dokumentation und zum
Backlog-Abgleich. Codex-Sitzung: `01a14c0c-62b5-76a2-87de-fd0cd60291a5`.

Codebezug: Integrationsbranch `plan/projektuebergabe-kuratierter-stand`,
Commit `784984da05249c77525273c658405d2f3b8e8e24`. Die Bestandsgrenzen unten
wurden gegen Registerheader und bestehende Sicherheitsverträge gelesen; keine
neuen Software-, Funk-, Geräte- oder Stromausfallprüfungen durchgeführt.
Planungsaufgaben: [F2 #28](https://github.com/Sven401/Larp-Escape/issues/28),
[F1 #27](https://github.com/Sven401/Larp-Escape/issues/27),
[Androidanschluss #37](https://github.com/Sven401/Larp-Escape/issues/37),
[Produktplan #25](https://github.com/Sven401/Larp-Escape/issues/25).

## 1. Geltungsbereich und Rollen

Das freie, weiterverteilbare Projekt ermöglicht unabhängig betriebene
Veranstaltungen. Eine Veranstaltung umfasst mehrere Boxen und Handys; jede Box
gehört genau einer aktiven Veranstaltung an. Zugang und Rechteverwaltung
funktionieren ohne Internet oder zentralen Server.

| Befugnis | Abgestimmte Zuständigkeit |
| --- | --- |
| Box einer Veranstaltung zuordnen, Veranstaltung wechseln, Notfallwiederherstellung und Schlüsselwechsel | Box-Verantwortlicher mit separat aufbewahrtem, boxgebundenem Einrichtungsschlüssel |
| Spiel bedienen und Spielleitungsrechte vergeben/entziehen | Jede an der jeweiligen Zielbox aktuell berechtigte Spielleitung |
| Zugang zu einer weiteren Box | Eigene ausdrückliche Berechtigung an dieser Box; Veranstaltungszugehörigkeit allein genügt nicht |

Eine zusätzliche Hauptadministratorrolle ist nicht vorgesehen. Spielleitungsrechte
allein erlauben weder die Übernahme einer geliehenen Box für eine andere
Veranstaltung noch den Ersatz des Einrichtungsschlüssels.
Öffentliche Kennungen, bekannte Namen, Schlüsselbesitz ohne passende Zuordnung
oder eine bestehende Verbindung verleihen keine Spielleitungsrechte.

Es gibt **keine fachlich festgelegte Höchstzahl hinterlegter Handys**. Eine
notwendige technische Kapazitätsgrenze muss durch Speicherbedarf, Laufzeit oder
Speicherverfahren begründet und geprüft werden. Die unterstützte Kapazität muss
erkennbar sein. Bei Erschöpfung scheitert eine neue Vergabe eindeutig; vorhandene
Zugänge bleiben erhalten und werden nicht automatisch verdrängt.
Dies ist keine Zusage unbegrenzter technischer Kapazität oder gleichzeitiger
Bluetooth-Verbindungen.

## 2. Erste Einrichtung und Rechteverwaltung

1. Der Box-Verantwortliche prüft die richtige Box und weist die Berechtigung
   durch deren Einrichtungsschlüssel nach.
2. Er ordnet die Box ausdrücklich der Veranstaltung zu und berechtigt die erste
   Spielleitung. Der Abschluss bestätigt Box, Veranstaltung und erteilten Zugang.
3. Weitere Vergabe oder Entzug wird von einer **an der Zielbox aktuell
   berechtigten** Spielleitung bewusst bestätigt. Ein an einer anderen Box
   bestätigter Auftrag wird nicht automatisch verbindlich übernommen.
4. Jede betroffene Box bestätigt ihre eigene dauerhafte Übernahme. Ein
   veranstaltungsweiter Überblick unterscheidet **bestätigt, ausstehend und
   unklar**. Eine unerreichbare Box kann ihren bisherigen Rechtebestand weiter
   anwenden; dann darf kein veranstaltungsweit abgeschlossener Entzug behauptet
   werden.
5. Der letzte berechtigte Zugang **je Box** bleibt geschützt. Vollständiges
   Sperren erfordert deren Einrichtungsschlüssel. Die Wiederherstellung unten
   ersetzt alte Zugänge durch mindestens einen vorbereiteten neuen Zugang.

Rechteänderungen schließen den bisherigen Zugang; eine neu berechtigte Sitzung
verlangt einen frischen Nachweis. Der Entzug stoppt das laufende Spiel nicht.
Eine bereits ausgeführte Spielaktion wird nicht rückgängig gemacht. Kopierte,
noch nicht ausgeführte Aufträge dürfen nach Entzug nicht durch alte Rechte
autorisiert werden.

## 3. Gemeinsame Fehlerregeln und Übernahmepunkt

Jeder Vorgang ist an Box, aktuellen Berechtigungsstand und eindeutige
Vorgangskennung gebunden. Gleichzeitige Änderungen überschreiben sich nicht
still: Hat sich der Ausgangsstand geändert, muss die spätere Änderung erneut
geprüft und bewusst bestätigt werden.

Die Vorbereitung verändert noch keine Berechtigung. Es gibt einen verbindlichen
Übernahmepunkt: Vor ihm bleibt der bisherige Stand gültig, nach ihm der neue.
Nach Neustart muss ein vollständiger alter oder neuer Stand erkennbar sein;
gemischte oder beschädigte Stände dürfen keinen Zugang eröffnen. Die technische
Absicherung dieses Vertrags ist offen.

**Bestätigt** bedeutet dauerhaft übernommen und zurückgeprüft, nicht bloß
abgesendet oder in eine Warteschlange aufgenommen. Bei fehlender Bestätigung
bleibt das Ergebnis unklar. Vor einem erneuten Versuch wird der tatsächliche
Stand geprüft; keine automatische Wiederholung. Derselbe Vorgang darf keinen
zusätzlichen Wechsel auslösen oder alte Rechte erneut einsetzen.

Schlüsselmaterial gehört weder in Statusmeldungen noch in Prüfberichte.

## 4. Einrichtungsschlüssel wechseln

### Nutzerablauf

| Schritt | Handlung und beobachtbares Ergebnis |
| --- | --- |
| K1 Ausgangsstand | Verantwortlicher prüft Box und weist bisherigen Schlüssel nach. Er sieht die betroffene Box und den Umfang: nur deren Einrichtungsschlüssel. |
| K2 Vorbereitung | Neuen Schlüssel erzeugen und separat aufbewahren. Erneuter Nachweis seiner Verfügbarkeit vor Übernahme; bloßes Anklicken von „gespeichert“ genügt nicht. Der neue Schlüssel hat noch keine Einrichtungsrechte. |
| K3 Bestätigung | Bewusst bestätigen: alter Schlüssel verliert seine Einrichtungsrechte; Veranstaltung, Spielleitungszugänge, Profile und Spielstand bleiben erhalten. |
| K4 Übernahme | Box ersetzt den Schlüsselstand vollständig. Erst jetzt gilt der neue Schlüssel. Alte Verwaltungsberechtigungen und vorbereitete Vorgänge mit altem Schlüsselstand werden ungültig. Bestehende Spielleitungszuordnungen werden nicht entfernt. |
| K5 Prüfung | Neuer Schlüssel weist Berechtigung frisch nach; Box bestätigt aktuellen Schlüsselstand und Vorgangsabschluss. Alten Schlüssel erst nach eindeutiger Bestätigung aus der eigenen Aufbewahrung entfernen. |

### Unterbrechungspunkte

| Punkt | Geforderter Zustand und nächster Schritt |
| --- | --- |
| Vor/während K1–K2 | Alter Schlüssel bleibt gültig; Vorbereitung verwerfbar. |
| Neuer Schlüssel nicht nachweisbar verfügbar | Übernahme bleibt gesperrt. |
| Nach Vorbereitung, vor K4 | Abbruch verändert keine Rechte; alter Schlüssel bleibt gültig. |
| Während K4, auch Stromausfall | Nach Neustart vollständiger alter oder neuer Stand; kein Zustand ohne gültigen Schlüssel durch Teilübernahme und keine unbeabsichtigte doppelte Einrichtungsberechtigung. Beschädigter Stand bleibt geschlossen und ist ein gesondert zu behandelnder Fehler. |
| K4 erfolgt, Antwort/K5 fehlt | Unklar anzeigen. Erst mit vorbereitetem neuen Schlüssel Stand prüfen; gilt er noch nicht, mit bisherigem Schlüssel prüfen. Ablehnung oder Verbindungsfehler allein beweist nicht, dass der andere Schlüssel gilt. Kein automatischer zweiter Wechsel. |
| Neuer Schlüssel nach bestätigtem Wechsel verloren | Alter bleibt ungültig. Ohne aktuellen Schlüssel gilt Abschnitt 7; keine datenerhaltende Umgehung. |

Bei unklarer Übernahme dürfen beide Schlüssel nacheinander zur Prüfung angeboten
werden. Daraus entsteht keine Phase mit zwei administrativ gültigen Schlüsseln.

## 5. Wiederherstellung nach Verlust aller berechtigten Handys

Voraussetzung ist der weiterhin verfügbare Einrichtungsschlüssel der jeweiligen
Box. Dieser Vorgang wechselt weder Einrichtungsschlüssel noch Veranstaltung.

### Nutzerablauf

| Schritt | Handlung und beobachtbares Ergebnis |
| --- | --- |
| W1 Berechtigung | Verantwortlicher weist Einrichtungsschlüssel nach; Box bestätigt Identität und aktuelle Veranstaltung. |
| W2 Ersatz vorbereiten | Mindestens ein neues Handy erzeugt einen eigenen Zugang und weist Besitz seines zugehörigen privaten Schlüssels nach. Noch keine Spielleitungsrechte. |
| W3 Bestätigung | Ausdrücklich bestätigen: sämtliche bisherigen Spielleitungszugänge dieser Box sperren und vorbereitete Ersatzzugänge berechtigen; Profile, Spielstand, Veranstaltung und Einrichtungsschlüssel erhalten. |
| W4 Übernahme | Sperrung und Ersatz als vollständigen neuen Berechtigungsstand übernehmen. Kein freigegebener Zwischenstand mit alten und bereits neu berechtigten Zugängen. Alte Sitzungen und noch nicht ausgeführte Aufträge dürfen keine alten Rechte weiterverwenden. |
| W5 Prüfung | Neues Handy meldet sich frisch an und liest bestätigten Veranstaltungs-, Profil- und Spielzustand. Fehlende Abschlussbestätigung bleibt unklar. |
| W6 Weitere Boxen | Jede weitere Box separat behandeln. Der Bericht bestätigt ausschließlich tatsächlich wiederhergestellte Boxen. |

### Unterbrechungspunkte

| Punkt | Geforderter Zustand und nächster Schritt |
| --- | --- |
| Vor W4 | Bisherige Rechte bleiben unverändert; neue Zugänge noch unberechtigt. Verlorene Handys können also bis zur Übernahme noch berechtigt sein. |
| Ersatzzugang nicht nachweisbar verfügbar | Übernahme bleibt gesperrt. |
| Während W4, auch Stromausfall | Nach Neustart vollständiger bisheriger oder neuer Rechtebestand; kein gemischter Stand. Beschädigter Stand eröffnet keinen Zugang. |
| W4 erfolgt, Antwort fehlt | Mit Einrichtungsschlüssel oder vorbereitetem Ersatzzugang aktuellen Stand prüfen; keine automatische zweite Wiederherstellung. |
| Ersatzzugang funktioniert nach Übernahme nicht | Mit Einrichtungsschlüssel bestätigten Stand prüfen, gegebenenfalls neue bewusste Wiederherstellung vorbereiten. Alte Rechte werden nicht automatisch reaktiviert. |
| Späterer Verlust eines weiteren Handys | Andere gültige Zugänge bleiben erhalten. Vollständige Wiederherstellung ist kein automatischer Standardvorgang. |

Profile werden nicht gelöscht oder ersetzt. Der Spielstand wird weder
zurückgesetzt noch auf einen älteren Stand zurückgerollt. Die Wiederherstellung
löst keine Spielaktion und keinen automatischen Spielstart aus. Bei einem
weiterlaufenden Spiel bedeutet Erhalt nicht Einfrieren: Nachzuweisen ist, dass
der Verwaltungsablauf keinen zusätzlichen Reset, Rücksprung oder Spielbefehl
verursacht. Ein währenddessen erfolgter Stromausfall benötigt einen getrennten
Nachweis der allgemeinen Spielstand-Persistenz und Wiederanlaufpolitik.

## 6. Veranstaltung wechseln

Mit Einrichtungsschlüssel Zielveranstaltung und Zurücksetzen des bisherigen
Spielstands ausdrücklich bestätigen. Profile bleiben erhalten, bisherige
Veranstaltungsrechte werden gesperrt und neue Zugänge bewusst eingerichtet.
Dies ist ein anderer Vorgang als die datenerhaltende Wiederherstellung in
Abschnitt 5. Alleinige Spielleitungsrechte reichen dafür nicht aus.

## 7. Einrichtungsschlüssel verloren

Ersatz ist nur mit dem bisherigen Einrichtungsschlüssel möglich.
Spielleitungsrechte erlauben keinen Schlüsselersatz und keine Übernahme der
Box-Verantwortung. Ohne verfügbaren aktuellen Schlüssel gibt es keinen
datenerhaltenden Wiederherstellungsweg. Letzter Ausweg ist eine ausdrücklich
bestätigte vollständige Neuinitialisierung mit Datenverlust. Deren technischer
Auslöseweg, Schutz und Lösch-/Neueinrichtungsablauf sind noch zu spezifizieren;
hier wird kein versteckter Ersatz- oder Wiederherstellungsschlüssel eingeführt.

## 8. Abnahmekatalog für die spätere Umsetzung

Alle folgenden Kriterien sind **offen**, nicht bereits erbrachte Nachweise.

| ID | Zu prüfendes Verhalten |
| --- | --- |
| Z-01 | Erste Vergabe verlangt Einrichtungsschlüssel und bestätigt richtige Box/Veranstaltung/Zugang. |
| Z-02 | Jede aktuell berechtigte Spielleitung darf an der Zielbox Rechte verwalten; unberechtigte Handys weder Spiel noch Rechte. |
| Z-03 | Berechtigung an Box A gewährt keinen automatischen Zugang zu Box B. Änderung an B verlangt dort aktuelle Berechtigung und bewusste Bestätigung. |
| Z-04 | Entzug bleibt nach Neustart wirksam. Unerreichbare Boxen bleiben sichtbar ausstehend; unklare Ergebnisse werden nicht als Erfolg gezählt. |
| Z-05 | Letzter Zugang je Box bleibt ohne Einrichtungsschlüssel geschützt. |
| Z-06 | Gleichzeitige Änderungen erfordern bei verändertem Ausgangsstand neue Prüfung/Bestätigung; keine stille Verdrängung. |
| Z-07 | Technische Kapazitätsgrenze ist begründet, geprüft und erkennbar; volle Kapazität verdrängt keine bestehenden Rechte. |
| K-01 | Ohne gültigen bisherigen Schlüssel oder nachgewiesene Verfügbarkeit des neuen Schlüssels kein Wechsel. |
| K-02 | Nach bestätigtem Wechsel und Neustart gilt nur der neue Schlüssel; alte Verwaltungsnachweise/Wechselaufträge bleiben ungültig. |
| K-03 | Unterbrechungen an K1–K5 ergeben alten oder neuen vollständigen Stand. Fehlende Antwort führt zu Prüfung statt blindem Wechsel; doppelte Vorgangsübermittlung bewirkt keinen weiteren Wechsel. |
| K-04 | Veranstaltung, Spielleitungszuordnungen, Profile und Spielfortschritt bleiben durch Schlüsselwechsel unverändert. |
| W-01 | Wiederherstellung verlangt Einrichtungsschlüssel und mindestens einen nachweisbar verfügbaren Ersatzzugang. |
| W-02 | Nach bestätigtem W4 und Neustart bleiben alle alten Zugänge, alten Sitzungen und verspäteten Aufträge ohne Rechte. Neues Handy weist sich frisch nach. |
| W-03 | Unterbrechungen an W1–W6 ergeben vollständigen alten oder neuen Bestand; keine automatische Reaktivierung alter Rechte und keine blinde Wiederholung. |
| W-04 | Profile und Spielstand bleiben erhalten, kein zusätzlicher Reset/Rücksprung/Spielbefehl. Bereits ausgeführte Wirkung bleibt von Rechtebestätigung unterscheidbar. |
| V-01 | Veranstaltungswechsel verlangt Einrichtungsschlüssel, bestätigt Reset ausdrücklich, erhält Profile und sperrt alte Veranstaltungsrechte. |
| L-01 | Spielleitungsrechte umgehen verlorenen Einrichtungsschlüssel nicht; Neuinitialisierung verlangt ausdrückliche Bestätigung des Datenverlusts. |

Prüfberichte nennen Softwarestand, Boxzuordnung, Vorgangskennung, erwarteten und
bestätigten Ausgangs-/Endstand, Unterbrechungspunkt, Neustartverhalten sowie
Prüfung gültiger/gesperrter Zugänge und erhaltener Profildaten/Spielzustände.
Keine privaten Schlüssel protokollieren. Softwaretests belegen Abläufe im Modell;
physische Speicherung und Stromausfalldurabilität benötigen eigene Gerätenachweise.

## 9. Bestehender Kern und offene Umsetzungsentscheidungen

**Ausgelesen am Basiscommit:**
[PeerRegistry.h](../../include/Companion/PeerRegistry.h),
[bestehender Registervertrag](peer-registry-v1.md),
[Identität und Rechte](identity-access.md) und
[Registerakteur](peer-registry-actor-v1.md) liefern vorhandene Ausgangsbausteine.
Der Registerkern hat vier Einträge, einen 404-Byte-Datensatz und einen
vertrauenswürdigen lokalen Administrationsport. Das ist **keine Umsetzung** des
hier beschlossenen Veranstaltungsmodells, der Verwaltung durch Spielleitungen,
des Einrichtungsschlüsselwechsels oder der Notfallwiederherstellung.
Der bestehende Baugruppen-USB-/HMAC-Zugang bleibt ein eigener begrenzter Umfang.

Vor Implementierung sind insbesondere festzulegen:

- Einrichtungsschlüsselformat, sichere Erzeugung/Aufbewahrung, gegenseitige
  Geräteprüfung und konkreter Nachweis der Verfügbarkeit des Ersatzschlüssels;
- dauerhafte Veranstaltungs-/Berechtigungs-/Schlüsselstände, Transaktions- und
  Vorgangskennung, Konkurrenzprüfung, Schutz vor alten Aufträgen und Rollback;
- technische Sicherung von Übernahme, Rücklesung, Abbruch, beschädigtem Speicher,
  Statusprüfung bei unklarem Ausgang und Neustart;
- technisch begründete Registerkapazität und Formatmigration des bisherigen Kerns;
- sichere erstmalige Bereitstellung des boxgebundenen Einrichtungsschlüssels,
  Veranstaltungswechsel und vollständige Neuinitialisierung;
- Anschluss an allgemeinen Spielzugang F1/F2, spätere Anwendung und reale
  Speicherung sowie Spielstand-Persistenz bei unabhängigem Stromausfall.

Diese Entscheidungen, Implementierung, App-/Hardwarearbeit und Produktfreigabe
sind mit diesem Dokumentationsauftrag nicht erledigt oder beauftragt.
