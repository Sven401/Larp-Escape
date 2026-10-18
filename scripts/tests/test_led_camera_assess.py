"""Independent optical fixtures: mapping, timing, missing LED, wrong color and stuck output."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

import numpy as np
import cv2

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import led_camera_assess as assess
from led_camera_lab import parse_frame,pack_frame
from led_camera_rois import acquire
from led_camera import digest

POINTS = [(50+(i%8)*65,70+(i//8)*130) for i in range(23)]


class OpticalFixture:
    stable = assess.Run.stable
    def __init__(self,root,mode="calibrate",fault=None):
        self.root,self.fault = root,fault
        self.meta = {"mode":mode,"command_sent_ms":500,"firmware_sha256":"fixture",
            "settings":{k:{"requested":0,"readback":0,"driver_accepted":True}
                        for k in ("width","height","fps","exposure","auto_wb")}}
        self.events=[]
        cursor=500
        def event(stage,pixels,duration):
            nonlocal cursor
            self.events.append({"stage":stage,"sequence":len(self.events),"host_ms":cursor,
                                "completed_ms":1000000+cursor,
                                "pixels":pixels.tolist()})
            cursor+=duration
        def all_pixels(value): return np.tile(value,(23,1))
        if mode=="calibrate":
            for stage in range(6): event(stage,all_pixels([16]*3 if stage%2==0 else [0]*3),500)
            for pass_number,indices in enumerate((range(23),reversed(range(23)))):
                for position,index in enumerate(indices):
                    stage=6+pass_number*46+position*2
                    pixels=np.zeros((23,3),int)
                    pixels[index]=16 if pass_number==0 else 8
                    event(stage,pixels,400); event(stage+1,all_pixels([0]*3),200)
            for family in range(2):
                for number,value in enumerate((1,2,4,8,16,24,32)):
                    stage=98+family*14+number*2
                    rgb=[value]*3 if family==0 else [value//4,value//4,value]
                    event(stage,all_pixels(rgb),400); event(stage+1,all_pixels([0]*3),200)
            for stage,color in ((126,[32,0,0]),(128,[0,32,0]),(130,[0,0,32])):
                event(stage,all_pixels(color),400);event(stage+1,all_pixels([0]*3),200)
            event(132,all_pixels([0]*3),1000)
            event(133,all_pixels([0]*3),0)
        else:
            for stage in range(6):event(stage,all_pixels([16]*3 if stage%2==0 else [0]*3),500)
            event(6,all_pixels([0]*3),1000)
            for number in range(49):
                value=round(32*(1-abs(number-24)/24))
                event(7,all_pixels([value]*3),33)
            event(7,all_pixels([0]*3),1000)
        self.times=np.arange(33,cursor+800,33)

    def image(self,index):
        lag=300 if self.fault=="camera_delay" else 60
        if self.fault=="timing" and self.times[index]>=next(e["host_ms"] for e in self.events if e["stage"]==7)+60:
            lag=300 # Incorrect effect timing, while the dedicated markers remain correct.
        when=self.times[index]-lag
        active=[e for e in self.events if e["host_ms"]<=when]
        pixels=np.array(active[-1]["pixels"] if active else [[0]*3]*23)
        if self.fault=="rolling":
            for led in range(23):
                matches=[e for e in self.events if e["host_ms"]<=self.times[index]-60-6*(led%7)]
                pixels[led]=matches[-1]["pixels"][led] if matches else [0]*3
        apply_fault=self.meta["mode"]=="calibrate" or (active and active[-1]["stage"]>=7)
        if self.fault=="missing" and apply_fault: pixels[4]=0
        if self.fault=="color" and apply_fault: pixels[:,0:2]=0
        if self.fault=="stuck" and apply_fault: pixels[4]=16
        image=np.zeros((480,640,3),float)
        for led,(x,y) in enumerate(POINTS):
            cv2.circle(image,(x,y),4,tuple(float(c*4) for c in pixels[led]),-1)
        return image


class AssessmentTests(unittest.TestCase):
    def test_native_frame_intervals_survive_host_receive_quantization(self):
        run=SimpleNamespace(events=[{"completed_ms":t,"host_ms":h} for t,h in
                                    ((1000,1050),(1033,1116),(1066,1116))])
        np.testing.assert_allclose(np.diff(assess.event_axis(run)),[33,33])

    def test_acquisition_is_unverified_and_tiles_preserve_exact_pixels(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            blank=np.zeros((480,640,3),dtype=np.uint8)
            lit=blank.copy()
            for point in POINTS: cv2.circle(lit,point,4,(128,128,128),-1)
            entries=[]
            for name,image in (("dark.png",blank),("lit.png",lit)):
                self.assertTrue(cv2.imwrite(str(root/name),image))
                entries.append({"file":name,"sha256":digest(root/name)})
            (root/"capture.json").write_text(json.dumps({"status":"recorded","mode":"pilot","frames":entries}))
            output=root/"rois.json"
            acquire(root,output)
            layout=json.loads(output.read_text())
            self.assertIn("unverified",layout["status"])
            size=layout["tile_size"];half=size//2
            for index,region in enumerate(layout["regions"]):
                x,y=region["x"],region["y"]
                blank[y-half:y+half,x-half:x+half]=[index+1,2,3]
            atlas=pack_frame(blank,layout)
            for index,region in enumerate(layout["regions"]):
                left,top=(index%5)*size,(index//5)*size
                x,y=region["x"],region["y"]
                expected=blank[y-half:y+half,x-half:x+half].copy()
                yy,xx=np.ogrid[-half:half,-half:half]
                expected[xx*xx+yy*yy>region["aperture_radius"]**2]=0
                np.testing.assert_array_equal(atlas[top:top+size,left:left+size],expected)
            layout["source_image_size"]=[1,1]
            with self.assertRaises(ValueError):pack_frame(blank,layout)

    def test_frame_parser_rejects_wrong_token_size_brightness_and_time(self):
        token="0123456789abcdef"
        valid=f"F {token} 0 0 10 12 "+"000000"*23
        self.assertEqual(parse_frame(valid,token)["sequence"],0)
        for invalid in (valid.replace(token,"x"),valid[:-2],valid.replace("10 12","20 12"),
                        valid[:-6]+"ff0000"):
            with self.assertRaises(ValueError): parse_frame(invalid,token)

    def test_independent_map_and_effect_faults(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            (root/"capture.json").write_text("fixture")
            calibration=root/"calibration.json"
            fixture=OpticalFixture(root)
            with patch.object(assess,"Run",return_value=fixture): assess.calibrate(root,calibration)
            config=json.loads(calibration.read_text())
            self.assertEqual([(p["x"],p["y"]) for p in config["leds"]],POINTS)
            self.assertEqual(len(config["verification"]),23)
            self.assertLess(abs(config["timing"]["delay_ms"]-60),35)
            for fault in (None,"missing","color","stuck","timing","camera_delay","rolling"):
                with self.subTest(fault=fault):
                    fixture=OpticalFixture(root,"burst",fault)
                    output=root/f"{fault}.json"
                    with patch.object(assess,"Run",return_value=fixture):
                        assess.assess(root,calibration,output)
                    report=json.loads(output.read_text())
                    self.assertEqual(report["passed"],fault in (None,"camera_delay","rolling"))
                    if fault=="rolling":
                        self.assertLess(abs(report["leds"][0]["timing"]["delay_ms"]-60),10)
                        self.assertLess(abs(report["leds"][6]["timing"]["delay_ms"]-96),10)
            missing=OpticalFixture(root,fault="missing")
            with patch.object(assess,"Run",return_value=missing),self.assertRaises(ValueError):
                assess.calibrate(root,root/"bad-map.json")


if __name__=="__main__": unittest.main()
