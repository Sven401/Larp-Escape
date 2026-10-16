#include "Pn532Spi.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <rom/ets_sys.h>
#include <cstring>

namespace nfcprobe {
namespace {
constexpr gpio_num_t Clock = GPIO_NUM_16, Miso = GPIO_NUM_17, Mosi = GPIO_NUM_23, Select = GPIO_NUM_4;
constexpr std::uint8_t Ack[] = {0, 0, 0xff, 0, 0xff, 0};
}
std::uint32_t Pn532Spi::now() const { return std::uint32_t(esp_timer_get_time() / 1000); }
void Pn532Spi::begin() {
    // Preserve documented wiring. GPIO34 is never driven; IRQ/reset are not used.
    // gpio_set_direction alone does not select the ESP32 IOMUX GPIO function.
    // gpio_config selects the mux explicitly, as Arduino pinMode also does.
    gpio_set_level(Select, 1); gpio_set_level(Clock, 0); gpio_set_level(Mosi, 0);
    gpio_config_t output = {};
    output.pin_bit_mask = (1ULL << Select) | (1ULL << Clock) | (1ULL << Mosi);
    output.mode = GPIO_MODE_OUTPUT; output.intr_type = GPIO_INTR_DISABLE;
    gpio_config_t input = {}; input.pin_bit_mask = 1ULL << Miso;
    input.mode = GPIO_MODE_INPUT; input.intr_type = GPIO_INTR_DISABLE;
    pinsReady_ = gpio_config(&output) == ESP_OK && gpio_config(&input) == ESP_OK;
    if (!pinsReady_) { error_ = "gpio_configuration"; phase_ = "gpio_setup"; return; }
    // Wake the SPI interface; no reset pin has been physically established.
    select(true); vTaskDelay(pdMS_TO_TICKS(10)); select(false); vTaskDelay(pdMS_TO_TICKS(10));
    abort();
}
void Pn532Spi::select(bool active) {
    gpio_set_level(Select, active ? 0 : 1); ets_delay_us(5);
}
std::uint8_t Pn532Spi::transfer(std::uint8_t value) {
    // PN532: SPI mode 0, least-significant bit first; deliberately slow software clock.
    std::uint8_t result = 0;
    for (unsigned bit = 0; bit < 8; ++bit) {
        gpio_set_level(Mosi, (value >> bit) & 1); ets_delay_us(2);
        gpio_set_level(Clock, 1); ets_delay_us(2);
        if (gpio_get_level(Miso)) result |= std::uint8_t(1U << bit);
        gpio_set_level(Clock, 0); ets_delay_us(2);
    }
    return result;
}
void Pn532Spi::write(const std::uint8_t *bytes, std::size_t size) {
    select(true);
    // Keep NSS asserted through oscillator startup AND the actual command.
    // A prior wake pulse followed by NSS high is insufficient in LowVbat mode.
    ets_delay_us(2000);
    transfer(1);
    for (std::size_t i = 0; i < size; ++i) transfer(bytes[i]);
    select(false);
}
void Pn532Spi::abort() {
    // Host ACK aborts a pending command. The next operation starts explicitly afresh.
    write(Ack, sizeof(Ack)); vTaskDelay(pdMS_TO_TICKS(2));
}
bool Pn532Spi::ready(std::uint32_t timeout) {
    const auto start = now();
    do {
        select(true); transfer(2); status_ = transfer(0); select(false); ++polls_;
        // UM0701-02 6.2.5 defines only RDY bit 0; RFU/unused bits are diagnostic.
        // A floating FF line still fails the subsequent complete ACK/frame validation.
        if (status_ & 1) return true;
        vTaskDelay(pdMS_TO_TICKS(2));
    } while (std::uint32_t(now() - start) < timeout);
    error_ = "spi_ready_timeout"; return false;
}
bool Pn532Spi::receive(std::uint8_t *bytes, std::size_t &size, bool ack) {
    size = 0;
    select(true); transfer(3);
    for (unsigned i = 0; i < 5; ++i) bytes[i] = transfer(0);
    std::memcpy(header_, bytes, 5); headerSize_ = 5;
    // Read exactly the declared normal-frame length, while chip select remains active.
    if (bytes[0] || bytes[1] || bytes[2] != 0xff || std::uint8_t(bytes[3] + bytes[4])) {
        // ACK's special length checksum is FF, not a normal length checksum.
        if (!(ack && bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0xff && bytes[3] == 0 && bytes[4] == 0xff)) {
            select(false); error_ = "frame_header_or_length"; return false;
        }
    }
    if (ack) {
        bytes[5] = transfer(0); header_[5] = bytes[5]; headerSize_ = 6; size = 6; select(false);
        if (std::memcmp(bytes, Ack, sizeof(Ack))) { error_ = "ack_invalid"; return false; }
        return true;
    }
    if (bytes[3] < 2 || bytes[3] == 0xff) {
        select(false); error_ = "extended_or_error_frame"; return false;
    }
    size = bytes[3] + 7;
    for (std::size_t i = 5; i < size; ++i) bytes[i] = transfer(0);
    headerSize_ = size < sizeof(header_) ? unsigned(size) : sizeof(header_);
    std::memcpy(header_, bytes, headerSize_);
    select(false); return true;
}
bool Pn532Spi::command(const std::uint8_t *request, std::size_t requestSize,
                       std::uint8_t *reply, std::size_t capacity, std::size_t &received) {
    received = 0; error_ = "none"; phase_ = "command_start";
    command_ = requestSize && request ? request[0] : 0; status_ = 0; polls_ = 0; headerSize_ = 0;
    if (!pinsReady_) { error_ = "gpio_configuration"; phase_ = "gpio_setup"; return false; }
    std::uint8_t frame[MaxFrame] = {};
    const auto size = encodeCommand(request, requestSize, frame, sizeof(frame));
    if (!size) { error_ = "command_length"; return false; }
    phase_ = "command_write"; write(frame, size);
    std::size_t readSize = 0;
    phase_ = "ack_wait"; if (!ready(1000)) { abort(); return false; }
    phase_ = "ack_read"; if (!receive(frame, readSize, true)) { abort(); return false; }
    phase_ = "response_wait"; if (!ready(2000)) { abort(); return false; }
    phase_ = "response_read"; if (!receive(frame, readSize, false)) { abort(); return false; }
    phase_ = "response_validate";
    if (!decodeFrame(frame, readSize, std::uint8_t(request[0] + 1), reply, capacity, received)) {
        error_ = "response_checksum_code_or_length"; abort(); return false;
    }
    phase_ = "complete"; return true;
}
bool Pn532Spi::initialize(std::uint8_t version[4]) {
    if (!pinsReady_) { error_ = "gpio_configuration"; phase_ = "gpio_setup"; return false; }
    abort();
    const std::uint8_t firmware[] = {2}; std::uint8_t reply[8] = {}; std::size_t size = 0;
    if (!command(firmware, sizeof(firmware), reply, sizeof(reply), size) || size != 4 || reply[0] != 0x32) return false;
    std::memcpy(version, reply, 4);
    const std::uint8_t sam[] = {0x14, 1, 0x14, 0};
    const std::uint8_t parameters[] = {0x12, 0x14}; // automatic RATS, preamble/postamble present
    const std::uint8_t retry[] = {0x32, 5, 0, 0, 0}; // one activation attempt, no infinite retries
    const std::uint8_t comRetry[] = {0x32, 4, 0}; // no automatic MIFARE write retry
    return command(sam, sizeof(sam), reply, sizeof(reply), size) && size == 0 &&
           command(parameters, sizeof(parameters), reply, sizeof(reply), size) && size == 0 &&
           command(retry, sizeof(retry), reply, sizeof(reply), size) && size == 0 &&
           command(comRetry, sizeof(comRetry), reply, sizeof(reply), size) && size == 0 && field(true);
}
bool Pn532Spi::field(bool enabled) {
    const std::uint8_t request[] = {0x32, 1, std::uint8_t(enabled ? 1 : 0)};
    std::uint8_t reply[1]; std::size_t size = 0;
    return command(request, sizeof(request), reply, sizeof(reply), size) && size == 0;
}
bool Pn532Spi::freshField() {
    if (!field(false)) return false;
    vTaskDelay(pdMS_TO_TICKS(20));
    if (!field(true)) return false;
    vTaskDelay(pdMS_TO_TICKS(20));
    return true;
}
}
