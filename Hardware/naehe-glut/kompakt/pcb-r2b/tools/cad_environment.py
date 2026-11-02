from pathlib import Path
import os
def configure():
 p=Path(__file__).resolve().parents[1]/"pruefung/kicad-config"
 p.mkdir(parents=True,exist_ok=True)
 os.environ["KICAD_CONFIG_HOME"]=str(p)
 return p
