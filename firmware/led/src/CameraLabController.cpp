#include "CameraLabController.h"
#include "CameraSequence.h"
#include <cstring>
#include <cstdlib>
#include <limits>
namespace larp { namespace led { namespace camera {
namespace {
bool number(const char *text, std::uint32_t &out) {
    if (!text || !*text) return false;
    std::uint32_t value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        const unsigned digit = unsigned(*text - '0');
        if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / 10U) return false;
        value = value * 10U + digit;
    }
    out = value; return true;
}
}
bool LiveController::begin(const char *id) {
    if (!id || !validToken(id)) return false;
    std::strcpy(token_, id); scene_ = near::Scene{}; lastRequest_ = 0; last_ = Request{}; return true;
}
bool LiveController::same(const Request &a, const Request &b) {
    if (a.background != b.background) return false;
    if (a.background) return a.pixel.red == b.pixel.red && a.pixel.green == b.pixel.green && a.pixel.blue == b.pixel.blue;
    return a.glow.slot == b.glow.slot && a.glow.generation == b.glow.generation &&
        a.glow.sequence == b.glow.sequence && a.glow.operation == b.glow.operation &&
        near::equal(a.glow.parameters, b.glow.parameters);
}
LiveReply LiveController::command(const char *line, std::uint32_t now) {
    LiveReply reply;
    if (!line || std::strlen(line) >= 192) return reply;
    char copy[192]; std::strcpy(copy, line);
    char *words[17] = {}; unsigned count = 0; char *context = nullptr;
    for (char *word = strtok_r(copy, " ", &context); word; word = strtok_r(nullptr, " ", &context)) {
        if (count == 17) return reply;
        words[count++] = word;
    }
    if (count < 2 || std::strcmp(words[1], token_) != 0) { reply.reason = "token"; return reply; }
    if (std::strcmp(words[0], "STATUS") == 0 && count == 2) { reply.kind = "STATUS"; reply.reason = "status"; return reply; }
    if (count < 3 || !number(words[2], reply.request) || !reply.request) return reply;
    Request parsed;
    if (std::strcmp(words[0], "BG") == 0) {
        if (count != 6) return reply;
        std::uint32_t rgb[3];
        for (unsigned c = 0; c < 3; ++c) if (!number(words[c+3], rgb[c]) || rgb[c] > 32) { reply.reason = "parameters"; return reply; }
        parsed.background = true; parsed.pixel.red = std::uint8_t(rgb[0]); parsed.pixel.green = std::uint8_t(rgb[1]); parsed.pixel.blue = std::uint8_t(rgb[2]);
    } else if (std::strcmp(words[0], "GLOW") == 0) {
        if (count != 14) return reply;
        std::uint32_t slot, generation, values[8];
        if (!number(words[3], slot) || slot >= near::Slots || !number(words[4], generation) || !generation) { reply.reason = "parameters"; return reply; }
        for (unsigned n = 0; n < 8; ++n) if (!number(words[n+6], values[n])) { reply.reason = "parameters"; return reply; }
        if (values[0] > 1000 || values[1] > 1 || values[2] > 32 || values[3] > 32 || values[4] > 32 || values[5] > 1000 || values[6] < 50 || values[6] > 5000 || values[7] > 1) { reply.reason = "parameters"; return reply; }
        auto &c = parsed.glow; c.slot = slot; c.generation = generation; c.sequence = reply.request;
        if (std::strcmp(words[5], "start") == 0) c.operation = near::Operation::Start;
        else if (std::strcmp(words[5], "update") == 0) c.operation = near::Operation::Update;
        else if (std::strcmp(words[5], "stop") == 0) c.operation = near::Operation::Stop;
        else return reply;
        c.parameters.level = std::uint16_t(values[0]); c.parameters.valid = values[1] != 0;
        c.parameters.colour.red = std::uint8_t(values[2]); c.parameters.colour.green = std::uint8_t(values[3]); c.parameters.colour.blue = std::uint8_t(values[4]);
        c.parameters.gain = std::uint16_t(values[5]); c.parameters.duration = std::uint16_t(values[6]); c.parameters.curve = std::uint8_t(values[7]);
    } else return reply;
    if (reply.request < lastRequest_) { reply.reason = "stale"; return reply; }
    if (reply.request == lastRequest_) {
        if (same(parsed, last_)) { reply.kind = "DUPLICATE"; reply.reason = "duplicate"; }
        else reply.reason = "conflict";
        return reply;
    }
    scene_.service(now);
    const char *result = parsed.background ? (scene_.background(parsed.pixel) ? "applied" : "parameters") : scene_.apply(parsed.glow, now);
    if (std::strcmp(result, "applied") != 0) { reply.reason = result; return reply; }
    lastRequest_ = reply.request; last_ = parsed;
    reply.kind = "APPLIED"; reply.reason = "applied"; return reply;
}
}}}
