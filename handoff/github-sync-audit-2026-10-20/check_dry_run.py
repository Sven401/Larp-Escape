"""Read-only replay for the 2026-10-20 GitHub sync audit.

Input: local JSON from read-only GitHub reads; output: report on stdout.
No network client, credentials, API mutation or file-writing function.
This checks proposed text operations, not a future model's compliance.
"""
import hashlib
import json
import re
import sys

START = "<!-- codex-project-sync:v2:start -->"
END = "<!-- codex-project-sync:v2:end -->"

def sha(value):
    return hashlib.sha256(value.encode("utf-8")).hexdigest()

def locate(body):
    starts = list(re.finditer(re.escape(START), body))
    ends = list(re.finditer(re.escape(END), body))
    if not starts and not ends:
        return None
    if len(starts) != 1 or len(ends) != 1 or starts[0].start() >= ends[0].start():
        raise ValueError("ambiguous-markers")
    a, b = starts[0].start(), ends[0].end()
    for pos, marker in [(a, START), (ends[0].start(), END)]:
        line_start = body.rfind("\n", 0, pos) + 1
        line_end = body.find("\n", pos)
        if line_end == -1:
            line_end = len(body)
        if body[line_start:line_end].strip("\r") != marker:
            raise ValueError("non-standalone-marker")
        prefix = body[:pos]
        if len(re.findall(r"(?mi)^\s*(?:```|~~~)", prefix)) % 2:
            raise ValueError("marker-in-code")
        if len(re.findall(r"(?i)<details(?:\s[^>]*)?>", prefix)) != len(re.findall(r"(?i)</details>", prefix)):
            raise ValueError("marker-in-history")
    return a, b

def candidate(before, fresh, content, seen=False, conflict=False, source_ok=True):
    if not source_ok:
        raise ValueError("unverified-source")
    if conflict:
        raise ValueError("scope-conflict")
    if before["number"] <= 23:
        raise ValueError("historical-issue")
    for key in ("number", "body", "updated_at", "state"):
        if before[key] != fresh[key]:
            raise ValueError("concurrent-change")
    body = before["body"]
    span = locate(body)
    block = START + "\n## Automatischer Fortschrittsabgleich\n\n" + content + "\n" + END
    if span is None:
        if seen:
            raise ValueError("removed-owned-section")
        result = block + "\n\n" + body
        assert result.endswith(body)
    else:
        a, b = span
        result = body[:a] + block + body[b:]
        assert result[:a] == body[:a]
        assert result[a+len(block):] == body[b:]
    return result

def checks():
    base = {"number": 25, "body": "Manual plan\r\n\r\n<details>History stays</details>\r\n", "state": "open", "updated_at": "fixed"}
    total = 0
    proposed = candidate(base, base, "Fact.")
    assert proposed.endswith(base["body"])
    total += 1
    updated = dict(base, body=proposed)
    assert candidate(updated, updated, "Fact.") == proposed
    total += 1
    changed = candidate(updated, updated, "New fact.")
    assert changed.endswith(base["body"])
    total += 1
    cases = [
        (base, dict(base, body="Another session"), {}, "concurrent-change"),
        (base, dict(base, state="closed"), {}, "concurrent-change"),
        (base, dict(base, updated_at="later"), {}, "concurrent-change"),
        (base, base, {"source_ok":False}, "unverified-source"),
        (base, base, {"conflict":True}, "scope-conflict"),
        (base, base, {"seen":True}, "removed-owned-section"),
    ]
    for body, error in [
        (START+"\nMissing end", "ambiguous-markers"),
        (END+"\n"+START, "ambiguous-markers"),
        (proposed+"\n"+proposed, "ambiguous-markers"),
        ("<details>\n"+proposed+"\n</details>", "marker-in-history"),
        ("```md\n"+proposed+"\n```", "marker-in-code"),
        ("Text "+START+"\n"+END, "non-standalone-marker"),
    ]:
        altered = dict(base, body=body)
        cases.append((altered, altered, {}, error))
    historical = dict(base, number=1)
    cases.append((historical, historical, {}, "historical-issue"))
    for before, fresh, options, error in cases:
        try:
            candidate(before, fresh, "Fact.", **options)
        except ValueError as exc:
            assert str(exc) == error, (str(exc), error)
        else:
            raise AssertionError(error)
        total += 1
    return total

def run(before, fresh):
    source = before["source_commit"]
    assert source == fresh["source_commit"], "source-changed"
    assert before["board"]["60"] == fresh["board"]["60"], "board-changed"
    index = {i["number"]:i for i in before["issues"]}
    current = {i["number"]:i for i in fresh["issues"]}
    assert set(index) == set(current), "issue-inventory-changed"
    for number, issue in index.items():
        assert all(issue[k] == current[number][k] for k in ("body","updated_at","state")), ("live-change",number)
    root = "https://github.com/Sven401/Larp-Escape/blob/" + source + "/"
    station = "[" + "Stationsbeleg, 19.10.2026" + "](" + root + "handoff/STATIONSPILOT-INTEGRATION-2026-10-19.md)"
    maintenance = "[Wartungsbericht, 20.10.2026](" + root + "handoff/WARTUNG-PRUEFSTAND-2026-10-20.md)"
    texts = {
        25: "Belegstand 20.10.2026. PR #79 ist als lesender nativer Stationspilot integriert; #85, #86 und #87 sind ebenfalls integriert. Larp Dev und Larp GM bleiben getrennt. "+station+"; "+maintenance+".\n\nLicht-/Spielaktionen, allgemeiner Spielzugang, TalkBack, mehrere reale Stationen, Funkverlust bei eingeschaltetem Bluetooth und Dauerbetrieb bleiben offen. Die frischen Wartungstests sind Softwarebelege auf Produktbasis 7e19c95, keine neue Geräteabnahme. PR #73 ist weiterhin offen und nicht integriert; D1–D4 bleiben offen. Der ältere Rückkehrsatz in #34 bedarf eines Abgleichs mit dem GM-Piloten; keine automatische Änderung seiner Kriterien.",
        33: "Belegstand 19.10.2026: Der integrierte GM-Stationspilot liest nach gegenseitiger V2-Anmeldung bestätigten Status und entwertet veraltete Beobachtungen. "+station+".\n\nBegrenzt am lokalen Paar geprüft; echter Funkverlust bei Bluetooth an, mehrere reale Stationen und Dauerbetrieb bleiben offen. Kein Abschluss des gesamten Issues und kein neuer Gerätetest durch die Synchronisierung.",
        37: "Belegstand 19.10.2026: Larp GM besitzt lokale Stationsvorbereitung, gezielte Anmeldung und einen lesenden Stationspiloten; über PR #79 integriert. "+station+".\n\nLicht-/Spielaktionen und allgemeine Spielleitungsrechte bleiben Folgearbeit. Der Baugruppen-HMAC-Zugang ersetzt #27/#28 nicht; Issue bleibt offen.",
    }
    proposed = []
    for number, content in texts.items():
        original = index[number]
        result = candidate(original, current[number], content)
        proposed.append({"issue":number,"action":"prepend_owned_section" if locate(original["body"]) is None else "replace_owned_section","before_sha256":sha(original["body"]),"proposed_sha256":sha(result),"outside_preserved":result.endswith(original["body"]),"state_before":original["state"],"state_after":original["state"],"section":content})
    try:
        candidate(index[34], current[34], "Not allowed.", conflict=True)
    except ValueError as exc:
        assert str(exc) == "scope-conflict"
    else:
        raise AssertionError("conflicting issue must remain unchanged")
    return {"mode":"read-only","source_commit":source,"guard_cases_passed":checks(),"live_issues_compared":len(index),"github_mutations":0,"issue_state_changes":0,"proposals":proposed,"blocked":[{"issue":34,"reason":"Older return criterion conflicts with the integrated GM pilot; preserve the entire issue."}],"board_proposal":{"issue":60,"before":before["board"]["60"],"proposed":"In progress","reason":"D1-A is implemented and in open review #73; not merged or complete.","applied":False},"unchanged_issues":[n for n in sorted(index) if n not in texts]}

if __name__ == "__main__":
    if sys.argv[1:] == ["--self-test"]:
        print(json.dumps({"guard_cases_passed":checks()}))
    else:
        before = json.loads(open(sys.argv[1], encoding="utf-8-sig").read())
        fresh = json.loads(open(sys.argv[2], encoding="utf-8-sig").read())
        print(json.dumps(run(before, fresh), ensure_ascii=True, indent=2))
