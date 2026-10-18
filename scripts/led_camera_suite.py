"""One explicit USB optical test suite; never flash, retry commands or overwrite a prior run."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from led_camera import digest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",required=True)
    parser.add_argument("--firmware",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--camera",type=int,default=0)
    parser.add_argument("--exposure",type=float,default=-7)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    scripts=Path(__file__).resolve().parent
    source_names=("led_camera.py","led_camera_lab.py","led_camera_rois.py","led_camera_assess.py","led_camera_suite.py")
    summary={"schema":1,"status":"incomplete","firmware_sha256":digest(args.firmware),
             "analysis_sources":{name:digest(scripts/name) for name in source_names},"results":{}}
    summary["firmware_sources"]={str(path.relative_to(scripts.parent)):digest(path) for path in
        [scripts.parent/"firmware/led"/name for name in ("include/LedOutput.h","include/CameraSequence.h",
            "src/LedOutput.cpp","src/RmtLedBus.cpp","src/CameraLabMain.cpp","platformio.ini","partitions.csv")]}
    def call(script,*arguments):
        subprocess.run([sys.executable,str(scripts/script),*map(str,arguments)],check=True,timeout=120)
    def capture(mode):
        arguments=[mode,"--port",args.port,"--firmware",args.firmware,"--camera",args.camera,
                   "--exposure",args.exposure,"--output",args.output/mode]
        if mode!="pilot":arguments += ["--rois",args.output/"rois.json"]
        call("led_camera_lab.py",*arguments)
    try:
        capture("pilot")
        call("led_camera_rois.py","--capture",args.output/"pilot","--output",args.output/"rois.json")
        capture("calibrate")
        calibration=args.output/"calibration.json"
        call("led_camera_assess.py","calibrate","--capture",args.output/"calibrate","--output",calibration)
        summary["calibration_sha256"]=digest(calibration)
        for mode in ("twinkle","burst","ripple"):
            capture(mode)
            result=args.output/f"{mode}-result.json"
            call("led_camera_assess.py","assess","--capture",args.output/mode,
                 "--calibration",calibration,"--output",result)
            summary["results"][mode]={"passed":json.loads(result.read_text())["passed"],"sha256":digest(result)}
        summary["status"]="passed" if all(r["passed"] for r in summary["results"].values()) else "failed"
        if summary["analysis_sources"]!={name:digest(scripts/name) for name in source_names}:
            summary["status"]="analysis_source_changed"
    finally:
        (args.output/"suite.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
    print(json.dumps(summary))
    if summary["status"]!="passed":raise SystemExit(1)


if __name__=="__main__":main()
