"""Read recorded JSON traces only; no camera access, rendering, or image reads."""
from pathlib import Path
import argparse
import hashlib
import json
import statistics

sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()

def only_seal(state):
    return not any(state["background"]) and all(s["state"] in (0, 3) for s in state["slots"])

def flat(pixels):
    return [v for pixel in pixels for v in pixel]

def target(seal, off=False):
    count = 0 if off or not seal["valid"] else seal["count"]
    limit = (0, 8, 15, 23)[count]
    return [seal["rgb"] if i < limit else [0, 0, 0] for i in range(23)]

def review(root, name):
    run = root / name
    capture_path, camera_path = run / "capture.json", run / "camera.json"
    data = json.loads(capture_path.read_text())
    camera = json.loads(camera_path.read_text())
    assert sha(camera_path) == data["camera_manifest_sha256"]
    events, commands = data["events"], data["commands"]
    checks = {}
    checks["recorded"] = data["status"] == "recorded" and camera["status"] == "recorded"
    checks["consecutive_sequences"] = [e["sequence"] for e in events] == list(range(len(events)))
    checks["all_23_rgb_and_bounded"] = all(len(e["pixels"]) == 23 and all(len(p) == 3 and all(type(v) is int and 0 <= v <= 32 for v in p) for p in e["pixels"]) for e in events)
    checks["monotonic_submit"] = all(a["submitted_ms"] < b["submitted_ms"] for a, b in zip(events, events[1:]))
    checks["monotonic_revision"] = all(a["revision"] <= b["revision"] for a, b in zip(events, events[1:]))
    checks["all_commands_applied"] = all(c["result"] == "APPLIED" for c in commands)
    checks["complete_paired_ack"] = all(c["active_state"]["revision"] == c["active_state"]["seal"]["revision"] == c["revision"] for c in commands)
    checks["fresh_final_black"] = bool(data.get("final_black_confirmed")) and not any(flat(events[-1]["pixels"])) and events[-1]["revision"] > commands[-1]["revision"]
    groups = []
    pulse_intervals = []
    progression = []
    for command_index, command in enumerate(commands):
        state, item = command["active_state"], command["input"]
        seal = state["seal"]
        previous = commands[command_index - 1]["active_state"]["seal"] if command_index else None
        group = [e for e in events if e["revision"] == command["revision"]]
        origin = command["applied_device_ms"]
        row = {"req": command["req"], "revision": command["revision"], "label": item.get("label"), "op": item["op"],
               "count": seal["count"], "valid": seal["valid"], "consumed": seal["consumed"], "pending_at_ack": seal["pending"],
               "frames": len(group), "applied_device_ms": origin, "isolated_seal": only_seal(state), "checks": {}}
        row["checks"]["has_transfer"] = bool(group)
        if group:
            row["first_submit_age_ms"] = group[0]["submitted_ms"] - origin
            row["checks"]["transfer_after_application"] = row["first_submit_age_ms"] >= 0
        # Settled endpoint only: this trial uses full-level NearGlow contributions.
        # No camera codes are compared with firmware RGB codes.
        if all(slot["p"] in (0, 1000) and slot["gain"] == 1000 for slot in state["slots"]):
            horizon = max([seal["transition_ms"], seal["sweep_ms"] + (seal["pulse_ms"] if seal["pending"] else 0)] + [slot["duration_ms"] for slot in state["slots"]])
            expected = target(seal, seal["state"] in (0, 2, 3))
            glow = [sum(slot["rgb"][ch] for slot in state["slots"] if slot["state"] == 1 and slot["valid"] and slot["p"] == 1000) for ch in range(3)]
            expected = [[min(32, pixel[ch] + state["background"][ch] + glow[ch]) for ch in range(3)] for pixel in expected]
            settled_mix = [e for e in group if e["submitted_ms"] - origin >= horizon]
            if settled_mix:
                row["checks"]["settled_mixture_matches_contributions"] = all(e["pixels"] == expected for e in settled_mix)
                row["settled_mixture_pixel0"] = expected[0]
                row["settled_mixture_all_pixels_uniform"] = all(pixel == expected[0] for pixel in expected)
            if item["op"] in ("seal-stop", "stop"):
                stop_ms = seal["transition_ms"] if item["op"] == "seal-stop" else state["slots"][item["slot"]]["duration_ms"]
                after_stop = [e for e in group if e["submitted_ms"] - origin >= stop_ms]
                row["checks"]["mixed_stop_target_after_declared_duration"] = bool(after_stop) and all(e["pixels"] == expected for e in after_stop)
                distance = [[abs(v - goal) for v, goal in zip(flat(e["pixels"]), flat(expected))] for e in group]
                row["checks"]["mixed_stop_monotonic_to_target"] = all(all(y <= x for x, y in zip(a, b)) for a, b in zip(distance, distance[1:]))
                row["mixed_stop_duration_ms"] = stop_ms
                matches = [e["submitted_ms"] - origin for e in group if e["pixels"] == expected]
                row["first_sampled_mixed_stop_target_age_ms"] = min(matches) if matches else None
        if only_seal(state) and item["op"].startswith("seal-"):
            off = item["op"] == "seal-stop" or not seal["valid"]
            regular_progress = (previous and previous["valid"] and seal["valid"] and seal["cause"] == 1
                                and seal["count"] == previous["count"] + 1 and seal["round"] == previous["round"]
                                and all(seal[k] == previous[k] for k in ("rgb", "peak_rgb", "sweep_ms", "pixel_ms", "transition_ms", "pulse_ms")))
            horizon = seal["transition_ms"] if off or item["op"] == "seal-start" or not regular_progress else seal["sweep_ms"]
            if seal["pending"]:
                horizon = max(horizon, seal["sweep_ms"] + seal["pulse_ms"])
            settled = [e for e in group if e["submitted_ms"] - origin >= horizon]
            row["settled_horizon_ms"] = horizon
            row["settled_frames"] = len(settled)
            if settled:
                row["checks"]["all_settled_frames_equal_snapshot"] = all(e["pixels"] == target(seal, off) for e in settled)
                row["settled_lit_pixels"] = sum(any(p) for p in settled[0]["pixels"])
            if not regular_progress and not seal["pending"]:
                matches = [e["submitted_ms"] - origin for e in group if e["pixels"] == target(seal, off)]
                row["first_sampled_snapshot_target_age_ms"] = min(matches) if matches else None
            if off and group:
                row["checks"]["fade_monotonic"] = all(all(y <= x for x, y in zip(flat(a["pixels"]), flat(b["pixels"]))) for a, b in zip(group, group[1:]))
                zero = [e["submitted_ms"] - origin for e in group if not any(flat(e["pixels"]))]
                row["first_sampled_black_age_ms"] = min(zero) if zero else None
                row["black_age_note"] = "RGB8 quantization may reach zero before the internal 300 ms fade completes"
            elevated = [e for e in group if not off and any(any(pixel[ch] > seal["rgb"][ch] for ch in range(3)) for pixel in e["pixels"])]
            row["elevated_frames"] = len(elevated)
            if elevated:
                row["checks"]["elevation_uniform_all_23"] = all(all(p == e["pixels"][0] for p in e["pixels"]) for e in elevated)
                row["checks"]["elevation_only_when_pending_at_ack"] = bool(seal["pending"])
                row["checks"]["elevation_within_peak_bounds"] = all(all(v <= seal["peak_rgb"][ch] for p in e["pixels"] for ch, v in enumerate(p)) for e in elevated)
                row["checks"]["one_contiguous_elevation"] = [e["sequence"] for e in elevated] == list(range(elevated[0]["sequence"], elevated[-1]["sequence"] + 1))
                row["elevation_submit_ages_ms"] = [elevated[0]["submitted_ms"] - origin, elevated[-1]["submitted_ms"] - origin]
                row["observed_peak_rgb"] = [max(p[ch] for e in elevated for p in e["pixels"]) for ch in range(3)]
                pulse_intervals.append({"req": command["req"], "round": seal["round"], "revision": command["revision"], "submit_ages_ms": row["elevation_submit_ages_ms"]})
        # Measure when each new LED first reaches its requested resting RGB.
        # Follow later +1 revisions in the same round so rapid arrivals cannot
        # hide a restarted earlier sector timer. Stop/new round ends observation.
        if (only_seal(state) and item["op"] == "seal-update" and seal["valid"] and seal["cause"] == 1
                and previous and previous["valid"] and seal["round"] == previous["round"]
                and seal["count"] == previous["count"] + 1 and all(seal[k] == previous[k] for k in ("rgb", "peak_rgb", "sweep_ms", "pixel_ms", "transition_ms", "pulse_ms"))):
            boundary = events[-1]["submitted_ms"] + 1
            for later in commands[command_index + 1:]:
                later_seal = later["active_state"]["seal"]
                if later["input"]["op"] != "seal-update" or later_seal["round"] != seal["round"] or not later_seal["valid"] or later_seal["count"] < seal["count"]:
                    boundary = later["applied_device_ms"]
                    break
            observed = [e for e in events if origin <= e["submitted_ms"] < boundary]
            low, high = (0, 8, 15, 23)[seal["count"] - 1:seal["count"] + 1]
            samples = []
            for index in range(low, high):
                reached = next((e for e in observed if e["pixels"][index] == seal["rgb"]), None)
                expected_ms = ((seal["sweep_ms"] - seal["pixel_ms"]) * (index - low)) // (high - low - 1) + seal["pixel_ms"]
                samples.append({"index": index, "expected_full_ms": expected_ms,
                                "first_sampled_full_ms": reached["submitted_ms"] - origin if reached else None})
            local_gaps = [b["submitted_ms"] - a["submitted_ms"] for a, b in zip(observed, observed[1:])]
            tolerance = max(local_gaps or [33]) + 2
            completed = all(sample["first_sampled_full_ms"] is not None for sample in samples)
            measured = {"req": command["req"], "round": seal["round"], "count": seal["count"], "applied_device_ms": origin,
                        "pixels": samples, "sample_tolerance_ms": tolerance, "observation_end_age_ms": boundary - origin,
                        "interrupted_before_full_duration": boundary - origin < seal["sweep_ms"], "complete": completed}
            if completed:
                measured["full_time_checks_pass"] = all(sample["expected_full_ms"] - 2 <= sample["first_sampled_full_ms"] <= sample["expected_full_ms"] + tolerance for sample in samples)
                row["checks"]["independent_pixel_full_times"] = measured["full_time_checks_pass"]
            progression.append(measured)
        groups.append(row)
    pulse_rounds = [p["round"] for p in pulse_intervals]
    checks["at_most_one_elevated_interval_per_round_in_isolated_segments"] = len(pulse_rounds) == len(set(pulse_rounds))
    failures = [key for key, value in checks.items() if not value]
    failures.extend(f"req{row['req']}:{key}" for row in groups for key, value in row["checks"].items() if not value)
    gaps = [b["submitted_ms"] - a["submitted_ms"] for a, b in zip(events, events[1:])]
    result = {"scope": "Recorded digital output trace and metadata only; not optical acceptance or full independent formula simulation",
              "source_capture_sha256": sha(capture_path), "camera_manifest_sha256": sha(camera_path),
              "generator_sha256": sha(Path(__file__)), "firmware_sha256": data["firmware_sha256"],
              "run": name, "events": len(events), "commands": len(commands), "checks": checks, "failures": failures,
              "observed_maximum_rgb": [max(p[ch] for e in events for p in e["pixels"]) for ch in range(3)],
              "digital_submit_gaps_ms": {"median": statistics.median(gaps), "max": max(gaps)},
              "transfer_duration_max_ms": max(e["completed_ms"] - e["submitted_ms"] for e in events),
              "command_groups": groups, "pulse_intervals": pulse_intervals, "section_progression": progression,
              "camera": {"frames": len(camera["frames"]), "timing": camera["timing"], "dropped_for_writer_backlog": camera["dropped_for_writer_backlog"],
                         "meets_existing_80ms_gap_bound": camera["timing"]["max_gap_ms"] <= 80}}
    return result

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--raw", required=True, type=Path, help="Directory holding the named raw run directories")
parser.add_argument("--output", required=True, type=Path)
parser.add_argument("--runs", nargs="+", default=["v1b", "v2", "v3"])
parser.add_argument("--aborted", nargs="*", default=["v1"])
args = parser.parse_args()
results = [review(args.raw, name) for name in args.runs]
aborted = []
for name in args.aborted:
    capture_path = args.raw / name / "capture.json"
    data = json.loads(capture_path.read_text())
    aborted.append({"run": name, "status": data["status"], "error_category": "watchdog" if "watchdog" in data.get("error", "") else "other",
                    "source_capture_sha256": sha(capture_path), "commands_applied": sum(c.get("result") == "APPLIED" for c in data["commands"]),
                    "unresolved_requests": [c["req"] for c in data["commands"] if c.get("result") == "UNRESOLVED"],
                    "final_black_confirmed": bool(data.get("final_black_confirmed")), "not_used_for_effect_acceptance": True})
result = {"schema": 1, "scope": "Digital trace checks only. Camera manifest hash checked, image bytes not read. Firmware, camera and optical judgments remain separate.",
          "generator_sha256": sha(Path(__file__)), "runs": results, "aborted_runs": aborted}
with args.output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
print(json.dumps({"output": str(args.output), "runs": [{"run": r["run"], "events": r["events"], "commands": r["commands"], "failures": r["failures"],
                  "maximum_rgb": r["observed_maximum_rgb"], "camera": r["camera"], "pulses": r["pulse_intervals"]} for r in results], "aborted": aborted}))
