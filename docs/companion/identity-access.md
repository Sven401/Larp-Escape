# Identitätslabels und ausdrückliche Spielleitungszuordnung

**Stand:** 7. Oktober 2026. **Status:** fester Rechtekern gegen tatsächlichen
Kanal/Application/Host und synthetische Firmwareprobe geprüft.
Echte kryptografische Verifikation, Provisionierung, Pairing, persistente
Rollen/Schlüssel und SDK-/Androidabnahme offen.

## Gemeinsamer Vertrag

Identity ist ein opakes, nichtnull16Byte-Registrylabel. Device- und Peerlabels
sind getrennte Werte. Kein Label ist ein Schlüssel oder Sicherheitsnachweis.
Ein vertrauenswürdiger Startpfad liefert das stabile Gerätelabel an begin;
es bleibt in diesem Boot unveränderlich. Invalides Label oder versuchte
Änderung sperrt den Kern bis zu einem neuen Objekt/Boot und schließt Authority.
Für Produktion fehlen Herkunft/Provisionierung und bestätigte Speicherung
dieses Labels. Die Probe verwendet ausdrücklich deterministische Testlabels.

[Öffentliches Metadatenschema](device-identity-v1.schema.json) und
[synthetische Referenz](fixtures/device-identity-v1.json) beschreiben
schemaVersion1,kind=device_identity,deviceId=32Upperhex,
scope=display_identity,requiresVerifiedConnection=true,
requiresGameMasterAssignment=true. Das tatsächliche encodeDeviceIdentity
verwendet einen festen256Byte-Ausgabevertrag; Fehler leert written/erstesByte.
decodeIdentityHex akzeptiert nur exakt32Upperhex, nichtnull und erhält das
Ziel bei Fehler. Tool: profile-tools --device-identity HEX32 erzeugt dieselben
echten Encoderbytes; ungültiger Input Exit2. Der Bediener liefert hier ein
Test-/Konfigurationslabel; die CLI liest keine Hardware und authentisiert
keine Verbindung. App darf Metadaten weder als Bondnachweis noch GM-Rolle
behandeln. Kein Peerlabel oder Authentisierungsmaterial wird veröffentlicht.

## Verifizierte Verbindung und vier ausdrückliche Rollenplätze

IdentityAccess besitzt feste vier volatile Zuordnungen Peerlabel→Principal,
einen aktiven BoundConnection und eine beobachtete TrustedAuthority.
assignGameMaster/revokeGameMaster sind ausschließlich vertrauenswürdige
lokale Administratoroperationen; dafür existiert kein App-/JSONwriteendpoint.
Keine automatische Zuordnung beim Connect oder erfolgreichen Bond.
Duplikate sind idempotent, gleiche Principal für mehrere Identitäten oder
stille Änderung einer vorhandenen Zuordnung wird abgewiesen. Ausdrücklicher
Entzug gibt einen Slot frei; ein anderer Peer erbt alte Rechte nicht durch
Wiederverwendung derselben Principal. Allgemeine Observerrechte unverändert
nicht eingeführt; aktuelle Spielantwort/Statusauswahl bleibt GM.

connect mintet seine lokale Generation aus einer neuen geschlossenen
Authorityepoch. BoundConnection besteht aus SDKhandle UND dieser Generation.
Der synchronisierte Kanal vergibt Epoch ausschließlich oberhalb seiner
höchsten beobachteten Epoch, einschließlich Konfliktfences. Kein clientseitiger
Counter, kein Null/Wrap. UINT32_MAX schließt weitere Vergabe. Reconnect darf
denselben Handle mit einer neuen Generation verwenden; alte Auth-/Disconnect-
Ereignisse verändern die neue Verbindung nicht. Zweite gleichzeitige
Verbindung meldet Busy, ohne laufende Rechte oder Ziel zu ändern.
Ein echter SDKadapter muss unbekannte weitere Verbindungen entsprechend
abweisen und die ursprüngliche Ereignisbindung beweisen.

VerifiedPeerEvidence kommt ausschließlich von einem vertrauenswürdigen
Verifizierer für exakt diese Verbindung und stabile Credentialregistry.
Policy verlangt nichtnullPeerlabel sowie encrypted/MITM/SecureConnections/
bonded. Diese booleschen Belege implementieren selbst keine Kryptografie!
Nicht aus JSON, Advertisingname, RawMAC oder bloßem Authsuccess erstellen.
Ohne ausdrückliche Zuordnung bleibt auch ein vollständiger Beleg ohne GM.
Fehlender Nachweis oder Identitätskonflikt sperrt diese Verbindung bis zum
Disconnect/neuen Connect. Späterer alter Erfolg darf sie nicht wieder öffnen.
Doppelte gleiche Verifikation erhält Epoch/Lease/Requestfolge.

Die gepinnte [ESP-IDF4.4.6 GAP-API](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/bluetooth/esp_gap_ble.html)
enthält Authcompletion mit Adresse/Typ/Erfolg/auth_mode und Bondverwaltung.
Diese Felder ersetzen keine geprüfte Zuordnung zu ursprünglichem Link und
Credential. Für den SDKschnitt bleiben Eventattribution, Privacy/Bondidentity,
Securitymodus, lokales Einrichtungsfenster und bestätigte Schlüsselspeicherung
konkret zu implementieren und physisch abzunehmen. Kein konstanter Passkey.

## Atomarer Vergabepunkt und Entzug

Für neue Verifikation und lokale Zuordnung an einem bereits verifizierten
Link wird advanceAuthorityIfCurrent benutzt. Der tatsächliche FreeRTOS-Kanal
vergleicht die ursprünglich beobachtete TrustedAuthority und vergibt die neue
Epoch unter EINER kurzen Authoritysperre. Ein zwischenzeitliches Suspend/
Lifecycleupdate verhindert die verspätete Vergabe; Fehler lässt das Ausgabeziel
unverändert und IdentityAccess sperrt den Link. Freies advanceAuthority dient
nur dem expliziten neuen Connect und schließenden Vorgängen. Rechteentzug darf
durch kein verlierbares Daten-FIFOereignis dargestellt werden; der spätere
SDKadapter muss Closure direkt publizieren und Datenbelege unverändert binden.

Diese wichtige Grenze wurde in der ersten neuen Implementierung tatsächlich
verletzt: freies advance konnte einen kurz zuvor gesperrten Kanal wieder öffnen.
Zwei gezielte Hostfunktionen waren rot (erwartet AuthorityUnavailable,
tatsächlich Verified bzw Assigned). Atomarer Vergleich plus Vergabe schließen
beide Fälle; final grün. Das betraf ausschließlich den neuen uncommitteten
Kern, keinen nachgewiesenen Fehler im vorherigen1e9610c.
Lokale Logs: %TEMP%/larp-identity-access-revoke-red-20261007.log und
%TEMP%/larp-identity-access-revoke-green-20261007.log.

route prüft originalen Handle/Generation, verifiziertes Peerlabel,
explizite Zuordnung und erneut den tatsächlichen Kanalstand.
False erhält das Ziel. Entzug schließt nur Zugang, das laufende Spiel bleibt.
GameControl beobachtet neue Authority und erzeugt neuen Lease-/Statusstand.
Frames tragen weiterhin originalen Principal+ConnectionEpoch; Empfänger-
auswahl, Streamadmission und Bearerprüfung bleiben zusätzliche Beobachtungspunkte.

## Vertikaler Nachweis und Grenzen

14neue native Funktionen, insgesamt113/113 erfolgreich/Exit0:
Codec/Golden/öffentlicher Scope; kanaleigene Epoch oberhalb Fault/Maximum;
vollständige Verifikation ohne Rolle abgewiesen, erst ausdrückliche
Zuordnung erlaubt echten gespeicherten Start und aktuelle Status/Replyframes;
jedes fehlende Securityflag/Label; idempotente Verifikation/Zuordnung/
Lease/Request; recycledHandle/alteAuth/Disconnect; Entzug vor Bearbeitung
bereits kopiertemStop bei weiterlaufendem Spiel; Konflikt/keine alte
Wiederöffnung; vierSlots/uniquePrincipal/Reuse; unveränderliches DeviceID;
Maximum/Suspend/Zielerhalt; die zwei atomaren Vergaberandfälle.

Erster Testlauf hatte eine zusätzliche ungültige Testvoraussetzung:
nach Entzug wurde ein Lease0-Befehl vom Fixtureencoder als ungültig abgewiesen,
bevor Rechteadmission erreicht wurde. Der Test verwendet nun die vormals
gültige kopierte Nachricht. Kein Firmwarefehler daraus.
Goldenexport im Erstlauf, final ohne Export; öffentliche Fixture blieb
bytegleich. Keine Fakeflags als tatsächlich geprüfte Verschlüsselung ausgeben.

Die synthetische SDK-Profilprobe verwendet genau IdentityAccess,
explizite Testverifikation und separate lokale Zuordnung vor Start. Sie
übernimmt Principal/Epoch aus route statt hartemEpoch1 und speist den
tatsächlichen CommandStream/Framequeue/GameControl/Reply-/Statuspfad.
Network=false, keine Schlüssel/Pairing/RF/GPIO und kein neuer Upload.
Toolgegenstelle zeigt tatsächliche publicEncoderbytes; unabhängige Python-
Schema-/Negativprüfung lehnt neun ungültige öffentliche Metadaten ab.
Fremder Appcode wird dadurch nicht geprüft oder geändert.

Registry und DeviceID sind hier flüchtig/vom Startpfad geliefert.
Persistente Provisionierung, Wiederherstellung, Adminführung, Credential-
verifikation, tatsächlicher Raw-SDK-Bearer, Androidtransport sowie physische
SMP-/RF-/Timing-/Stack-/Energie-/Flashabnahme bleiben offen.

## Abschließender lokaler Prüfstand

113/113native Funktionen/Exit0, %TEMP%/larp-identity-access-revoke-green-20261007.log.
Vier finale Builds SUCCESS/Exit0,
%TEMP%/larp-identity-access-builds-final-20261007.log:
Produktion55.148RAM/1.228.529Flash(93,7%),alteUSBprobe21.480/291.785,
Profilprobe107.328/395.001 plus57.272Profilheap/Allocator/FS.
Neue Profilprobe insgesamt160RAMbytes über1e9610c; tatsächliches ELF maßgeblich.
Architektur98Dateien/222Includes,62Header,164Formatdateien erfolgreich.
Achtv2+Katalogschema/Generator/Reply-/Receipt-/Statusgegenstelle gültig,
publicIdentity187exakteJSONbytes +neunSchemafehlerfolgen; tatsächliche CLI
bytegleich, drei ungültige Identityinputs Exit2/keinOutput.
Bestehende CLI13Befehlsframes/1732Katalogbytes unverändert bytegleich.

Tatsächliches finales Probe-ELF IdentityAccess144/BoundConnection8/
TrustedRoute12/Kanal1264Byte. Identity16/Evidence20Byte im Xtensacompiler.
Eigene -Os/-fstack-usage-Frames: begin32/connect48/authenticate32/assign48/
revoke64/route48/publish48, authority.advance48/advanceIfCurrent32,
publicIdentityencode112. Tatsächliche SDKadapterentryframes advance32/
conditional48 plus32Byte-Thunks, Probenloop864/setup224 plus Ketten.
Lokale Messquelle %TEMP%/larp-identity-access-budget-20261007 und SDKobjekte.
Kein gemessener Gesamtstack oder Mikrosekunden-/Funkbudget. Keine neuen
Cppcheck-/Remote-CI-/Geräte-/SMP-/Android-/Energie-/Flashausfallprüfungen.
Keine Schlüssel oder echten Peeridentitäten in Fixtures/Belegen.
