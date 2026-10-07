"""Generate only a public known P256 test point; never a deployment credential."""
from pathlib import Path
import cryptography
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
if cryptography.__version__ != "45.0.7":
    raise RuntimeError("Pinned independent generator requires cryptography45.0.7")
public = ec.derive_private_key(2, ec.SECP256R1()).public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
header = "#pragma once\n// Known public test point; never deploy.\nstatic const std::uint8_t peerRegistrySecondKey[65] = {" + ",".join(f"0x{b:02x}" for b in public) + "};\n"
(Path(__file__).resolve().parents[1]/"test/test_configured_control/PeerRegistryVectors.h").write_text(header, encoding="utf-8")
print("Generated only the public known second P256 test point")
