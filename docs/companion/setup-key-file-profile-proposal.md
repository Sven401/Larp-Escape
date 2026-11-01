# Einrichtungsschlüsseldatei: bestätigtes Profil v1

Status: **Dateiformat v1 vom Nutzer bestätigt; Produktintegration offen**,
01.11.2026. Quelle: Antwort „Geprüftes Dateiformat v1 übernehmen (empfohlen)“
in Sitzung `01a192a7-694c-73b2-b7b4-05d67b091cf5` auf den konkreten Vorschlag
JWE/PBES2-HS256+A128KW/A256GCM, 600000 Ableitungsschritte und Passphrase ab
16 Zeichen. Dies ergänzt die zuvor bestätigte
[kennwortgeschützte Datei pro Box](../ui-ux/SETUP-SCHLUESSEL-VORLAGE.md).
Physische Erstbereitstellung und produktive Rechtevergabe waren ausdrücklich
ausgenommen und bleiben offen. Der Dateiname bewahrt den ursprünglichen Entwurf;
die unten festgelegten Formatparameter sind jetzt beschlossen.
[Tatsächlich ausgeführte Vorprüfungen](setup-key-file-qualification.md):
111 JDK-Modultests einschließlich unabhängiger Python-Kryptografievektoren,
vollständiger begrenzter Parser und 2029 Checks der isolierten Android-Probe
auf dem bereitgestellten moto g14/API34. Diese Nachweise betreffen ausschließlich
synthetische Testdaten. Produktintegration, nativer Datei-Wiederimport,
UI-Lebensdauer und physischer Erstnachweis bleiben offen; andere Android-
Versionen/OEMs sind dadurch nicht qualifiziert.

## Bestätigter Container

JWE Compact, ausschließlich `PBES2-HS256+A128KW` und `A256GCM`; verschlüsselter
Inhalt ist ein einzelner P-256-PrivateKey als PKCS #8 DER. Passwortkodierung ist
exaktes gültiges UTF-8 ohne Normalisierung. PBES2-Salt ist exakt
`UTF8(alg) || 0x00 || p2s16`; PBKDF2-HMAC-SHA256 leitet den **16-Byte-A128KW-KEK**
ab, nicht den 32-Byte-CEK. GCM-AAD ist exakt die ASCII-Bytefolge des **originalen
ersten Compact-Base64urlsegments**. Weder dekodiertes JSON noch eine erneut
serialisierte Headerversion darf diesen Wert ersetzen.
[RFC 7518 §4.8](https://www.rfc-editor.org/rfc/rfc7518.html#section-4.8),
[RFC 7516 §3.1](https://www.rfc-editor.org/rfc/rfc7516.html#section-3.1).

Das bestätigte **Projektprofil v1** legt fest:

- Höchstens 4096 Dateibytes, genau fünf ungepaddete kanonische Base64urlteile,
  kein BOM, Whitespace, Zeilenumbruch oder angehängter Inhalt.
- Geschützter JSON-Header höchstens 1024 dekodierte Bytes; feste Feldmenge
  `alg,enc,p2s,p2c,typ,cty,box,kid,jwk`, unabhängig von Feldreihenfolge.
  Doppelte/fehlende/unbekannte Felder werden vor KDF verworfen; keine URLs,
  zusätzlichen Empfänger, `zip`, `crit` oder Algorithmusauswahl aus Fremddaten.
- `typ=larp.setup-key.v1`, `cty=application/pkcs8`, `p2c=600000` exakt.
  Dieser feste Wert ist Teil der bestätigten Formatwahl. Seine Funktion wurde
  auf dem genannten Androidgerät ausgeführt; ein Produktlatenzbudget ist damit
  nicht qualifiziert. Andere Werte sind kein stiller Kompatibilitätsfallback.
- Frisches zufälliges p2s16, eigener zufälliger CEK32 und eigener zufälliger IV12
  bei jedem Export. Eingewickelter CEK40 und GCM-Tag16; PKCS8-Ciphertext genau 67 Byte.
  Die festen Werte des Kompatibilitätstests dürfen nie Produktentropie ersetzen.
- Kennwort mindestens 16 Unicodecodepoints, höchstens 128 UTF-8-Bytes; ungültige
  UTF-16-Surrogate werden abgelehnt. Diese Grenze beweist keine Kennwortentropie;
  eine lange Passphrase ist der vorgesehene Bedienhinweis, keine Wiederherstellungs-
  oder serverseitige Kennwortfunktion.
- `box` ist die nichtnull bstr16-Boxkennung in Base64url; keine Credentialbehauptung.
  `jwk` hat exakt `kty=EC,crv=P-256,x,y`; x/y je genau 32 Byte und gültiger P-256-Punkt.
- `kid` ist Base64url des SHA-256 des gleichen kanonischen öffentlichen COSE_Key-
  Formats wie D0. Diese Fingerprintdarstellung teilt **keine** Schlüsselrolle.
  Erwartete Box, Headerpublickey, kid und entschlüsselter privater Schlüssel
  müssen zusammenpassen; eine frische Signaturprüfung bestätigt das Schlüsselpaar.

Der PKCS8-Klartext wird für dieses Profil aus dem gültigen 32-Byte-Skalar in
eine exakte minimale DER-Form normalisiert: PrivateKeyInfo-Version0, Algorithmus
id-ecPublicKey, genau namedCurve secp256r1, enthaltenes ECPrivateKey-Version1
mit genau 32 Skalarbytes, keine Attribute, Zusatzobjekte, optionalen Parameter-
oder PublicKeyfelder. Ergebnis exakt 67 Byte. Import prüft die gesamte feste
DER-Präfixdarstellung und Skalargrenzen vor dem Provideraufruf; ein bloßes
`KeyFactory("EC")` oder Provider-Roundtrip ersetzt diesen Vollverbrauch nicht.
Damit sind alle gültigen P-256-Schlüssel darstellbar, ohne einen allgemeinen
ASN.1-/Privatschlüsselimportkanal zu eröffnen.

Die Boxkennung im Header wird erst durch korrekten GCM-Tag geschützt. Ihre
Übereinstimmung mit der ausgewählten Kennung ersetzt weder unabhängigen
Boxnachweis noch das erste vertrauenswürdige Einrichten des öffentlichen Ankers.
Eine selbst erzeugte Datei verleiht allein überhaupt keine Boxrechte.

## Lebensdauer und begrenzte API

Der Export erzeugt den Einrichtungsschlüssel außerhalb der Box. Sein Ergebnis
enthält ausschließlich verschlüsselte Datei und öffentliche Metadaten; kein
Signierer aus der Erzeugung wird an die Oberfläche weitergereicht. Mutable
temporäre Passwort-/PKCS8-/KDF-/CEK-Arrays werden soweit möglich geschlossen und
überschrieben. Daraus folgt keine zugesagte forensische Löschung im verwalteten
Runtimeheap oder beim Provider.

Für den tatsächlichen Nachweis wird die gespeicherte Datei erneut über den
nativen Dateidialog eingelesen, neu entschlüsselt und ein neuer Schlüsselbesitzer
erzeugt. Eine neue Challenge muss tatsächlich von **diesem** importierten Besitzer
beantwortet werden. Ein Klick auf „gespeichert“, weiterlebender Erzeugungssignierer
oder bloßer App-Speicherwert genügen nicht. Die konkrete Boxchallenge und die
physische Erstbereitstellung bleiben ein eigener qualifizierter Anschluss.

Der Dateikern gewährt keine Einrichtung, Veranstaltung, GM-Rolle oder D0-Origin-
Befugnis. Ein Öffnen erzeugt keinen Live-Administrationsauftrag. D0-Ursprungsschlüssel
verbleibt auf der Box, Einrichtungsschlüssel und Handyzugänge bleiben verschieden.
Schlüsselverlust folgt unverändert dem beschlossenen Zugangskonzept, ohne
zusätzlichen Papier-/Rescuekey oder datenerhaltende Umgehung.

## Nachweise und noch offene Pflichtprüfungen vor Integration

Der verlinkte Prüfbericht ordnet die tatsächlich ausgeführten unabhängigen
JWE-/UTF-8-/KDF-/AES-KW-/GCM-/PKCS8-Vektoren, Paar-/Box-/Kid-Gegenfälle,
661 kürzeren Dateipräfixe, Grenzen vor KDF, Inputkopien und Wiederverwendung
den konkreten Quellenständen zu. Auf API34 wurden zusätzlich beschädigte
KW-/IV-/Ciphertext-/Tag- und Original-AAD-Fälle wirklich abgelehnt. Gemeldete
Provider sind separate Factorysamples und keine Attestation der tatsächlichen
privaten Kandidateninstanzen.

Offen bleiben insbesondere die tatsächliche native Dokumentauswahl mit
Schließen und Wiederimport, Coroutine-/UI-Abbruch einschließlich verspätetem
Arbeitsergebnis, frische Boxchallenge durch den erneut importierten Besitzer,
physische Erstbereitstellung und die qualifizierte Einbindung in Setupaufträge.
Ein Thread-Interrupt-Test ersetzt keinen Coroutine-Lifecycletest. Die Probe
hat die öffentliche Referenz nur über ihren eigenen Appcache wiedergeöffnet.
Host-, isolierte Android-, Sicht-, Produktgeräte- und Ersteinrichtungsnachweise
bleiben getrennt. Es gibt keine forensische Heaplöschungszusage.

Root implementiert den Dateikern, unabhängige Security-/UI-Vertragsgegenprüfung
bleibt erforderlich. Die Formatwahl allein ist keine Mergefreigabe oder
Einrichtung einer neuen produktiven Vertrauensgrenze; die bestehenden übrigen
Bedingungen der Mergepolicy gelten weiter. Die separat autorisierte isolierte
Android-Prüfapp wurde nach beiden Läufen entfernt und ihre Abwesenheit geprüft.
Diese Ausführung genehmigt keine Produktgeräteaktion, Schlüsselprovisionierung,
Veröffentlichung oder Auswahl einer unbekannten physischen Enrollmentmethode.
