#pragma once
#include "NfcProbe.h"

namespace nfcprobe {
class Pn532Spi final : public Channel {
 public:
    void begin();
    bool initialize(std::uint8_t version[4]);
    bool field(bool enabled);
    bool freshField();
    bool command(const std::uint8_t *command, std::size_t size,
                 std::uint8_t *reply, std::size_t capacity, std::size_t &received) override;
    std::uint32_t now() const override;
    const char *error() const { return error_; }
    const char *phase() const { return phase_; }
    std::uint8_t lastCommand() const { return command_; }
    std::uint8_t lastStatus() const { return status_; }
    unsigned statusPolls() const { return polls_; }
    const std::uint8_t *header() const { return header_; }
    unsigned headerSize() const { return headerSize_; }
 private:
    void select(bool active);
    std::uint8_t transfer(std::uint8_t byte);
    bool ready(std::uint32_t timeout);
    void write(const std::uint8_t *bytes, std::size_t size);
    bool receive(std::uint8_t *bytes, std::size_t &size, bool ack);
    void abort();
    const char *error_ = "none";
    const char *phase_ = "not_started";
    std::uint8_t command_ = 0, status_ = 0, header_[8] = {};
    unsigned polls_ = 0, headerSize_ = 0;
    bool pinsReady_ = false;
};
}
