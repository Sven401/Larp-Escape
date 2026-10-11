#include "CompanionIntegration/DiagnosticGattsRoute.h"
#include "Companion/AuthorityState.h"
#include <cstdio>
#include <cstdlib>
using namespace larp;
using namespace larp::companion;
using namespace larp::diagnostic;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "FAIL %u: %s\n", __LINE__, #x); std::exit(1); } } while (false)
struct Publisher : IAuthorityPublisher {
    AuthorityState state;
    AuthorityUpdate advanceAuthority(std::uint32_t p, bool gm, TrustedAuthority &out) noexcept override { return state.advance(p, gm, out); }
    AuthorityUpdate advanceAuthorityIfCurrent(const TrustedAuthority &old, std::uint32_t p, bool gm, TrustedAuthority &out) noexcept override { return state.advanceIfCurrent(old, p, gm, out); }
    AuthorityUpdate closeAuthorityIfCurrent(const TrustedAuthority &old, TrustedAuthority &out) noexcept override { return state.closeIfCurrent(old, out); }
    void suspendAuthority() noexcept override { state.suspend(); }
    bool readAuthority(TrustedAuthority &out) const noexcept override { return state.read(out); }
};
static Identity identity(unsigned n) { Identity value; value.bytes[0] = static_cast<std::uint8_t>(n); return value; }
struct Bearer : IOriginalGattsNotify {
    bool live = true, accept = true;
    unsigned calls = 0;
    bool originalLive(const GattsRouteOrigin &v) const noexcept override { return live && v.sdkGeneration == 9 && v.interface == 3 && v.peerAddress[0] == 7; }
    bool notifyOriginal(const GattsRouteOrigin &v, std::uint16_t attribute, const std::uint8_t *data, std::size_t n) noexcept override {
        ++calls;
        return accept && originalLive(v) && attribute == 42 && data && n == 88;
    }
};
int main() {
    CHECK(GattsRouteV1::Version == 1);
    CHECK(GattsRouteV1::service()[15] == 0x10);
    CHECK(GattsRouteV1::controlWrite()[15] == 0x11);
    CHECK(GattsRouteV1::controlReply()[15] == 0x12);
    CHECK(GattsRouteV1::stepWrite()[15] == 0x13);
    CHECK(GattsRouteV1::stepReply()[15] == 0x14);
    CHECK(GattsRouteV1::service()[0] == 0x40);
    CHECK(!GattsRouteV1::mtuSufficient(130) && GattsRouteV1::mtuSufficient(131));
    Publisher publisher;
    IdentityAccess access(publisher);
    CHECK(access.begin(identity(1)) == IdentityAccessOutcome::Initialized);
    CHECK(access.assignGameMaster(identity(2), 7) == IdentityAccessOutcome::Assigned);
    GattsRouteOrigin origin;
    CHECK(access.connect(12, origin.connection) == IdentityAccessOutcome::Connected);
    VerifiedPeerEvidence proof; proof.peer = identity(2);
    proof.encrypted = proof.mitm = proof.secureConnections = proof.bonded = true;
    CHECK(access.authenticate(origin.connection, proof) == IdentityAccessOutcome::Verified);
    CHECK(access.route(origin.connection, origin.authority));
    origin.sdkGeneration = 9; origin.interface = 3; origin.peerAddress[0] = 7;
    Bearer bearer;
    DiagnosticGattsRoute route(access, bearer);
    std::uint8_t bytes[128] = {};
    CHECK(!route.notify(origin, 42, bytes, 88, 130));
    CHECK(!route.notify(origin, 42, bytes, 129, 131));
    CHECK(!route.notify(origin, 0, bytes, 88, 131));
    CHECK(bearer.calls == 0);
    CHECK(route.notify(origin, 42, bytes, 88, 131));
    CHECK(bearer.calls == 1);
    bearer.live = false;
    CHECK(!route.notify(origin, 42, bytes, 88, 131));
    CHECK(bearer.calls == 1);
    bearer.live = true; bearer.accept = false;
    CHECK(!route.notify(origin, 42, bytes, 88, 131));
    CHECK(bearer.calls == 2);
    bearer.accept = true;
    CHECK(access.revokeGameMaster(identity(2)) == IdentityAccessOutcome::Revoked);
    CHECK(!route.notify(origin, 42, bytes, 88, 131));
    CHECK(bearer.calls == 2);
    std::printf("%u diagnostic GATT route checks passed\n", checks);
}
