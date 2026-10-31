"""Independent canonical Bundle-v1 byte oracle; never imports product codecs."""
import hashlib
import json
from pathlib import Path


def head(major, value):
    assert 0 <= value < 2**64
    if value < 24:
        return bytes([(major << 5) | value])
    for width, additional in ((1, 24), (2, 25), (4, 26), (8, 27)):
        if value < 2 ** (8 * width):
            return bytes([(major << 5) | additional]) + value.to_bytes(width, "big")
    raise AssertionError(value)


def cbor(value):
    if type(value) is int:
        return head(0, value)
    if isinstance(value, bytes):
        return head(2, len(value)) + value
    if isinstance(value, str):
        encoded = value.encode("utf-8")
        return head(3, len(encoded)) + encoded
    if isinstance(value, list):
        return head(4, len(value)) + b"".join(cbor(x) for x in value)
    if isinstance(value, dict):
        return head(5, len(value)) + b"".join(cbor(k) + cbor(value[k]) for k in sorted(value))
    raise TypeError(type(value))


def sha(value):
    return hashlib.sha256(value).digest()


def blob(size):
    # Opaque canonical storage-layer fixtures, not a fabricated RulesProgressCodec.
    if size == 1:
        return b"\x00"
    for width in range(1, 10):
        n = size - width
        if n >= 0 and len(head(2, n)) == width:
            return cbor(bytes((i * 17 + 9) % 256 for i in range(n)))
    raise AssertionError(size)


def key(kind, size, catalog):
    raw = blob(size)
    return {0: "larp.rules.bundle.v1", 2: kind, 3: bytes([0x40 + kind]) * 16,
            4: bytes([0x10 + kind]) * 32, 5: bytes([0x23]) * 32,
            6: catalog, 7: sha(raw), 8: size}, raw


def wire(common, raw, first_index=0, first_identity=0x80):
    count = (len(raw) + 2047) // 2048
    chunks, links = [], []
    for i in range(count):
        payload = cbor(common | {1: 1, 9: count, 10: i,
                        11: raw[i * 2048:(i + 1) * 2048]})
        identity = bytes([first_identity + i]) * 16
        chunks.append({"index": first_index + i, "operation": identity.hex(),
                       "payload": payload.hex(), "request": sha(payload).hex()})
        links.append([first_index + i, identity, sha(payload)])
    final = cbor(common | {1: 2, 9: count, 10: links})
    return chunks, final


def generate():
    catalog = sha(blob(1))
    rows = []
    for kind, sizes in ((1, (1, 2048, 2049, 32768)), (2, (1, 2048, 2049, 32768))):
        for size in sizes:
            common, raw = key(kind, size, catalog)
            if kind == 1:
                common[6] = sha(raw)
            chunks, final = wire(common, raw, 0 if kind == 1 else 2, 0x80 if kind == 1 else 0x81)
            rows.append({"name": f"kind{kind}-{size}", "kind": kind,
                         "blob": raw.hex(), "blob_sha256": sha(raw).hex(),
                         "chunks": chunks, "final": final.hex(),
                         "final_sha256": sha(final).hex()})
    return {"scope": "Independent Bundle-v1 canonical bytes; storage only, no Owner or D1 semantic codec claim",
            "vectors": rows, "malformed": malformed()}


def malformed():
    common, raw = key(1, 2049, bytes(32))
    common[6] = common[7]
    def chunk(i=0, changes=None):
        v = common | {1: 1, 9: 2, 10: i, 11: raw[i*2048:(i+1)*2048]}
        return v | (changes or {})
    def rec(payload, identity=0x80, **fields):
        return {"operation": bytes([identity]*16).hex(), "request": sha(payload).hex(),
                "kind": 5, "issues": 0, "profile": 17, "trust": 23,
                "payload": payload.hex()} | fields
    rows = []
    def case(name, error, records):
        rows.append({"name": name, "error": error, "records": records})
    for label, value in ((0,"wrong.domain"),(1,0),(1,3),(2,0),(2,3),(3,bytes(16)),
                         (3,bytes(15)),(4,bytes(31)),(5,bytes(33)),(6,bytes(31)),
                         (7,bytes(31)),(8,0),(8,32769),(8,2**32),(9,0),(9,17),
                         (10,2),(11,b""),(11,raw[:2047])):
        case(f"field-{label}-{len(rows)}", "Canonical", [rec(cbor(chunk(changes={label:value})))])
    canonical=cbor(chunk())
    case("trailing-byte", "Canonical", [rec(canonical+b"\x00")])
    case("unknown-map-label", "Canonical", [rec(cbor(chunk(changes={12:0})))])
    case("nonminimal-map-length", "Canonical", [rec(b"\xb8\x0c"+canonical[1:])])
    case("indefinite-map", "Canonical", [rec(b"\xbf"+canonical[1:]+b"\xff")])
    case("duplicate-map-label", "Canonical", [rec(b"\xad"+canonical[1:]+cbor(11)+cbor(b"x"))])
    case("header-request-substitution", "DigestMismatch", [rec(canonical, request=bytes(32).hex())])
    case("chunk-parent-identity", "Conflict", [rec(canonical, identity=0x41)])
    case("chunk-allocates-id", "Conflict", [rec(canonical, issues=1)])
    case("chunk-application-header", "Conflict", [rec(canonical, kind=2)])
    for field,value in ((4,bytes([0x91])*32),(5,bytes([0x92])*32),(6,bytes([0x93])*32),(7,bytes([0x94])*32)):
        case(f"orphan-parent-field-{field}","Conflict",[rec(canonical),rec(cbor(chunk(1,({6:value,7:value} if field in (6,7) else {field:value}))),0x81)])
    case("duplicate-chunk-index", "Conflict", [rec(canonical),rec(canonical,0x81)])
    case("orphan-header-profile", "Conflict", [rec(canonical),rec(cbor(chunk(1)),0x81,profile=18)])
    case("orphan-header-trust", "Conflict", [rec(canonical),rec(cbor(chunk(1)),0x81,trust=24)])
    case("orphan-header-last-issued", "Conflict", [rec(canonical),rec(cbor(0),0x70,kind=1,issues=1),rec(cbor(chunk(1)),0x81)])
    refs=[[0,bytes([0x80])*16,sha(canonical)], [1,bytes([0x81])*16,sha(cbor(chunk(1)))]]
    final=common | {1:2,9:2,10:refs}
    def finalrec(v):
        return rec(cbor(v),0x41)
    case("final-missing-all-chunks","MissingChunk",[finalrec(final)])
    case("final-missing-second-chunk","MissingChunk",[rec(canonical),finalrec(final)])
    full=[rec(canonical),rec(cbor(chunk(1)),0x81)]
    case("final-ref-wrong-digest","MissingChunk",full+[finalrec(final | {10:[refs[0],[1,refs[1][1],bytes(32)]]})])
    case("final-ref-wrong-identity","MissingChunk",full+[finalrec(final | {10:[refs[0],[1,bytes([0x99])*16,refs[1][2]]]})])
    case("final-ref-reversed","Canonical",full+[finalrec(final | {10:list(reversed(refs))})])
    case("final-ref-duplicate-index","Canonical",full+[finalrec(final | {10:[refs[0],refs[0]]})])
    case("final-list-short","Canonical",full+[finalrec(final | {10:refs[:1]})])
    case("second-final","Conflict",full+[finalrec(final),rec(cbor(final),0x99)])
    false_chunks=[cbor(chunk(i,{6:bytes(32),7:bytes(32)})) for i in range(2)]
    false_refs=[[i,bytes([0x80+i])*16,sha(raw)] for i,raw in enumerate(false_chunks)]
    false_final=final | {6:bytes(32),7:bytes(32),10:false_refs}
    case("actual-blob-hash-mismatch","DigestMismatch",[rec(false_chunks[0]),rec(false_chunks[1],0x81),finalrec(false_final)])

    # A short valid chunk supplies every strictly shorter prefix without huge media.
    c, b=key(1,1,bytes(32));c[6]=c[7]
    minimal=cbor(c | {1:1,9:1,10:0,11:b})
    for size in range(len(minimal)):
        case(f"prefix-{size}","Canonical",[rec(minimal[:size])])
    return rows


if __name__ == "__main__":
    path = Path(__file__).with_name("vectors.json")
    path.write_text(json.dumps(generate(), indent=2) + "\n", encoding="utf-8")
