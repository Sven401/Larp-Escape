# Einrichtungsschlüssel: prüfbare Entscheidungsvorlage

Status: **Aufbewahrungsweg A vom Nutzer bestätigt, Umsetzung offen**, 31.10.2026.
Quelle: Antwort „Schlüsseldatei pro Box umsetzen (empfohlen)“ auf die konkrete
Aufbewahrungsfrage in Sitzung `01a192a7-694c-73b2-b7b4-05d67b091cf5`.
Container/KDF/Providerdetails und der physische Erstnachweis waren ausdrücklich
ausgenommen und bleiben eigenständige offene Ausarbeitungen.
Anlass: der vollständige manuelle Ersatzweg aus #93 benötigt tatsächlich
veranstaltungsbezogene GM-Rechte. Der bestehende Device-Registerkern beweist
diese Zuordnung noch nicht. Nutzerziel, Schlüsselwechsel und Recovery folgen
dem [bereits bestätigten Zugangskonzept](../companion/veranstaltungszugang.md);
Schlüsselformat und Erstbereitstellung sind dort ausdrücklich offen.

Arbeitsbasis `7a44ac502529cb66b45d6a802ff96b180012e8d3` mit
[Draft #135](https://github.com/Sven401/Larp-Escape/pull/135). Entwurf und
Koordination Root nach [UI/UX-Rolle](ROLLE.md), technische Ausarbeitung
`developer_p1a3`, gezielte unabhängige Sicherheitsprüfung `module_digest_review`.
Bestätigt ist die Aufbewahrungswahl; die weiteren technischen und gestalterischen
Aussagen unten sind eine Ausarbeitung, keine gerenderte Oberfläche,
Nutzerprüfung oder Geräteabnahme.

## Nutzer und Geltungsbereich

Der Box-Verantwortliche richtet eine konkrete Box ein, bewahrt deren separaten
Einrichtungsschlüssel auf und kann damit später Schlüsselwechsel, Veranstaltung-
wechsel oder Wiederherstellung anstoßen. Gewöhnliche Spielleitung bedient die
vorbereitete Station über die bestätigte Stationsauswahl; dieser Verwaltungsweg
verleiht keine zusätzliche Hauptadministratorrolle im laufenden Spiel.

Erste Einrichtung und Verlust eines Einrichtungsschlüssels sind unterschiedliche
Ausgangszustände. Ein Lesefehler oder unbekannter Gerätestand ist keine positiv
uninitialisierte Box. Vorhandene Rechte dürfen dadurch nicht überschrieben werden.

## Vorschlag A: separate verschlüsselte Schlüsseldatei pro Box

Für jede Box wird ein eigenes Setup-Schlüsselpaar außerhalb der Box erzeugt.
Die Box speichert nur den zugehörigen öffentlichen Prüfschlüssel und den gebundenen
Box-/Schlüsselstand. Die private Einrichtungsschlüsseldatei wird kennwortgeschützt
exportiert und vom Verantwortlichen getrennt aufbewahrt. Dateizugriff und Kennwort
sind für den späteren Wiederimport erforderlich. Eine bereits geöffnete Sitzung
belegt noch keine wiederherstellbare Sicherung. Ohne verfügbaren aktuellen
Einrichtungsschlüssel entsteht kein zusätzlicher datenerhaltender Ersatzweg.

Der private D0-Ursprungsschlüssel verbleibt separat auf der Box. Er wird weder
als Einrichtungsschlüssel umgedeutet noch für die Einrichtung aufs Handy exportiert.
Gewöhnliche Handy-Zugangsschlüssel bleiben ebenfalls eigenständige Credentials.

Vorteile: tatsächliches erneutes Öffnen und kryptografischer Verfügbarkeitsnachweis,
klare Boxbindung und kontrollierter Wechsel. Belastung: Verantwortliche müssen
Datei und Kennwort verfügbar halten; Offlineverfügbarkeit eines Cloudanbieters
ist nicht zugesichert. Eine Datei auf demselben Gerät belegt keine physische
Trennung des Backups.

Der konkrete standardisierte Verschlüsselungscontainer, KDF, Kennwortbehandlung
und Android-Providerweg werden vor Implementierung fachlich qualifiziert. Diese
Vorlage behauptet keine bereits getroffene Algorithmus-/Parameterentscheidung.

## Alternative B: separat aufbewahrter Papier-/QR-Schlüssel

Ein Papier-/QR-Schlüssel wäre eine andere Aufbewahrungs- und Wiederaufnahmeform.
Ablesen, Kopieren, Verlust, Sichtschutz und Wiederimport müssten eigens entworfen
und geprüft werden. Die Implementierung führt diese Variante nicht still zusätzlich
als Rescuekey ein. Auch sie ersetzt weder Boxprüfung noch aktuelle Autorisierung.

Die konkrete Produktentscheidung ist **A als vorgesehener Aufbewahrungsweg**.
Die Nutzerfreigabe umfasst keine zusätzliche Papier-/QR- oder Rescuevariante.

## Zustände und sichtbare Handlungen für A

| Zustand | Vorgeschlagene Anzeige | Erlaubte Handlung / beobachtetes Ergebnis |
| --- | --- | --- |
| Box ausgewählt, nicht unabhängig geprüft | „Box prüfen“ mit verständlichem Boxnamen | Prüfung vorbereiten; noch keine Zuordnung oder Rechteänderung. |
| Boxzustand unklar oder Lesefehler | „Einrichtungsstand konnte nicht geprüft werden“ | Erneut lesen oder abbrechen; kein Erstinitialisieren aus Fehler/Abwesenheitsvermutung. |
| Box bereits eingerichtet | „Box ist bereits eingerichtet“ | Aktuellen Einrichtungsschlüssel nachweisen; keine Neuvergabe über bloße USB-Verbindung. |
| Schlüsseldatei vorbereitet | „Schlüsseldatei sichern“ mit Boxname | Datei exportieren; der neue Schlüssel hat noch keine Einrichtungsrechte. |
| Arbeitskopie geschlossen | „Gesicherte Datei erneut öffnen“ | Tatsächlichen Wiederimport anfordern und eine neue Boxchallenge mit dieser Kopie beantworten. |
| Wiederimport nachgewiesen | „Datei geöffnet und geprüft“ | Software-Verfügbarkeit dieser Datei belegt; physische getrennte Aufbewahrung bleibt Verantwortung des Nutzers. |
| Ersteinrichtungsnachweis fehlt | „Bestätigung an der Box erforderlich“ | Der konkrete physische Nachweis muss noch festgelegt und implementiert werden; Übernahme bleibt geschlossen. |
| Exakte Box/Veranstaltung/erster Zugang vorbereitet | „Einrichtung bestätigen“ mit tatsächlichen Zielen | Bewusst die vollständige Zuordnung bestätigen; keine spätere Umdeutung auf eine andere aktuell ausgewählte Box. |
| Übernahme läuft | „Einrichtung wird übernommen“ | Erneute Betätigung erzeugt keinen neuen Auftrag und verlängert keine Frist. |
| Gerätestand bestätigt zurückgelesen | „Einrichtung bestätigt“ | Tatsächliche Box, Veranstaltung und erteilter Zugang werden bestätigt; danach normale Stationsbedienung. |
| Antwort/Übernahme unklar | „Einrichtungsstand unklar“ | Denselben Vorgang nach authentifizierter Wiederverbindung lesen; kein automatischer zweiter Wechsel/Reset. |
| Ausgangsstand oder Rechte geändert | „Vorbereitung ist nicht mehr gültig“ | Aktuellen Stand neu lesen und gegebenenfalls neu bewusst vorbereiten. |

Touchziele, Schriftvergrößerung, Screenreaderreihenfolge und nativer Dateidialog
brauchen spätere Compose-/Geräteprüfung. Es existiert hier noch kein visueller
oder Accessibility-Nachweis. Die Produktoberfläche zeigt keine Protokollfelder,
Krypto-Algorithmusnamen oder dauerhaften technischen Abnahmehinweise.

## Noch eigenständig zu entscheiden: erste vertrauenswürdige Boxzuordnung

Ein beliebiger selbstgelieferter PublicKey, Advertisingname, Box-ID oder eine
USB-Verbindung allein erfüllt den Erstnachweis nicht. Vorgeschlagen ist ein
beaufsichtigter physischer Erstmodus ausschließlich bei positiv fehlendem Anker,
mit lokaler bewusster Bestätigung und unabhängigem Abgleich der tatsächlichen Box.

Der konkrete lokale Nachweis hängt von der tatsächlich beauftragten Boxhardware
und ihrem geprüften Bedien-/Anzeigepfad ab. Diese Vorlage erfindet dafür weder
einen vorhandenen Taster/Display noch eine fabrikseitige PKI. Bis dieser Weg
konkret beschrieben, entschieden und tatsächlich angeschlossen ist, bleibt
produktive Erstinitialisierung geschlossen. Die Aufbewahrungsentscheidung A/B
allein genehmigt noch keinen unbekannten Enrollmentablauf.

Schlüsselwechsel K1–K5, Recovery W1–W6, Eventwechsel und Verlust folgen weiterhin
dem vorhandenen Vertrag: alter/neuer Gesamtstand, explizite CAS-/Rücklesung,
geschlossene Zwischenzustände, kein Wiederbeleben alter Rechte, keine automatische
Neuinitialisierung und keine datenerhaltende Umgehung bei Schlüsselverlust.

## Prüffälle vor Produktumsetzung

- Falsche Box, falsche Datei, falsches Kennwort und falsche Veranstaltung werden
  eindeutig abgegrenzt; keine erfolgreiche Meldung allein aus Appauswahl.
- Verfügbarkeit wird nach Schließen und tatsächlichem Wiederimport mit neuer
  Challenge durch den wirklich wiederimportierten Schlüssel geprüft; ein
  weiterlebender Signiercache und ein „gespeichert“-Klick genügen nicht.
- Boxwechsel, Disconnect, Ablauf, neue Rollen-/Registryrevision oder alter
  Vorgang dürfen keine aktuelle Einrichtung bestätigen.
- Lesefehler, beschädigter Anker und unklarer Commit bleiben geschlossen.
- Statusabfrage legt fremden Nutzern keine Auftragsbelege offen.
- Abbruch, Neustart, Antwortverlust und Stromausfall liefern eindeutigen alten,
  neuen oder ausdrücklich unklaren Zustand; keine still doppelte Befugnis.
- Privates Schlüsselmaterial erscheint nicht in Logs, Status, Paketen oder
  öffentlichen Testartefakten. Testschlüssel bleiben ausdrücklich synthetisch.

Die gezielte unabhängige Gegenprüfung durch `module_digest_review` am 31.10.2026
ergab keine offene P1/P2-Abweichung zum bestehenden Vertrag. Die Klarstellungen
zum tatsächlichen Wiederimport und zum vollständigen Recoveryweg W1–W6 sind
eingearbeitet. Das ist eine Konzept-/Vertragsprüfung, keine Produktfreigabe.
Nächster Schritt ist die konkrete technische Ausarbeitung des bestätigten
Dateiwegs; der physische Erstnachweis bleibt ein gesondert vorzubereitender
Entscheidungspunkt. Kein Upload,
Release, Schlüsselprovisionieren oder Gerätezugriff durch diesen Entwurf.
