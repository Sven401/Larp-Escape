"""Materialize the public independent JSON vectors as a C++ test header."""
import argparse
import json
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output",type=Path,required=True)
args=parser.parse_args()
values=json.loads(Path(__file__).with_name("ble_vectors.json").read_text(encoding="utf-8"))
args.output.write_text("namespace fixtures {\n"+"\n".join("const unsigned char "+name+"[] = {"+",".join(map(str,bytes.fromhex(value)))+"};" for name,value in values.items())+"\n}\n",encoding="utf-8")
