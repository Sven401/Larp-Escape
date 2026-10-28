#pragma once
#include "Companion/PeerProof.h"

namespace larp {
namespace companion {
// Public PSA API; no SDK-private types and no legacy Mbed TLS primitives.
// This proves key possession only, not link security, a bond or a role.
class PsaPeerProofCrypto final : public IPeerProofCrypto {
  public:
    bool validKey(const PeerPublicKey &) noexcept override;
    bool verify(const PeerPublicKey &, const PeerTranscript &,
                const PeerSignature &) noexcept override;
};
} // namespace companion
} // namespace larp
