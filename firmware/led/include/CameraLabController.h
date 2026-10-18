#pragma once
#include "NearGlow.h"

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
    const char *token() const { return token_; }
    void hardStop() { scene_.reset(); }
private:
    struct Request {
        bool background = false;
        near::Command glow;
        Pixel pixel;
    };
    static bool same(const Request &, const Request &);
    char token_[17] = {};
    near::Scene scene_;
    std::uint32_t lastRequest_ = 0;
    Request last_;
};
}}}
