# BLE, Rollen und Profile: aktive Produktintegration

Stand: **28.10.2026**, Sitzung `01a183bd-0df7-72e3-a67c-ddde84798a0b`.
Status: **begonnen; kein fertiger Produktanschluss und keine neue Geraeteabnahme**.
Basis: `f81956faffa4d4c6b0198fdbd0c626b21e137cd4`, nach frischem Fetch.
Zielbranch bleibt `plan/projektuebergabe-kuratierter-stand`.

## Auftrag, Verantwortliche und Grenzen

Nutzerziel: Android verbindet sich authentifiziert mit einer V2-Box, erkennt
bzw. ordnet eine autorisierte Rolle zu, laedt ein gueltiges Spielprofil und
fuehrt ausschliesslich freigegebene Bedienaktionen aus. Host-, App- und echter
Geraetenachweis einschliesslich Fehler und Wiederverbindung sind erforderlich.
Nutzerantwort am 28.10.: Geraete spaeter verfuegbar, zunaechst Software fertigstellen.
Keine Installation oder Geraetebeobachtung in diesem Zwischenstand.

Maintainer und fuehrendes Plattform-/Integrationsmandat: Hauptagent `/root`.
Unabhaengige Security-/Qualitaetsgegenpruefung des begrenzten Aenderungsumfangs:
Subagent `/root/security_review`, eigener lesender Durchgang. Keine externe
Zertifizierung. Varianten: gemeinsamer Android-Protokollkern, Dev und GM;
Firmware unveraendert. UI und Geraeteadapter noch nicht geaendert.

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
   Nur `ProbeReplyBearer` in `src/ProfileRuntimeProbe/main.cpp` ist vorhanden.
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

## Naechste konkrete Arbeit

1. Engen Originalbindungs-/Outputvertrag definieren und echten BLE-Antwort-/
   Statusbearer samt registriertem Attribut implementieren. Bei Notify/Indicate
   CCC und Sendebestaetigungen unterscheiden; alternativ einen vollstaendig
   beschriebenen Lesepfad. SDK-Aufrufe ausserhalb von Spinlocks.
2. Hostgegenfaelle: falscher Originalaktor, recycelter Handle, Entzug vor/nach
   Kopie und vor Sendung, volle Ausgabe, Disconnect, verspaetete Antwort,
   Ausgabefrist, unberechtigtes Lesen/Abonnieren. Betroffenen SDK-Build pruefen.
3. Geschlossenes Integrationsziel mit echten Registry-/Security-/Profilbesitzern
   zusammensetzen; Servernachweis, Einrichtung und Bond-Reconnect belegen.
4. Android-Transport und GM-Produktbedienung anschliessen, vorhandene
   UI/UX-Vertraege beachten. Aktionen nur aus aktueller autorisierter Sitzung
   und bestaetigtem gueltigem Profil anbieten; unbekannte Rollen ablehnen.
5. Reale Profilgeraeteadapter, Betrieb und Ressourcen pruefen. Danach passende
   APK/Firmware eindeutig kennzeichnen und Geraeteablauf vorbereiten. Fuer die
   spaetere Installation zuerst Geraeteverfuegbarkeit und chipgebundenen
   Flash-Rueckweg neu pruefen. Keine Geraeteabnahme aus Hostmodellen ableiten.

## Arbeitsbereich und Fortsetzung

`C:/Users/seide/.codex/worktrees/ble-rollen-profile/Larp Escape`,
Branch `codex/ble-rollen-profile`. Vom App-Worktreewerkzeug angelegt/angehaengt.
Der Hauptcheckout mit fremder `.vscode/extensions.json` und `.tmp/` bleibt erhalten.
Aktuellen Status und HEAD vor Fortsetzung in genau diesem Worktree pruefen.
Die lokale Berechtigungsgrenze erfordert bei dortigen Git-/Build-/Schreibbefehlen
gegebenenfalls gezielte Freigabe; kein Defekt im Produktcode.

Pruefergebnisse des zugehoerigen Commits werden unten ergaenzt. Gesamtziel bleibt
aktiv, auch wenn dieser erste Baustein erfolgreich getestet ist.
