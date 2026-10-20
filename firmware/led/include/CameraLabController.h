#pragma once
#include "NearGlow.h"
#include "CrystalSeal.h"

namespace larp { namespace led { namespace camera {
struct LiveReply {
    const char *kind = "REJECT";
    const char *reason = "syntax";
    std::uint32_t request = 0;
};
// Isolated laboratory protocol only; never included by the product BLE owner.
class LiveController {
public:
    bool begin(const char *token);
    LiveReply command(const char *line, std::uint32_t now);
    near::Scene &scene() { return scene_; }
    const near::Scene &scene() const { return scene_; }
    const seal::Scene &sealScene() const { return seal_; }
    std::uint32_t revision() const { return revision_; }
    void service(std::uint32_t now) { scene_.service(now); seal_.service(now); }
    Frame render(std::uint32_t now);
    const char *token() const { return token_; }
    void hardStop();
private:
    struct Request {
        enum class Kind { Glow, Background, Seal } kind = Kind::Glow;
        near::Command glow;
        seal::Command crystal;
        Pixel pixel;
    };
    static bool same(const Request &, const Request &);
    char token_[17] = {};
    near::Scene scene_;
    seal::Scene seal_;
    std::uint32_t revision_ = 1;
    std::uint32_t lastRequest_ = 0;
    Request last_;
};
}}}
