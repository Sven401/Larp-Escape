#include "CameraLabRssi.h"
#include "CameraSequence.h"
#include "EspNowTest.h"
#include <cstring>
#include <limits>

namespace larp { namespace led { namespace camera {
namespace {
bool number(const char *text, std::uint32_t &out) {
    if (!text || !*text) return false;
    std::uint32_t value = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        const unsigned digit = unsigned(*p - '0');
        if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / 10U) return false;
        value = value * 10U + digit;
    }
    out = value; return value != 0;
}
int hex(char c) {
    return c >= '0' && c <= '9' ? c - '0' :
           c >= 'a' && c <= 'f' ? c - 'a' + 10 :
           c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}
bool peer(const char *text, espnowtest::Peer &out) {
    if (!text || std::strlen(text) != 12) return false;
    for (unsigned i = 0; i < out.size(); ++i) {
        const int hi = hex(text[2*i]), lo = hex(text[2*i+1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<std::uint8_t>(hi * 16 + lo);
    }
    return espnowtest::validPeer(out);
}
}
void RssiController::begin(const char *token) {
    std::strcpy(token_, token); lastSample_ = pendingSample_ = 0;
    pending_ = false; active_ = true;
}
void RssiController::command(const char *line, std::uint32_t now) {
    char buffer[96] = {};
    if (!line || std::strlen(line) >= sizeof(buffer)) {
        observer_.rejected(token_, 0, "syntax", now); return;
    }
    std::strcpy(buffer, line);
    char *context = nullptr;
    const char *verb = strtok_r(buffer, " ", &context);
    const char *token = strtok_r(nullptr, " ", &context);
    const char *id = strtok_r(nullptr, " ", &context);
    const char *destination = strtok_r(nullptr, " ", &context);
    const char *extra = strtok_r(nullptr, " ", &context);
    std::uint32_t sample = 0;
    const bool tokenOk = token && validToken(token);
    const bool sampleOk = number(id, sample);
    const char *replyToken = tokenOk ? token : token_;
    espnowtest::Peer destinationPeer{};
    const char *reason = nullptr;
    if (!verb || std::strcmp(verb, "RSSI") || !tokenOk || !id || !destination || extra) reason = "syntax";
    else if (!sampleOk) reason = "sample-id";
    else if (!peer(destination, destinationPeer)) reason = "peer";
    else if (!active_) reason = "inactive";
    else if (std::strcmp(token, token_)) reason = "token";
    else if (sample <= lastSample_) reason = "stale";
    else if (pending_) reason = "busy";
    else if (!radio_.submit(destinationPeer, now)) reason = "radio-unavailable";
    if (reason) { observer_.rejected(replyToken, sample, reason, now); return; }
    // Admission includes a local send failure: that gets one terminal result.
    lastSample_ = pendingSample_ = sample; pending_ = true;
    observer_.measuring(token_, sample, now);
}
RssiResult RssiController::result(std::uint32_t now) {
    RssiResult out; std::strcpy(out.token, token_);
    out.sample = pendingSample_; out.now = now; out.radio = radio_.snapshot();
    return out;
}
void RssiController::service(std::uint32_t now) {
    radio_.service(now);
    if (!pending_ || radio_.snapshot().pending) return;
    const auto out = result(now); pending_ = false;
    observer_.measured(out);
}
void RssiController::end(std::uint32_t now) {
    if (pending_) {
        auto out = result(now); pending_ = false;
        out.radio.outcome = 14; out.radio.pending = false;
        out.radio.received = out.radio.rssiValid = false;
        out.radio.rssi = out.radio.filteredRssi = 0;
        out.radio.receiverBoot = out.radio.receiverCount = 0;
        observer_.measured(out);
    }
    active_ = false;
    radio_.cancel();
}
}}}
