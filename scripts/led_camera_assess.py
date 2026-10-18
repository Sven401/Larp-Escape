"""Derive an optically verified index map and assess actual firmware frame traces."""
import argparse
import json
from pathlib import Path

import cv2
import numpy as np
from led_camera import digest, regions


def event_axis(run):
    device=np.array([event["completed_ms"] for event in run.events],dtype=float)
    host=np.array([event["host_ms"] for event in run.events],dtype=float)
    if not np.all(np.isfinite(device)) or np.any(np.diff(device)<=0):
        raise ValueError("Invalid device completion clock")
    # Preserve actual device intervals. The unknown constant host/device/UART
    # offset cancels against this run's independent optical markers.
    return device+np.median(host-device)


class Run:
    def __init__(self, root):
        self.root = root
        self.meta = json.loads((root / "capture.json").read_text(encoding="utf-8"))
        if self.meta["status"] != "recorded": raise ValueError("Incomplete capture")
        if digest(root / "events.json") != self.meta["events_sha256"]:
            raise ValueError("Changed event trace")
        self.events = json.loads((root / "events.json").read_text())
        if [e["sequence"] for e in self.events] != list(range(len(self.events))):
            raise ValueError("Nonconsecutive frame trace")
        self.times = np.array([f["t_ms"] for f in self.meta["frames"]])
        self.size = (self.meta["frames"][0]["width"],self.meta["frames"][0]["height"])
        if len(self.times) < 10 or np.any(np.diff(self.times) <= 0): raise ValueError("Invalid timeline")
        if self.meta["timing"]["max_gap_ms"] > 80: raise ValueError("Camera gap exceeds 80 ms")
        self.checked = set()

    def image(self, index):
        entry = self.meta["frames"][index]
        path = self.root / entry["file"]
        if index not in self.checked:
            if digest(path) != entry["sha256"]: raise ValueError("Changed camera frame")
            self.checked.add(index)
        image = cv2.imread(str(path))
        if image is None or image.shape != (self.size[1],self.size[0],3): raise ValueError("Invalid image size")
        return image[:, :, ::-1]  # RGB, camera code values

    def stable(self, stage, delay):
        events = [e for e in self.events if e["stage"] == stage]
        if len(events) != 1: raise ValueError(f"Stage {stage} missing/duplicated")
        event = events[0]
        # Dark stages last 200 ms, bright stages 400 ms (sync stages 500 ms).
        dark = not np.any(event["pixels"])
        low, high = (70,160) if dark else (140,320)
        time=event_axis(self)[event["sequence"]]
        selected = np.flatnonzero((self.times >= time+delay+low) &
                                  (self.times <= time+delay+high))
        if len(selected) < 2: raise ValueError(f"Too few stable images in stage {stage}")
        result = self.image(int(selected[0])).astype(np.float32)
        for i in selected[1:]: result += self.image(int(i))
        return result/len(selected)


def sync_delay(run,measured_brightness=None):
    sync = [e for e in run.events if e["stage"] < 6]
    if len(sync) != 6: raise ValueError("Need three complete optical sync markers")
    axis=event_axis(run)
    end=next(axis[event["sequence"]] for event in run.events if event["stage"]==6)-20
    selected = np.flatnonzero(run.times < end)
    brightness = (np.array([run.image(int(i)).mean() for i in selected]) if measured_brightness is None
                  else np.array(measured_brightness)[selected])
    times = run.times[selected]
    scores = []
    for delay in range(-100,501,5):
        expected = np.zeros(len(times))
        for event in sync:
            expected[times >= axis[event["sequence"]]+delay] = int(np.any(event["pixels"]))
        scores.append(float(np.corrcoef(brightness, expected)[0,1]))
    best = float(np.nanmax(scores))
    if not np.isfinite(best) or best < .9: raise ValueError("Optical synchronization ambiguous")
    # Each marker transition is bracketed by two actually received pictures.
    # Count this sampling interval once, rather than adding it again to a
    # correlation plateau that already arises from the same discrete samples.
    low,high=np.percentile(brightness,[10,90])
    states=brightness>(low+high)/2
    edges=np.flatnonzero(states[1:]!=states[:-1])+1
    if len(edges)!=6:raise ValueError("Ambiguous optical marker transitions")
    intervals=np.array([[times[i-1]-axis[event["sequence"]],times[i]-axis[event["sequence"]]]
                        for i,event in zip(edges,sync)])
    exposure=run.meta["settings"].get("exposure",{}).get("readback",-7)
    guard=1000*2**exposure/2 if -20<=exposure<=-1 else 0
    lower=float(np.max(intervals[:,0])-guard-1)
    upper=float(np.min(intervals[:,1])+guard+1)
    if lower>upper:raise ValueError("Inconsistent optical marker timing")
    delay=(lower+upper)/2
    uncertainty=(upper-lower)/2
    if uncertainty > 60: raise ValueError("Optical timing uncertainty exceeds 60 ms")
    return {"delay_ms": delay, "uncertainty_ms": uncertainty, "correlation": best,
            "edge_delay_intervals_ms":intervals.tolist(),"half_exposure_guard_ms":guard,
            "delay_bounds_ms":[lower,upper],"clock":"relative device TX_DONE intervals, constant host anchor",
            "method": "dedicated markers of this transaction; no fit to the tested effect"}


def centroid(delta,scale=None):
    scale=delta.shape[1]/640 if scale is None else scale
    positive = np.maximum(delta.mean(axis=2), 0)
    blurred = cv2.GaussianBlur(positive, (5,5), 0)
    _, maximum, _, point = cv2.minMaxLoc(blurred)
    if maximum < 8: raise ValueError("Missing/weak individual LED")
    x,y = point
    radius=round(6*scale)
    if x < radius+2 or y < radius+2 or x >= delta.shape[1]-radius-2 or y >= delta.shape[0]-radius-2:
        raise ValueError("LED at image boundary")
    patch = positive[y-radius:y+radius+1,x-radius:x+radius+1]
    yy,xx = np.indices(patch.shape)
    return (float(x-radius+(xx*patch).sum()/patch.sum()),
            float(y-radius+(yy*patch).sum()/patch.sum()))


def sample(image, points, masks=None,scale=None):
    values, clipped = [], []
    radius=round(3*(image.shape[1]/640 if scale is None else scale))
    yy,xx = np.ogrid[-radius:radius+1,-radius:radius+1]
    mask = xx*xx+yy*yy <= radius*radius
    for index,(x,y) in enumerate(points):
        x,y = round(x),round(y)
        if masks is None:
            pixels = image[y-radius:y+radius+1,x-radius:x+radius+1][mask]
        else:
            offsets = np.array(masks[index])
            pixels = image[y+offsets[:,1],x+offsets[:,0]]
        values.append(pixels.mean(axis=0))
        clipped.append(float(np.mean(np.any(pixels >= 250, axis=1))))
    return np.array(values), np.array(clipped)


def calibrate(root, output, mapping_only=False):
    run = Run(root)
    if run.meta["mode"] != "calibrate": raise ValueError("Not a calibration transaction")
    if len(run.events) != 134: raise ValueError("Incomplete calibration stage sequence")
    for stage,event in enumerate(run.events):
        expected = np.zeros((23,3),int)
        if stage < 6 and stage%2==0: expected[:]=16
        elif 6 <= stage < 98 and stage%2==0:
            local=stage-6
            index=local//2 if local<46 else 22-(local-46)//2
            expected[index]=16 if local<46 else 8
        elif 98 <= stage < 126 and stage%2==0:
            value=(1,2,4,8,16,24,32)[((stage-98)//2)%7]
            expected[:]=[value]*3 if stage<112 else [value//4,value//4,value]
        elif stage in (126,128,130): expected[:,(stage-126)//2]=32
        if event["stage"] != stage or not np.array_equal(event["pixels"],expected):
            raise ValueError(f"Wrong calibration firmware frame in stage {stage}")
    timing = sync_delay(run)
    delay = timing["delay_ms"]
    first=run.image(0)
    width,height=first.shape[1],first.shape[0]
    scale=run.meta.get("source_camera_size",[width,height])[0]/640
    points = []
    individual=[]
    for index in range(23):
        stage = 6+index*2
        delta=run.stable(stage,delay)-run.stable(stage+1,delay)
        individual.append(delta)
        points.append(centroid(delta,scale))
    distances = np.linalg.norm(np.array(points)[:,None]-np.array(points)[None,:],axis=2)
    np.fill_diagonal(distances, np.inf)
    if distances.min() < 12*scale: raise ValueError("Duplicate/overlapping LED assignments")
    verification = []
    for position in range(23):
        index,stage = 22-position,52+position*2
        delta = run.stable(stage,delay)-run.stable(stage+1,delay)
        response,_ = sample(delta,points,scale=scale)
        strength = response.mean(axis=1)
        other = np.delete(strength,index)
        center_error = float(np.linalg.norm(np.array(centroid(delta,scale))-points[index]))
        if strength[index] < 5 or np.max(other) > strength[index]*.35+3 or center_error > 3*scale:
            raise ValueError(f"Held-out individual LED {index} failed index verification")
        verification.append({"index": index, "signal": float(strength[index]),
                             "maximum_other": float(np.max(other)), "center_error_px": center_error})
    if mapping_only:
        result={"schema":1,"mapping_status":"verified","radiometry_status":"unassessed",
                "image_size":[width,height],"firmware_sha256":run.meta["firmware_sha256"],
                "capture_sha256":digest(root/"capture.json"),"timing":timing,
                "verification":verification,"settings":run.meta["settings"],
                "leds":[{"index":i,"x":round(x),"y":round(y),"radius":round(3*scale)}
                        for i,(x,y) in enumerate(points)]}
        with output.open("x",encoding="utf-8") as stream:json.dump(result,stream,indent=2)
        print("23 index positions independently verified; radiometry unassessed")
        return
    levels = [0,1,2,4,8,16,24,32]
    # Integrate several LED PWM cycles with a longer exposure. Direct LED cores
    # can saturate; select an empirical unsaturated footprint around each LED.
    # Selection uses calibration only and stays fixed in held-out effect runs.
    white32 = run.stable(110,delay)
    black32 = run.stable(111,delay)
    maximum = np.maximum.reduce([run.stable(stage,delay) for stage in (110,124,126,128,130)])
    masks=[]
    radius=round(14*scale)
    for index,(x,y) in enumerate(points):
        x,y=round(x),round(y)
        if not radius <= x < width-radius or not radius <= y < height-radius:
            raise ValueError("LED footprint at boundary")
        signal=(white32-black32)[y-radius:y+radius+1,x-radius:x+radius+1].mean(axis=2)
        peak=maximum[y-radius:y+radius+1,x-radius:x+radius+1].max(axis=2)
        yy,xx=np.indices(signal.shape)
        own=individual[index][y-radius:y+radius+1,x-radius:x+radius+1].mean(axis=2)
        other=np.maximum.reduce([delta[y-radius:y+radius+1,x-radius:x+radius+1].mean(axis=2)
                                 for number,delta in enumerate(individual) if number!=index])
        selected=(signal>5)&(peak<180)&(own>np.maximum(2,3*other+2))&((xx-radius)**2+(yy-radius)**2<=radius*radius)
        rows,cols=np.where(selected)
        if len(rows)<5: raise ValueError("Too few unsaturated footprint pixels; use optical attenuation")
        masks.append([[int(cx-radius),int(cy-radius)] for cy,cx in zip(rows,cols)])
    responses = {}
    for family,start in (("white",98),("cool",112)):
        table = [np.zeros((23,3))]
        for step in range(7):
            stage = start+step*2
            bright = run.stable(stage,delay)
            black = run.stable(stage+1,delay)
            values,clipped = sample(bright,points,masks)
            if np.any(clipped > 0): raise ValueError("Calibration clipped; reduce exposure")
            dark,_ = sample(black,points,masks)
            table.append(np.maximum(values-dark,0))
        responses[family] = np.array(table).tolist()
        luminance=np.array(table).mean(axis=2)
        if np.any(np.diff(luminance,axis=0) < -(.15*luminance[-1]+3)):
            raise ValueError("Nonmonotonic camera response; unstable exposure/PWM or optical setup")
    full = np.array(responses["white"])[-1].mean(axis=1)
    if np.any(full < 20): raise ValueError("Insufficient camera dynamic range")
    # Primary color observations retained for review; effect scoring uses the
    # empirically measured white/cool families, not an assumed RGB gamma model.
    primaries = {}
    for stage,color in ((126,"red"),(128,"green"),(130,"blue")):
        bright = run.stable(stage,delay)
        values,clipped = sample(bright,points,masks)
        if np.any(clipped > 0): raise ValueError("Primary color clipped; reduce exposure")
        dark,_ = sample(run.stable(stage+1,delay),points,masks)
        response=np.maximum(values-dark,0)
        channel={"red":0,"green":1,"blue":2}[color]
        other=np.delete(response,channel,axis=1).max(axis=1)
        if np.any(response[:,channel]<5) or np.any(response[:,channel]<=other*1.2+2):
            raise ValueError(f"Primary {color} does not dominate the expected camera channel")
        primaries[color] = response.tolist()
    result = {"schema": 2, "mapping_status": "verified", "image_size": [width,height],
              "stable_frame_reduction":"arithmetic mean within fixed settled windows",
              "camera_index":run.meta.get("camera_index",0),
              "firmware_sha256": run.meta["firmware_sha256"], "settings": run.meta["settings"],
              "capture_sha256": digest(root/"capture.json"), "timing": timing,
              "leds": [{"index":i,"x":round(x),"y":round(y),"radius":radius,"offsets":masks[i]}
                       for i,(x,y) in enumerate(points)],
              "verification": verification, "levels": levels, "responses": responses,
              "primaries": primaries,
              "scope": "verified index map and empirical camera response; no electrical/photometric calibration"}
    if "roi_layout" in run.meta:
        result["roi_layout_sha256"]=run.meta["roi_layout_sha256"]
        result["source_camera_size"]=run.meta["source_camera_size"]
        layout=run.meta["roi_layout"];size=layout["tile_size"]
        for led in result["leds"]:
            tile=(led["y"]//size)*layout["columns"]+(led["x"]//size)
            region=layout["regions"][tile]
            led["camera_x"]=region["x"]-size//2+led["x"]%size
            led["camera_y"]=region["y"]-size//2+led["y"]%size
    with output.open("x",encoding="utf-8") as stream: json.dump(result,stream,indent=2)
    image = run.stable(0,delay)[:,:,::-1].astype("uint8")
    for i,(x,y) in enumerate(points):
        x,y = round(x),round(y)
        cv2.circle(image,(x,y),8,(0,255,0),1)
        cv2.putText(image,str(i),(x+9,y),cv2.FONT_HERSHEY_SIMPLEX,.4,(0,255,0),1)
    cv2.imwrite(str(output.with_suffix(".png")),image)
    print(json.dumps({"mapping":"verified","leds":23,"timing":timing}))


def assess(root, calibration, output):
    run = Run(root)
    if run.meta["mode"] not in ("twinkle","burst","ripple"):
        raise ValueError("Not an effect test transaction")
    timing=sync_delay(run)
    config = json.loads(calibration.read_text())
    if config["schema"] != 2 or config["mapping_status"] != "verified": raise ValueError("Unverified calibration")
    first=run.image(0)
    regions(config,first.shape[1],first.shape[0])
    if config.get("camera_index",0) != run.meta.get("camera_index",0): raise ValueError("Camera index changed")
    if config.get("roi_layout_sha256") != run.meta.get("roi_layout_sha256"):
        raise ValueError("Acquisition layout changed")
    if config["levels"] != [0,1,2,4,8,16,24,32]: raise ValueError("Invalid response levels")
    for family in ("white","cool"):
        table=np.array(config["responses"][family])
        if table.shape != (8,23,3) or not np.all(np.isfinite(table)) or np.any(table<0):
            raise ValueError("Invalid response table")
    if config["firmware_sha256"] != run.meta["firmware_sha256"]: raise ValueError("Firmware changed since calibration")
    for name in config["settings"]:
        if config["settings"][name] != run.meta["settings"][name]: raise ValueError(f"Camera setting changed: {name}")
    points = [(led["x"],led["y"]) for led in config["leds"]]
    masks=[led["offsets"] for led in config["leds"]]
    observed,clipping = [],[]
    for i in range(len(run.times)):
        values,clipped = sample(run.image(i),points,masks)
        observed.append(values); clipping.append(clipped)
    observed = np.array(observed)
    led_timing=[sync_delay(run,observed[:,led].mean(axis=1)) for led in range(23)]
    baseline = np.median(observed[run.times < run.meta["command_sent_ms"]],axis=0)
    observed = observed-baseline
    events = run.events
    axis=event_axis(run)
    pixels = np.array([e["pixels"] for e in events])
    expected = np.zeros_like(observed)
    family = "cool" if run.meta["mode"] == "ripple" else "white"
    lut = np.array(config["responses"][family])
    for led in range(23):
        lookup=np.searchsorted(axis+led_timing[led]["delay_ms"],run.times,side="right")-1
        for frame,event in enumerate(lookup):
            if event < 0 or events[event]["stage"]<7:continue
            r,g,b = pixels[event,led]
            if not ((r==g==b) if family=="white" else (r==g==b//4)):
                raise ValueError("Firmware emitted a color outside the calibrated family")
            for channel in range(3):
                expected[frame,led,channel] = np.interp(b,config["levels"],lut[:,led,channel])
    first_effect=next(axis[e["sequence"]] for e in events if e["stage"]==7)+timing["delay_ms"]
    selected=run.times>=first_effect-200
    observed=observed[selected];expected=expected[selected]
    measured_times=run.times[selected]
    clipping=np.array(clipping)[selected]
    # Fixed 3-frame smoothing limits sensitivity to host receive quantization.
    def smooth(values): return (values[:-2]+values[1:-1]+values[2:])/3
    full = np.array(config["responses"]["white"])[-1].mean(axis=1)
    actual_curve = smooth(observed).mean(axis=2)
    ideal_curve = smooth(expected).mean(axis=2)
    normalized = np.abs(smooth(observed)-smooth(expected)).mean(axis=2)/full
    results = []
    for led in range(23):
        off=measured_times>=axis[-1]+led_timing[led]["delay_ms"]+200
        if np.sum(off)<10:raise ValueError("Insufficient final darkness observation")
        correlation = (float(np.corrcoef(actual_curve[:,led],ideal_curve[:,led])[0,1])
                       if np.std(actual_curve[:,led]) > 0 and np.std(ideal_curve[:,led]) > 0 else float("nan"))
        mae = float(np.mean(normalized[:,led]))
        p95 = float(np.percentile(normalized[:,led],95))
        dark = float(np.percentile(np.maximum(observed[off,led].mean(axis=1),0),95)/full[led])
        active = float(np.max(actual_curve[:,led])/full[led])
        passed = bool(np.isfinite(correlation) and correlation >= .90 and mae <= .12 and p95 <= .25
                      and dark <= .05 and active >= .10)
        results.append({"led":led,"correlation":correlation if np.isfinite(correlation) else None,
                        "normalized_mae":mae,"normalized_p95":p95,"final_dark_fraction":dark,
                        "peak_fraction":active,"passed":passed,"timing":led_timing[led]})
    clipped = float(np.max(clipping))
    result = {"schema":1,"mode":run.meta["mode"],"calibration_sha256":digest(calibration),
              "capture_sha256":digest(root/"capture.json"),"firmware_sha256":run.meta["firmware_sha256"],
              "passed":bool(all(led["passed"] for led in results) and clipped==0),
              "maximum_clipped_fraction":clipped,"leds":results,
              "criteria":{"correlation_min":.90,"normalized_mae_max":.12,"normalized_p95_max":.25,
                          "final_dark_max":.05,"peak_min":.10,"clipping_max":0},
              "scope":"optical agreement with actual TX-completed frames in isolated lab firmware; no BLE/product installation acceptance",
              "timing":timing,"calibration_timing":config["timing"]}
    with output.open("x",encoding="utf-8") as stream: json.dump(result,stream,indent=2,allow_nan=False)
    print(json.dumps({"mode":result["mode"],"passed":result["passed"],
                      "failed_leds":[led["led"] for led in results if not led["passed"]],
                      "maximum_clipped_fraction":clipped}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command",choices=["map","calibrate","assess"])
    parser.add_argument("--capture",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--calibration",type=Path)
    args = parser.parse_args()
    if args.command in ("map","calibrate"): calibrate(args.capture,args.output,args.command=="map")
    else:
        if args.calibration is None: parser.error("assess requires --calibration")
        assess(args.capture,args.calibration,args.output)
