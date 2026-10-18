"""Find acquisition regions from a short overview; these are NOT verified LED indices."""
import argparse
import json
from pathlib import Path
import cv2
import numpy as np
from led_camera import digest


def acquire(capture,output):
    meta=json.loads((capture/"capture.json").read_text())
    if meta["status"]!="recorded" or meta["mode"]!="pilot": raise ValueError("Need completed pilot")
    peak=baseline=None
    for entry in meta["frames"]:
        path=capture/entry["file"]
        if digest(path)!=entry["sha256"]:raise ValueError("Changed pilot frame")
        image=cv2.imread(str(path))
        if baseline is None:baseline=image.astype(float)
        peak=image if peak is None else np.maximum(peak,image)
    delta=np.maximum(peak.astype(float)-baseline,0).max(axis=2)
    regions=None
    for threshold in (220,200,160,120,80,40,20):
        count,_,stats,centers=cv2.connectedComponentsWithStats((delta>threshold).astype('uint8'))
        candidates=[]
        for number in range(1,count):
            if 4<=stats[number,cv2.CC_STAT_AREA]<=2000:
                x,y=[round(v) for v in centers[number]]
                if 40<=x<image.shape[1]-40 and 40<=y<image.shape[0]-40:
                    candidates.append({"x":x,"y":y,"component_area":int(stats[number,cv2.CC_STAT_AREA])})
        # Bright optical halos can split into several components. Keep the
        # strongest local component, without treating those fragments as LEDs.
        distinct=[]
        for candidate in sorted(candidates,key=lambda p:-p["component_area"]):
            if all((candidate["x"]-p["x"])**2+(candidate["y"]-p["y"])**2>=32**2 for p in distinct):
                distinct.append(candidate)
        candidates=sorted(distinct,key=lambda p:(p["y"],p["x"]))
        if len(candidates)==23:
            regions=candidates;break
    if regions is None:raise ValueError("No unambiguous set of 23 acquisition regions")
    for region in regions:
        separation=min(np.hypot(region["x"]-other["x"],region["y"]-other["y"])
                       for other in regions if other is not region)
        region["aperture_radius"]=min(40,int(separation*.45))
    result={"schema":1,"status":"acquisition only; firmware index mapping unverified",
            "source_image_size":[image.shape[1],image.shape[0]],"tile_size":80,"columns":5,
            "regions":regions,"threshold":threshold,"pilot_sha256":digest(capture/"capture.json")}
    with output.open("x") as stream:json.dump(result,stream,indent=2)
    print("23 acquisition regions; index assignment still requires the full calibration")


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();acquire(args.capture,args.output)
