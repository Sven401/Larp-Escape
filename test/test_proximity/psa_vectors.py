"""Generate public, independent SHA256-HMAC golden bytes for the actual PSA path."""
import argparse
import hmac
from pathlib import Path


def vectors():
    key = bytes(range(32))
    a, b = bytes([2,1,2,3,4,5]), bytes([2,1,2,3,4,6])
    pair = hmac.digest(key, b"LARP-PROX-PAIR/1" + a + b, "sha256")[:16]
    request = b"LPRX" + bytes([1,1,0,0]) + pair + a + b + bytes([1])*16 + (1).to_bytes(4,"little") + bytes([7])*16 + bytes(16)
    response = b"LPRX" + bytes([1,2,0,0]) + pair + b + a + bytes([2])*16 + (1).to_bytes(4,"little") + bytes([7])*16 + bytes([1])*16
    return {"EmptyHmac": hmac.digest(key,b"","sha256"),
            "Pmk": hmac.digest(key,b"LARP-PROX-PMK/1","sha256"),
            "Lmk": hmac.digest(key,b"LARP-PROX-LMK/1","sha256"),
            "Request": request+hmac.digest(key,request,"sha256"),
            "Response": response+hmac.digest(key,response,"sha256")}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    args.output.write_text("#pragma once\n"+"\n".join("constexpr unsigned char "+name+"[] = {"+",".join(map(str,value))+"};" for name,value in vectors().items())+"\n",encoding="utf-8")


if __name__ == "__main__": main()
