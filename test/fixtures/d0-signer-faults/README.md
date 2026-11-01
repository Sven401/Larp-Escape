# D0 signer API/lifecycle seam

Compiles the actual `PsaD0FactSigner.cpp` against a deliberately noncryptographic
PSA seam. Checks borrowed-handle ownership, attributes, public-only export,
algorithm/length/error handling, cleared rejected outputs, key mismatch and
copies made before backend callbacks. No import, destroy, generation or private
export API is provided, so accidental use fails compilation/linking.

Artificial success bytes test API flow only. They are not valid ECDSA signatures
and provide no cryptographic qualification, trust, authorization or persistence
evidence. Real writer/decoder round trips and qualified PSA tests are separate.

Run `python test/fixtures/d0-signer-faults/run.py --compiler <g++>`.
