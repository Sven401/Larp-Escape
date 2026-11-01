# Rules effect codec implementation checks

Root-authored implementation tests, not independent review. Five exact fixed
Python byte oracles cover Claim and HandlerReturned/HandlerFailed/Unknown,
intent indices 0/31, all integer-width boundaries through UINT64_MAX, and exact
physical phase-ID/header binding. Strict C++11/Wpedantic/Werror: **1515 checks**.
All shorter prefixes, every output capacity below the actual frame length,
trailing/nonminimal/indefinite forms, cleared output/reuse and aliases are checked.

Reproduce: `python test/fixtures/rules-effect-own/run.py --compiler /path/to/g++
--build-dir private/rules-effect-own`, or `--binary /path/to/effect`.
`generate.py` preserves editable byte-oracle sources and `vectors.json` the bytes.

No storage write, Claim ordering, effect execution, observation, authority,
entropy or device proof. Raw SHA, parent/full intent cause, original eligibility,
once-only order and recovery belong to the actual owner. In particular a handler
return is not D0 observed. No physical/logical revision equality is invented.
