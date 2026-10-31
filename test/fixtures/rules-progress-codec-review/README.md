# Rules progress codec implementation checks

Root-authored representation tests, not independent approval. The empty catalog
and local plan exercise canonical bytes, every truncated prefix, output-capacity
boundaries, reuse, full uint64 values and INT32_MIN. They are deliberately not
compiler/planner, authority, signing, durable-owner or device positives. Decoded
applications remain non-executable, with no constructed override admission.

Reproduce with `python test/fixtures/rules-progress-codec-review/run.py
--compiler /path/to/g++ --build-dir private/rules-progress-codec-review`, or
`--binary /path/to/prebuilt/codec` for an already built test executable.

Current local run: **5683 representation checks**, 252-byte empty catalog and
1634-byte empty local application (including program fingerprint and event revision);
strict GCC 5.1 C++11/Wpedantic/Werror.
The first size-boundary run exposed stale output-count handling; the encoder
now ignores the old output count while validating capacity and aliasing.
Alias preflight preserves old flags/counts and returns rejection. Duplicate or
overflowing outgoing ranges reject before encoding; their opaque negative byte
fixtures deliberately do not represent valid COSE/signature positives.

Real compiled catalogs/plans, exact independent wire oracles, copied evidence,
signatures, aliasing, malformed complete inputs and full owner recovery remain
required next checks. These numbers are not independent approval.
