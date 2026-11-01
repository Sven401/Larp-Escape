"""Own independent-language byte oracle; not independent reviewer approval."""
import json
from pathlib import Path


def head(major, value):
    if value < 24:
        return bytes([(major << 5) | value])
    size = next(n for n in (1, 2, 4, 8) if value < 1 << (8 * n))
    return bytes([(major << 5) | {1: 24, 2: 25, 4: 26, 8: 27}[size]]) + value.to_bytes(size, "big")


def encode(value):
    if isinstance(value, int):
        return head(0, value)
    if isinstance(value, bytes):
        return head(2, len(value)) + value
    if isinstance(value, str):
        raw = value.encode("utf8")
        return head(3, len(raw)) + raw
    if isinstance(value, list):
        return head(4, len(value)) + b"".join(map(encode, value))
    if isinstance(value, dict):
        return head(5, len(value)) + b"".join(encode(k) + encode(v) for k, v in sorted(value.items()))
    raise TypeError(type(value))


def main():
    vectors = []
    for phase, result, index in [(1, 0, 0), (1, 0, 31), (2, 1, 0), (2, 2, 31), (2, 3, 1)]:
        frame = {0: "larp.rules.effect.v1", 1: bytes(range(16)), 2: bytes(range(32)),
                 3: index, 4: bytes(range(16, 32)),
                 5: [0, 1, 23, 24, 255, 256, 65535, 65536, 4294967296, 18446744073709551615],
                 6: phase, 7: result}
        vectors.append({"phase": phase, "result": result, "index": index, "hex": encode(frame).hex()})
    Path(__file__).with_name("vectors.json").write_text(json.dumps(vectors, indent=2) + "\n", encoding="utf8")


if __name__ == "__main__":
    main()
