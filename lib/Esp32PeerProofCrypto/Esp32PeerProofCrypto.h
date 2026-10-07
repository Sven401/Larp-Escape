#pragma once
#include "Companion/PeerProof.h"
namespace larp {
namespace companion {
// Real Mbed TLS P-256/SHA256 public-key verifier. Library allocates temporary
// MPI/group storage; use ONLY on a separate worker, never under a source lock.
// No signing, private key, RNG, transport flags, role grant or SDK init here.
class Esp32PeerProofCrypto : public IPeerProofCrypto {
  public:
    bool validKey(const PeerPublicKey &) noexcept override;
    bool verify(const PeerPublicKey &, const PeerTranscript &,
                const PeerSignature &) noexcept override;
};
} // namespace companion
} // namespace larp
