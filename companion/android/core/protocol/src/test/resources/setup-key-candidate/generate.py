"""Independent Python cryptography fixtures. PUBLIC scalar 1; never a real setup key.

Run using cryptography >=45. Fixed salt/CEK/IV ONLY for reproducible tests.
"""
import base64
import hashlib
import json
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.keywrap import aes_key_wrap


def b64(value):
    return base64.urlsafe_b64encode(value).rstrip(b"=").decode("ascii")


algorithm = "PBES2-HS256+A128KW"
box = bytes(range(1, 17))
salt = bytes(range(16))
cek = bytes(range(32))
iv = bytes(range(12))
point = ec.derive_private_key(1, ec.SECP256R1()).public_key().public_numbers()
xy = point.x.to_bytes(32, "big") + point.y.to_bytes(32, "big")
public_cose = bytes.fromhex("a5010203262001215820") + xy[:32] + bytes.fromhex("225820") + xy[32:]
prefix = bytes.fromhex("3041020100301306072a8648ce3d020106082a8648ce3d030107042730250201010420")
der = prefix + (1).to_bytes(32, "big")
header = dict(alg=algorithm, enc="A256GCM", p2s=b64(salt), p2c=600000,
              typ="larp.setup-key.v1", cty="application/pkcs8", box=b64(box),
              kid=b64(hashlib.sha256(public_cose).digest()),
              jwk=dict(kty="EC", crv="P-256", x=b64(xy[:32]), y=b64(xy[32:])))


def encode(password, plain=der, text=None):
    text = text if text is not None else json.dumps(header, separators=(",", ":"))
    protected = b64(text.encode("utf-8"))
    kek = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), algorithm.encode() + b"\0" + salt, 600000, 16)
    sealed = AESGCM(cek).encrypt(iv, plain, protected.encode("ascii"))
    return ".".join(map(str, (protected, b64(aes_key_wrap(kek, cek)), b64(iv), b64(sealed[:-16]), b64(sealed[-16:]))))


password = "public-test-password-123"
unicode_password = "Prüfpasswort-🔐-12345"
cases = [dict(name="ascii-public-scalar-one", password=password, file=encode(password), accepted=True),
         dict(name="unicode-original-spaced-header", password=unicode_password,
              file=encode(unicode_password, text=json.dumps(dict(reversed(list(header.items()))), indent=1)), accepted=True)]
for name, plain in [("zero-scalar", prefix + bytes(32)),
                    ("order-scalar", prefix + int("FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551", 16).to_bytes(32, "big")),
                    ("wrong-key-pair", prefix + (2).to_bytes(32, "big")),
                    ("wrong-der-version", bytes([der[0], der[1], der[2], der[3], 1]) + der[5:]),
                    ("der-tail", der + b"\0")]:
    cases.append(dict(name=name, password=password, file=encode(password, plain), accepted=False))
cases.append(dict(name="authenticated-duplicate-escaped-field", password=password,
                  file=encode(password, text=json.dumps(header)[:-1] + ',"\\u0061lg":"' + algorithm + '"}'), accepted=False))
output = dict(note="PUBLIC scalar 1, deterministic test entropy; never import as a real setup key",
              box=box.hex(), xy=xy.hex(), kid=hashlib.sha256(public_cose).hexdigest(), cases=cases)
Path(__file__).with_name("vectors.json").write_text(json.dumps(output, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
