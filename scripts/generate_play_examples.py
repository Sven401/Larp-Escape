#!/usr/bin/env python3
"""Regenerate editable game-authoring examples from the exported standard profile.

Run after rebuilding profile-tools and exporting standard-profile.json. This is
an example generator; it performs no assertions and does not execute firmware.
"""
from pathlib import Path
import copy
import json

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "docs" / "configuration"
standard = json.loads((OUTPUT / "standard-profile.json").read_text(encoding="utf-8"))


def play_script(roles, fourth=False, bonus=True):
    lines = ["# Synthetic devices; Session computes conditions/progress from these inputs.",
             "start 0", "wakes 4 15", "tick 0", "tick 1"]
    selected = 1001
    for index, role in enumerate(roles):
        lines += ["", f"# Round {index + 1}: profile role ID {role['id']}",
                  "crystals 4", f"card 1 {role['id']}", f"tick {selected}"]
        entered = selected + 1
        lines += [f"tick {entered}", "crystals 4 " + " ".join(
            f"{item['row']}:{item['column']}" for item in role["crystals"])]
        # Three new crystals arrive while the first requested track is busy.
        lines += [f"tick {entered + 100}", f"tick {entered + 101}",
                  f"tick {entered + 201}", f"tick {entered + 301}", "finish 5",
                  f"tick {entered + 302}", f"tick {entered + 303}", "finish 5",
                  f"tick {entered + 304}", f"tick {entered + 305}", "finish 5"]
        if fourth:
            # Extended third state still reads the matrix. Fourth has its own
            # playback action and a 750ms completion wait, supplied by JSON only.
            lines += [f"tick {entered + 405}", f"tick {entered + 406}", "finish 5"]
            completed = entered + 1156
        else:
            completed = entered + 805
        lines += [f"tick {completed}", f"tick {completed + 1}", "finish 5",
                  f"tick {completed + 501}"]
        returned = completed + 502
        if bonus and index + 1 == len(roles):
            lines += [f"tick {returned}", "finish 5", f"tick {returned + 500}"]
            returned += 501
        lines += [f"tick {returned}", "snapshot"]
        if index + 1 < len(roles):
            # Existing policy needs two groups of five fresh empty card reads.
            lines += ["card 1 0"]
            lines += [f"tick {returned + 1000 * sample}" for sample in range(1, 11)]
            selected = returned + 11000
    return "\n".join(lines) + "\n"


(OUTPUT / "standard-play.txt").write_text(play_script(standard["roles"]), encoding="utf-8")
extended = copy.deepcopy(standard)
extended["revision"] = 5
extended["name"] = "Seven roles with fourth crystal"
role = copy.deepcopy(extended["roles"][0])
role.update(id=77, name="Companion custom role", uid=[77, 2, 3, 4, 5, 6, 7], audioTrack=177)
fourth = copy.deepcopy(role["crystals"][2])
fourth.update(name="Fourth orange crystal", row=9, column=8, audioTrack=178,
              palette=[0xFFAA00] * 16)
role["crystals"].append(fourth)
extended["roles"].append(role)
for item in extended["conditions"]:
    if item["kind"] == "predicate" and item["field"] == "seen_role_count":
        item["operand"] = len(extended["roles"])
extended["conditions"] += [
    {"id": 220, "kind": "wait", "operand": 750},
    {"id": 221, "kind": "all", "first": 220, "second": 1},
    {"id": 222, "kind": "all", "first": 221, "second": 15},
]
action = copy.deepcopy(next(item for item in extended["actions"] if item["id"] == 27))
action.update(id=220, value=3)
extended["actions"].append(action)
state = next(item for item in extended["states"] if item["id"] == 6)
state["tick"] += [7, 8]
extended["states"].append({"id": 88, "name": "fourth_crystal_placed", "manualAllowed": True,
                           "entry": [10, 26, 220], "tick": [28], "exit": []})
# Existing three-crystal roles retain their completion edge. The extra edge wins
# only when another crystal's audio is pending after third playback finishes.
for item in extended["transitions"]:
    if item["source"] == 6 and item["priority"] == 1:
        item["priority"] = 2
extended["transitions"] += [
    {"id": 220, "source": 6, "target": 88, "condition": 9, "priority": 1,
     "manualAllowed": False, "actions": [4]},
    {"id": 221, "source": 88, "target": 7, "condition": 222, "priority": 0,
     "manualAllowed": False, "actions": []},
]
(OUTPUT / "extended-profile.json").write_text(
    json.dumps(extended, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
(OUTPUT / "extended-play.txt").write_text(play_script([role], fourth=True, bonus=False),
                                          encoding="utf-8")

# Each bounded settle advances owner FIFOs and Session ticks at the same wall
# time. It demonstrates queued replies without changing configured intervals.
for name in ("standard", "extended"):
    source = (OUTPUT / f"{name}-play.txt").read_text(encoding="utf-8")
    lines = ["# Queued host owners: four bounded micro-ticks per supplied time."]
    lines += [f"settle {line.split()[1]} 4" if line.startswith("tick ") else line
              for line in source.splitlines()]
    (OUTPUT / f"{name}-queued-play.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
