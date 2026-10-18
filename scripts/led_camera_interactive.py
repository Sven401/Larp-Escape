"""Short interactive camera trial: one serial owner, independent bounded camera process.

Append JSON objects, one complete line at a time, to --commands while this runs.
Start requires slot/gen/p/valid/rgb/gain/duration_ms/curve. Update/stop inherit only
ACK-confirmed parameters. op=background sets rgb; op=status reads state;
op=finish ends with STOP and a bounded wait for actual black + DONE.
No flashing, reconnecting, command retry, calibration or acceptance verdict.
"""
import argparse
from collections import deque
from datetime import datetime, timezone
import hashlib
import json
import multiprocessing as mp
import os
from pathlib import Path
import queue
import secrets
import statistics
import time

from led_camera import digest
from led_camera_lab import pack_frame


class CommandTail:
    """Append-only UTF-8 JSONL; an unfinished EOF line waits for the next append."""
    def __init__(self, path):
        self.path, self.offset, self.pending = Path(path), 0, b""

    def read(self):
        if not self.path.exists():
            return []
        if self.path.stat().st_size < self.offset:
            raise ValueError("Command file was truncated; append only")
        with self.path.open("rb") as stream:
            stream.seek(self.offset)
            chunk = stream.read(65536)
        self.offset += len(chunk)
        self.pending += chunk
        pieces = self.pending.split(b"\n")
        self.pending = pieces.pop()
        if len(self.pending) > 4096 or any(len(p) > 4096 for p in pieces):
            raise ValueError("Command line exceeds 4096 bytes")
        commands = []
        for piece in pieces:
            if not piece.strip():
                continue
            try:
                item = json.loads(piece.decode("utf-8-sig"))
                if not isinstance(item, dict):
                    raise ValueError("Command must be a JSON object")
                commands.append(item)
            except (UnicodeError, ValueError) as error:
                commands.append({"_invalid": str(error)})
        return commands


def integer(value, lo, hi, name):
    if type(value) is not int or not lo <= value <= hi:
        raise ValueError(f"{name} must be an integer in {lo}..{hi}")
    return value


def rgb(value):
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError("rgb must contain three integers")
    return [integer(c, 0, 32, "rgb channel") for c in value]


def encode_command(item, token, req, state):
    """Build a complete wire command from explicit or confirmed parameters."""
    if "_invalid" in item:
        raise ValueError(item["_invalid"])
    op = item.get("op")
    allowed = {"op", "label"}
    if op == "background":
        allowed |= {"rgb"}
        wire = f"BG {token} {req} " + " ".join(map(str, rgb(item.get("rgb"))))
    elif op in ("status", "finish"):
        wire = f"STATUS {token}" if op == "status" else "STOP"
    elif op in ("start", "update", "stop"):
        allowed |= {"slot", "gen", "p", "valid", "rgb", "gain", "duration_ms", "curve"}
        slot = integer(item.get("slot"), 0, 1, "slot")
        current = state["slots"][slot] if state else None
        params = dict(current) if op != "start" and current and current["state"] else {}
        params.update({k: v for k, v in item.items() if k in allowed})
        values = [integer(params.get("gen"), 1, 0xFFFFFFFF, "gen"), op,
                  integer(params.get("p"), 0, 1000, "p"),
                  integer(params.get("valid"), 0, 1, "valid")]
        values += rgb(params.get("rgb"))
        values += [integer(params.get("gain"), 0, 1000, "gain"),
                   integer(params.get("duration_ms"), 50, 5000, "duration_ms"),
                   integer(params.get("curve"), 0, 1, "curve")]
        if op == "stop" and current:
            for key in ("gen", "p", "valid", "rgb", "gain", "duration_ms", "curve"):
                if params.get(key) != current[key]:
                    raise ValueError("Stop must repeat ACK-confirmed parameters; update first")
        wire = f"GLOW {token} {req} {slot} " + " ".join(map(str, values))
    else:
        raise ValueError("op must be start/update/stop/background/status/finish")
    unknown = set(item) - allowed
    if unknown:
        raise ValueError("Unknown command fields: " + ", ".join(sorted(unknown)))
    return wire


def parse_state(line, token):
    fields = line.split()
    if len(fields) != 29 or fields[:2] != ["STATE", token]:
        raise ValueError("Invalid STATE envelope")
    nums = [int(v) for v in fields[2:]]
    revision, device_ms = nums[:2]
    bg = rgb(nums[2:5])
    slots = []
    for offset in (5, 16):
        s = nums[offset:offset+11]
        slots.append({"state": integer(s[0], 0, 3, "state"), "gen": s[1], "seq": s[2],
                      "p": s[3], "valid": s[4], "rgb": s[5:8], "gain": s[8],
                      "duration_ms": s[9], "curve": s[10]})
    return {"revision": revision, "device_ms": device_ms, "background": bg, "slots": slots}


def parse_live_frame(line, token):
    fields = line.split()
    if len(fields) != 7 or fields[:2] != ["L", token]:
        raise ValueError("Invalid live frame envelope")
    revision, number, submitted, completed = [integer(int(v), 0, 0xFFFFFFFF, "frame field") for v in fields[2:6]]
    pixels = bytes.fromhex(fields[6])
    if len(pixels) != 69 or max(pixels) > 32:
        raise ValueError("Invalid frame channels")
    if ((completed - submitted) & 0xFFFFFFFF) > 2000:
        raise ValueError("Invalid transfer interval")
    return {"revision": revision, "sequence": number, "submitted_ms": submitted,
            "completed_ms": completed, "pixels": [list(pixels[i:i+3]) for i in range(0,69,3)]}



def publish_preview(output, frame, entry, write_image):
    """Best-effort UI files. Locked previews never invalidate saved camera frames."""
    warnings = []
    try:
        image_temp = output / "latest.tmp.png"
        if not write_image(str(image_temp), frame):
            raise OSError("Preview encoding failed")
        os.replace(image_temp, output / "latest.png")
    except OSError as error:
        return [{"file": "latest.png", "error": str(error), "t_ms": entry["t_ms"]}]
    try:
        metadata_temp = output / "latest.tmp.json"
        metadata_temp.write_text(json.dumps({**entry, "preview_only": True}), encoding="utf-8")
        os.replace(metadata_temp, output / "latest.json")
    except OSError as error:
        warnings.append({"file": "latest.json", "error": str(error), "t_ms": entry["t_ms"]})
    return warnings


def unresolved_at_close(command):
    """Preserve partial ACK information and explicitly disposition an unfinished request."""
    if command is None:
        return False
    if command.get("result") is not None:
        command["ack_result"] = command["result"]
    command["result"] = "UNRESOLVED"
    command["reason"] = "Trial ended without complete ACK and matching STATE (or explicit REJECT)"
    return True


def camera_worker(config, origin, stop_event, messages):
    """Only this process owns the camera; serial owner can terminate a stuck read."""
    try:
        import cv2
    except ImportError as error:
        messages.put({"type": "error", "error": "Camera dependency unavailable: " + str(error)})
        return
    from concurrent.futures import ThreadPoolExecutor
    out = Path(config["output"])
    cap = None
    writer = ThreadPoolExecutor(max_workers=2)
    pending = deque()
    entries = []
    report = {"status": "incomplete", "frames": entries, "opencv": cv2.__version__,
              "dropped_for_writer_backlog": 0, "settings": {}, "read_gap_warning_ms": 100,
              "preview_warning_count": 0, "preview_warnings": []}
    layout = None
    def save(path, frame, entry):
        if not cv2.imwrite(str(path), frame, [cv2.IMWRITE_PNG_COMPRESSION, 1]):
            raise RuntimeError("Camera image write failed")
        entry["sha256"] = digest(path)
    try:
        if config.get("rois"):
            layout = json.loads(Path(config["rois"]).read_text())
            report["roi_layout"] = layout
            report["roi_layout_sha256"] = digest(Path(config["rois"]))
        cap = cv2.VideoCapture(config["camera"], cv2.CAP_DSHOW)
        if not cap.isOpened():
            raise RuntimeError("Camera unavailable")
        settings = [("width", cv2.CAP_PROP_FRAME_WIDTH, config["width"]),
                    ("height", cv2.CAP_PROP_FRAME_HEIGHT, config["height"]),
                    ("fps", cv2.CAP_PROP_FPS, 30),
                    ("auto_exposure", cv2.CAP_PROP_AUTO_EXPOSURE, 0),
                    ("exposure", cv2.CAP_PROP_EXPOSURE, config["exposure"]),
                    ("auto_wb", cv2.CAP_PROP_AUTO_WB, 0), ("gain", cv2.CAP_PROP_GAIN, 0),
                    ("backlight", cv2.CAP_PROP_BACKLIGHT, 0),
                    ("sharpness", cv2.CAP_PROP_SHARPNESS, 0),
                    ("gamma", cv2.CAP_PROP_GAMMA, 100),
                    ("fourcc", cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))]
        for name, prop, value in settings:
            report["settings"][name] = {"requested": value, "accepted": cap.set(prop, value)}
        for name, prop, _ in settings:
            report["settings"][name]["readback"] = cap.get(prop)
        for _ in range(20):
            if not cap.read()[0]:
                raise RuntimeError("Camera warmup failed")
        report["ready_host_ms"] = (time.perf_counter() - origin) * 1000
        messages.put({"type": "ready", "settings": report["settings"]})
        previous = None
        gaps = []
        last_preview = 0
        while not stop_event.is_set():
            before = time.perf_counter()
            ok, frame = cap.read()
            received = time.perf_counter()
            if not ok:
                raise RuntimeError("Camera read failed")
            if previous is not None:
                gaps.append((received - previous) * 1000)
            previous = received
            while pending and pending[0].done():
                pending.popleft().result()
            if len(pending) >= 20:
                report["dropped_for_writer_backlog"] += 1
                continue
            source_frame = frame
            if layout:
                frame = pack_frame(frame, layout)
            path = out / f"frame-{len(entries):06d}.png"
            entry = {"file": path.name, "t_ms": (received - origin) * 1000,
                     "read_ms": (received - before) * 1000,
                     "width": frame.shape[1], "height": frame.shape[0],
                     "mean_bgr": [float(v) for v in frame.mean(axis=(0,1))],
                     "saturated_fraction": float((frame >= 250).any(axis=2).mean())}
            entries.append(entry)
            pending.append(writer.submit(save, path, frame, entry))
            if received - last_preview >= .25:
                # Heartbeat describes acquired/scheduled frames, independent of UI locks.
                messages.put({"type": "frame", "t_ms": entry["t_ms"], "count": len(entries)})
                warnings = publish_preview(out, source_frame, entry, cv2.imwrite)
                report["preview_warning_count"] += len(warnings)
                report["preview_warnings"].extend(warnings[:max(0, 20-len(report["preview_warnings"]))])
                last_preview = received
        for future in pending:
            future.result()
        report["status"] = "recorded"
        report["timing"] = {"received_fps": 1000/statistics.mean(gaps) if gaps else None,
                            "max_gap_ms": max(gaps) if gaps else None,
                            "gaps_over_100ms": sum(gap > 100 for gap in gaps)}
    except BaseException as error:
        report["error"] = str(error)
        messages.put({"type": "error", "error": str(error)})
    finally:
        if cap is not None:
            cap.release()
        writer.shutdown(wait=True)
        (out / "camera.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        messages.put({"type": "closed", "status": report["status"]})


def run(args):
    import serial
    binary = args.firmware.read_bytes()
    args.output.mkdir(parents=True, exist_ok=False)
    origin = time.perf_counter()
    token = secrets.token_hex(8)
    report = {"schema": 1, "status": "incomplete", "token": token,
              "utc": datetime.now(timezone.utc).isoformat(), "port": args.port,
              "firmware_sha256": hashlib.sha256(binary).hexdigest(),
              "host_source_sha256": digest(Path(__file__)), "commands": [], "events": [],
              "states": [], "serial": [], "scope": "Interactive visual trial; not optical acceptance",
              "time_meaning": "Host perf_counter receive times; camera exposure and buffering unknown"}
    trace = (args.output / "session.jsonl").open("w", encoding="utf-8")
    def now_ms():
        return (time.perf_counter() - origin) * 1000
    def record(kind, data):
        trace.write(json.dumps({"kind": kind, "host_ms": now_ms(), **data}) + "\n")
        trace.flush()
    connection = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=.25)
    connection.dtr = connection.rts = False
    connection.port = args.port
    camera_stop, messages = mp.Event(), mp.Queue()
    camera = None
    pending_bytes = b""
    last_state = None
    live = False
    identity_confirmed = False
    pending = None
    def lines():
        nonlocal pending_bytes
        pending_bytes += connection.read(8192)
        if len(pending_bytes) > 65536:
            raise RuntimeError("Serial receive buffer overflow")
        pieces = pending_bytes.split(b"\n")
        pending_bytes = pieces.pop()
        result = []
        for piece in pieces:
            line = piece.decode("ascii", errors="strict").strip()
            report["serial"].append({"host_ms": now_ms(), "line": line})
            record("receive", {"line": line})
            result.append(line)
        return result
    def send(wire):
        when = now_ms()
        connection.write((wire + "\n").encode("ascii"))
        record("send", {"line": wire, "sent_ms": when})
        return when
    def receive_line(line):
        nonlocal last_state
        if line.startswith(f"L {token} "):
            event = parse_live_frame(line, token)
            event["host_ms"] = now_ms()
            if report["events"] and event["sequence"] != report["events"][-1]["sequence"] + 1:
                report.setdefault("trace_sequence_gaps", []).append(event["sequence"])
            report["events"].append(event)
        elif line.startswith(f"STATE {token} "):
            last_state = parse_state(line, token)
            last_state["host_ms"] = now_ms()
            report["states"].append(last_state)
            (args.output / "active.json").write_text(json.dumps(last_state, indent=2), encoding="utf-8")
    def acknowledge(line):
        nonlocal pending
        fields = line.split()
        if pending and len(fields) >= 3 and fields[1] == token and fields[2] == str(pending["req"]):
            if fields[0] in ("APPLIED", "DUPLICATE") and len(fields) == 5:
                pending.update(result=fields[0], revision=int(fields[3]),
                               applied_device_ms=int(fields[4]), ack_host_ms=now_ms())
            elif fields[0] == "REJECT" and len(fields) == 6:
                pending.update(result="REJECT", reason=fields[3], ack_host_ms=now_ms())
                print(json.dumps(pending), flush=True)
                pending = None
        if pending and pending.get("revision") is not None and last_state and last_state["revision"] == pending["revision"]:
            pending["active_state"] = last_state
            print(json.dumps(pending), flush=True)
            pending = None
    try:
        connection.open()
        identity = capability = False
        deadline, last_hello = time.perf_counter() + 5, 0
        while time.perf_counter() < deadline and not (identity and capability):
            if time.perf_counter() - last_hello > .3:
                send("HELLO")
                last_hello = time.perf_counter()
            for line in lines():
                fields = line.split()
                if len(fields) == 6 and fields[:4] == ["LARP-CAMERA/1", "23", "32", "GPIO15"]:
                    if int(fields[4]) != len(binary) or fields[5] != hashlib.md5(binary).hexdigest():
                        raise RuntimeError("Runtime image differs from --firmware; no LIVE started")
                    identity = True
                if fields == ["LARP-LIVE/1", "2", "23", "32"]:
                    capability = True
            time.sleep(.002)
        if not (identity and capability):
            raise RuntimeError("Missing exact firmware identity or LIVE capability")
        identity_confirmed = True
        send("STOP")
        deadline = time.perf_counter() + 3
        stopped = False
        while time.perf_counter() < deadline and not stopped:
            stopped = any(line.startswith("DONE ") for line in lines())
            time.sleep(.002)
        if not stopped:
            raise RuntimeError("Initial STOP did not confirm DONE")
        config = {k: getattr(args, k) for k in ("camera", "width", "height", "exposure")}
        config.update(output=str(args.output), rois=str(args.rois) if args.rois else None)
        camera = mp.Process(target=camera_worker, args=(config, origin, camera_stop, messages), daemon=True)
        camera.start()
        ready, deadline = False, time.perf_counter() + 12
        while time.perf_counter() < deadline and not ready:
            try:
                message = messages.get(timeout=.05)
                if message["type"] == "error":
                    raise RuntimeError(message["error"])
                ready = message["type"] == "ready"
            except queue.Empty:
                if not camera.is_alive():
                    raise RuntimeError("Camera process stopped before ready")
        if not ready:
            raise RuntimeError("Camera did not become ready within 12 seconds")
        send(f"LIVE {token}")
        deadline = time.perf_counter() + 2
        while time.perf_counter() < deadline and not live:
            for line in lines():
                receive_line(line)
                if line.startswith(f"LIVE {token} "):
                    live = True
            time.sleep(.002)
        if not live:
            raise RuntimeError("LIVE was not acknowledged")
        report["live_start_host_ms"] = now_ms()
        print(json.dumps({"status": "live", "output": str(args.output), "token": token}), flush=True)
        tail, requests, pending, req = CommandTail(args.commands), deque(), None, 0
        last_keep, end = 0, time.perf_counter() + args.seconds
        last_camera_frame = time.perf_counter()
        while time.perf_counter() < end:
            if time.perf_counter() - last_keep >= .2:
                send(f"KEEP {token}")
                last_keep = time.perf_counter()
            while True:
                try:
                    message = messages.get_nowait()
                except queue.Empty:
                    break
                if message["type"] == "error":
                    raise RuntimeError(message["error"])
                if message["type"] == "frame":
                    last_camera_frame = time.perf_counter()
            if time.perf_counter() - last_camera_frame > 2.5:
                raise RuntimeError("No camera frame received for 2.5 seconds")
            if not camera.is_alive():
                raise RuntimeError("Camera process stopped during trial")
            for line in lines():
                receive_line(line)
                acknowledge(line)
                if line.startswith(f"STOP {token} "):
                    raise RuntimeError("Firmware stopped trial: " + line)
            if pending and now_ms() - pending["sent_ms"] > 2000:
                raise RuntimeError("Command acknowledgement exceeded 2 seconds")
            requests.extend(tail.read())
            if len(requests) > 32:
                raise RuntimeError("Interactive command backlog exceeds 32")
            if not pending and requests:
                item = requests.popleft()
                try:
                    wire = encode_command(item, token, req + 1, last_state)
                except (ValueError, KeyError) as error:
                    rejected = {"input": item, "result": "HOST_REJECT", "error": str(error), "host_ms": now_ms()}
                    report["commands"].append(rejected)
                    record("command", rejected)
                    print(json.dumps(rejected), flush=True)
                    continue
                if item["op"] == "finish":
                    report["finish_requested"] = True
                    break
                if item["op"] == "status":
                    send(wire)
                else:
                    req += 1
                    pending = {"req": req, "input": item, "wire": wire, "sent_ms": send(wire)}
                    report["commands"].append(pending)
            time.sleep(.002)
        report["status"] = "trial_finished"
    except BaseException as error:
        report["error"] = str(error)
        raise
    finally:
        if connection.is_open and identity_confirmed:
            try:
                event_floor = len(report["events"])
                report["stop_sent_ms"] = send("STOP")
                done = False
                deadline = time.perf_counter() + 3
                while time.perf_counter() < deadline and not done:
                    for line in lines():
                        receive_line(line)
                        acknowledge(line)
                        if line == f"DONE {token}":
                            done = True
                    time.sleep(.002)
                final_events = report["events"][event_floor:]
                report["final_black_confirmed"] = bool(done and final_events and not any(any(p) for p in final_events[-1]["pixels"]))
                report["stop_done_host_ms"] = now_ms() if done else None
            except BaseException as error:
                report["stop_error"] = str(error)
            finally:
                connection.close()
        elif connection.is_open:
            connection.close()
        if report.get("final_black_confirmed") and camera and camera.is_alive():
            time.sleep(.5)  # Capture an optical tail; still no calibrated delay claim.
        if unresolved_at_close(pending):
            report["unresolved_commands"] = [pending["req"]]
        camera_stop.set()
        if camera:
            camera.join(3)
            if camera.is_alive():
                camera.terminate()
                camera.join(1)
                report["camera_terminated"] = True
        for command in report["commands"]:
            if command.get("ack_host_ms") is not None:
                command["host_to_ack_ms"] = command["ack_host_ms"] - command["sent_ms"]
            if command.get("revision") is not None:
                match = next((e for e in report["events"] if e["revision"] == command["revision"]), None)
                if match:
                    command["first_frame"] = match["sequence"]
                    command["applied_to_submitted_ms"] = (match["submitted_ms"] - command["applied_device_ms"]) & 0xFFFFFFFF
                    command["applied_to_completed_ms"] = (match["completed_ms"] - command["applied_device_ms"]) & 0xFFFFFFFF
                    command["host_to_tx_event_ms"] = match["host_ms"] - command["sent_ms"]
        camera_manifest = args.output / "camera.json"
        if camera_manifest.exists():
            camera_data = json.loads(camera_manifest.read_text(encoding="utf-8"))
            report["camera_status"] = camera_data["status"]
            report["camera_timing"] = camera_data.get("timing")
            report["camera_manifest_sha256"] = digest(camera_manifest)
        if report["status"] == "trial_finished" and report.get("final_black_confirmed") and report.get("camera_status") == "recorded" and not report.get("unresolved_commands"):
            report["status"] = "recorded"
        else:
            report["status"] = "incomplete"
        record("complete", {"status": report["status"], "final_black_confirmed": report.get("final_black_confirmed", False)})
        trace.close()
        (args.output / "capture.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({"status": report["status"], "commands": len(report["commands"]),
                      "led_frames": len(report["events"]), "final_black_confirmed": report.get("final_black_confirmed")}), flush=True)
    if report["status"] != "recorded":
        raise RuntimeError("Trial incomplete; inspect capture.json")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--commands", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--exposure", type=float, default=-7)
    parser.add_argument("--rois", type=Path)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 80:
        parser.error("Trial duration must be 1..80 seconds (firmware limit 90 seconds)")
    run(args)


if __name__ == "__main__":
    mp.freeze_support()
    main()
