"""Capture one isolated USB calibration/effect transaction alongside its actual LED frames."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import secrets
import statistics
import time
import shutil
import subprocess
from concurrent.futures import ThreadPoolExecutor

from led_camera import digest


def parse_frame(line, token):
    fields = line.split()
    if len(fields) != 7 or fields[0] != "F" or fields[1] != token:
        raise ValueError("Invalid frame envelope")
    stage, sequence, submitted, completed = map(int, fields[2:6])
    pixels = bytes.fromhex(fields[6])
    if len(pixels) != 69 or max(pixels) > 32 or min(stage, sequence, submitted, completed) < 0:
        raise ValueError("Invalid frame payload")
    if completed < submitted or completed-submitted > 2000:
        raise ValueError("Invalid transfer interval")
    return {"stage": stage, "sequence": sequence, "submitted_ms": submitted,
            "completed_ms": completed, "pixels": [list(pixels[i:i+3]) for i in range(0,69,3)]}


def pack_frame(frame,layout):
    import numpy as np
    if layout["source_image_size"] != [frame.shape[1],frame.shape[0]]:
        raise ValueError("Camera dimensions differ from acquisition layout")
    size,columns=layout["tile_size"],layout["columns"]
    if size not in (64,80) or columns!=5 or len(layout["regions"])!=23:
        raise ValueError("Unsupported ROI tile geometry")
    atlas=np.zeros((5*size,5*size,3),dtype=frame.dtype)
    for tile,region in enumerate(layout["regions"]):
        x,y=region["x"],region["y"]
        if not size//2<=x<frame.shape[1]-size//2 or not size//2<=y<frame.shape[0]-size//2:
            raise ValueError("Acquisition region at image boundary")
        left,top=(tile%columns)*size,(tile//columns)*size
        patch=frame[y-size//2:y+size//2,x-size//2:x+size//2].copy()
        radius=region.get("aperture_radius",size//2)
        if not 1<=radius<=size//2:raise ValueError("Invalid acquisition aperture")
        yy,xx=np.ogrid[-size//2:size//2,-size//2:size//2]
        patch[xx*xx+yy*yy>radius*radius]=0
        atlas[top:top+size,left:left+size]=patch
    return atlas


def run(args):
    import cv2
    import serial

    args.output.mkdir(parents=True, exist_ok=False)
    if args.image_format == "bmp" and shutil.disk_usage(args.output).free < 8*1024**3:
        raise RuntimeError("Full HD-frame evidence needs at least 8 GiB free space")
    binary = args.firmware.read_bytes()
    token = secrets.token_hex(8)
    metadata = {"schema": 1, "status": "incomplete", "mode": args.mode, "token": token,
                "utc": datetime.now(timezone.utc).isoformat(), "port": args.port,
                "firmware_sha256": hashlib.sha256(binary).hexdigest(),
                "camera_index": args.camera, "opencv": cv2.__version__, "frames": [],
                "image_format":args.image_format,
                "timestamp_meaning": "host receive times; optical sync delay calibrated separately"}
    expected_reason = "watchdog" if args.watchdog_after is not None else "host" if args.stop_after is not None else "finished"
    metadata["expected_stop_reason"] = expected_reason
    source = Path(__file__).resolve().parents[1]
    metadata["source_head"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    names = ("firmware/led/src/CameraLabMain.cpp", "firmware/led/src/LedOutput.cpp",
             "firmware/led/src/RmtLedBus.cpp", "firmware/led/include/CameraSequence.h",
             "firmware/led/include/ResonanceCameraSequence.h", "scripts/led_camera_lab.py")
    metadata["source_files_sha256"] = {name: digest(source/name) for name in names}
    layout=json.loads(args.rois.read_text()) if args.rois else None
    if layout:
        if layout.get("schema")!=1 or len(layout.get("regions",[]))!=23:
            raise ValueError("Invalid acquisition ROI layout")
        metadata["roi_layout"]=layout
        metadata["roi_layout_sha256"]=digest(args.rois)
    connection = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=1)
    connection.dtr = connection.rts = False
    connection.port = args.port
    cap = None
    events = []
    wake_events = []
    writer = ThreadPoolExecutor(max_workers=2)
    pending_images = []
    image_entries=[]
    def save_image(path, frame):
        params=[cv2.IMWRITE_PNG_COMPRESSION,1] if args.image_format=="png" else []
        if not cv2.imwrite(str(path), frame, params):
            raise RuntimeError("Image write failed")
        # Re-reading HD images for hashing competes with camera decode and disk
        # writes. Hash only after capture has stopped; no image is trusted earlier.
    try:
        connection.open()
        pending = b""
        def lines():
            nonlocal pending
            pending += connection.read(8192)
            if len(pending) > 32768:
                raise RuntimeError("Serial input overflow")
            parts = pending.split(b"\n")
            pending = parts.pop()
            return [part.decode("ascii", errors="strict").strip() for part in parts]

        until = time.perf_counter()+5
        identified = False
        while time.perf_counter() < until and not identified:
            connection.write(b"HELLO\n")
            time.sleep(.1)
            for line in lines():
                fields = line.split()
                if len(fields) == 6 and fields[:4] == ["LARP-CAMERA/1", "23", "32", "GPIO15"]:
                    length = int(fields[4])
                    if length != len(binary) or hashlib.md5(binary).hexdigest() != fields[5]:
                        raise RuntimeError("Running image does not match the specified firmware binary")
                    metadata["runtime_image_bytes"] = length
                    metadata["runtime_image_md5"] = fields[5]
                    identified = True
        if not identified:
            raise RuntimeError("No camera-lab firmware identity; no calibration started")
        connection.write(b"STOP\n")
        time.sleep(.2)
        lines()
        cap = cv2.VideoCapture(args.camera, cv2.CAP_DSHOW)
        if not cap.isOpened():
            raise RuntimeError("Camera unavailable")
        settings = [("width", cv2.CAP_PROP_FRAME_WIDTH, args.width),
                    ("height", cv2.CAP_PROP_FRAME_HEIGHT, args.height),
                    ("fps", cv2.CAP_PROP_FPS, 30),
                    ("auto_exposure", cv2.CAP_PROP_AUTO_EXPOSURE, 0),
                    ("exposure", cv2.CAP_PROP_EXPOSURE, args.exposure),
                    ("auto_wb", cv2.CAP_PROP_AUTO_WB, 0),
                    ("gain", cv2.CAP_PROP_GAIN, 0),
                    ("backlight", cv2.CAP_PROP_BACKLIGHT, 0),
                    ("sharpness", cv2.CAP_PROP_SHARPNESS, 0),
                    ("gamma", cv2.CAP_PROP_GAMMA, 100)]
        # DShow's FPS setter recreates the stream and drops the prior FourCC.
        # Set the compressed camera format last, then read every property back.
        settings.append(("fourcc",cv2.CAP_PROP_FOURCC,cv2.VideoWriter_fourcc(*"MJPG")))
        metadata["settings"] = {}
        for name, prop, value in settings:
            accepted = cap.set(prop, value)
            metadata["settings"][name] = {"requested": value, "driver_accepted": accepted,
                                           "readback": cap.get(prop)}
        for name,prop,_ in settings:
            metadata["settings"][name]["readback"] = cap.get(prop)
        for _ in range(20):
            if not cap.read()[0]: raise RuntimeError("Camera warmup failed")
        start = last_keep = time.perf_counter()
        camera_number=0
        metadata["contexts"]=[]
        sent = False
        host_stop_sent = False
        finished_at = None
        while time.perf_counter()-start < 75:
            now = time.perf_counter()
            if now-last_keep > .2 and (args.watchdog_after is None or now-start < args.watchdog_after):
                connection.write(f"KEEP {token}\n".encode())
                last_keep = now
            if not sent and now-start > .5:
                effect="burst" if args.mode=="pilot" else args.mode
                command = f"CAL {token}" if args.mode == "calibrate" else f"RUN {token} {effect}"
                connection.write((command+"\n").encode())
                metadata["command_sent_ms"] = (time.perf_counter()-start)*1000
                sent = True
            if args.stop_after is not None and now-start >= args.stop_after and not host_stop_sent:
                connection.write(b"STOP\n")
                metadata["host_stop_sent_ms"] = (time.perf_counter()-start)*1000
                host_stop_sent = True
            before = time.perf_counter()
            ok, frame = cap.read()
            received = time.perf_counter()
            if not ok: raise RuntimeError("Camera read failed")
            for line in lines():
                when = (time.perf_counter()-start)*1000
                if line.startswith(f"F {token} "):
                    event = parse_frame(line, token)
                    if event["sequence"] != len(events):
                        raise RuntimeError("Lost or duplicated firmware frame event")
                    event["host_ms"] = when
                    events.append(event)
                elif line.startswith(f"W {token} "):
                    fields = line.split()
                    if len(fields) != 8:
                        raise RuntimeError("Invalid synthetic Wake event")
                    number, device, valid, mask, effect, restart = map(int, fields[2:])
                    if number != len(wake_events) or valid not in (0, 1) or restart not in (0, 1):
                        raise RuntimeError("Invalid synthetic Wake sequence")
                    wake_events.append(dict(sequence=number, device_ms=device, valid=bool(valid),
                                            mask=mask, effect=effect, restart=bool(restart), host_ms=when))
                elif line == f"STOP {token} {expected_reason}":
                    metadata["finished_normally"] = True
                    metadata["observed_stop_reason"] = expected_reason
                elif line.startswith(f"STOP {token} "):
                    raise RuntimeError("Firmware aborted the transaction")
                elif line == f"DONE {token}":
                    if not metadata.get("finished_normally"):
                        raise RuntimeError("Unexpected completion")
                    finished_at = time.perf_counter()
            camera_number+=1
            if args.mode=="pilot" and camera_number%6!=1: continue
            if layout:
                metadata["source_camera_size"]=[frame.shape[1],frame.shape[0]]
                atlas=pack_frame(frame,layout)
                if camera_number%30==1:
                    context=args.output/f"context-{camera_number:06d}.png"
                    entry={"file":context.name,"t_ms":(received-start)*1000}
                    metadata["contexts"].append(entry);image_entries.append(entry)
                    pending_images.append(writer.submit(save_image,context,frame))
                frame=atlas
            path = args.output / f"frame-{len(metadata['frames']):06d}.{args.image_format}"
            if sum(not future.done() for future in pending_images[-60:]) >= 30:
                raise RuntimeError("Image writer backlog exceeds one second")
            pending_images.append(writer.submit(save_image,path,frame))
            entry={"file": path.name, "t_ms": (received-start)*1000,
                "read_ms": (received-before)*1000,
                "width": frame.shape[1], "height": frame.shape[0]}
            metadata["frames"].append(entry);image_entries.append(entry)
            if finished_at is not None and time.perf_counter()-finished_at > .8:
                break
        if finished_at is None or not events or any(any(p) for p in events[-1]["pixels"]):
            raise RuntimeError("Missing confirmed final black/completion")
        gaps = [b["t_ms"]-a["t_ms"] for a,b in zip(metadata["frames"],metadata["frames"][1:])]
        metadata["timing"] = {"received_fps": 1000/statistics.mean(gaps), "max_gap_ms": max(gaps)}
        metadata["status"] = "recorded"
    except Exception as error:
        metadata["error"] = str(error)
        raise
    finally:
        if connection.is_open:
            try: connection.write(b"STOP\n")
            except serial.SerialException: metadata["stop_request_failed"] = True
            finally: connection.close()
        if cap is not None: cap.release()
        writer.shutdown(wait=True)
        for entry,future in zip(image_entries,pending_images):
            try:
                future.result()
                entry["sha256"] = digest(args.output/entry["file"])
            except Exception as error:
                metadata["status"] = "incomplete"
                metadata["error"] = str(error)
        (args.output / "events.json").write_text(json.dumps(events, indent=2), encoding="utf-8")
        metadata["events_sha256"] = digest(args.output / "events.json")
        if args.mode.startswith("resonance"):
            (args.output / "wake-events.json").write_text(json.dumps(wake_events, indent=2), encoding="utf-8")
            metadata["wake_events_sha256"] = digest(args.output / "wake-events.json")
            metadata["wake_source"] = "synthetic lab observations; no physical MCP inputs or BLE"
        (args.output / "capture.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    if metadata["status"] != "recorded": raise RuntimeError("Incomplete image evidence")
    print(json.dumps({"status": metadata["status"], "mode": args.mode,
                      "frames": len(metadata["frames"]), "events": len(events), "timing": metadata["timing"]}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["pilot", "calibrate", "twinkle", "burst", "ripple",
                                        "resonance", "resonance-transitions", "resonance-colors"])
    parser.add_argument("--port", required=True)
    parser.add_argument("--firmware", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--image-format",choices=["bmp","png"],default="png")
    parser.add_argument("--rois",type=Path)
    parser.add_argument("--exposure", type=float, default=-7)
    parser.add_argument("--watchdog-after", type=float, help="Stop KEEP messages at this host time; expect the lab watchdog")
    parser.add_argument("--stop-after", type=float, help="Send host STOP at this time during the optical transaction")
    args=parser.parse_args()
    if args.watchdog_after is not None and args.stop_after is not None:
        parser.error("Use one stop mechanism per transaction")
    if args.width>640 and args.mode!="pilot" and args.rois is None:
        parser.error("HD measurement needs --rois; first acquire a pilot and ROI layout")
    run(args)
