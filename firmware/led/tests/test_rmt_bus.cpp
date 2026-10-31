#include "LedOutput.h"
#include <driver/rmt.h>
#include <esp_intr_alloc.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace larp::led;
namespace larp { namespace led { LedBus &cameraLabBus(); }}
namespace {
std::uint32_t currentTime = 0, frequency = 40000000;
rmt_config_t configured{};
unsigned installs = 0, uninstalls = 0, stops = 0;
int interruptFlags = 0;
esp_err_t writeResult = ESP_OK, waitResult = ESP_ERR_TIMEOUT;
std::vector<rmt_item32_t> sent;
const rmt_item32_t *retained = nullptr;
void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
std::uint8_t decode(std::size_t byte) {
    std::uint8_t value = 0;
    for (unsigned bit = 0; bit < 8; ++bit) {
        const auto &item = sent.at(byte * 8 + bit);
        check(item.level0 == 1 && item.level1 == 0, "bit signal polarity changed");
        check((item.duration0 == 10 && item.duration1 == 40) ||
              (item.duration0 == 24 && item.duration1 == 26), "proven 800 kHz bit timing changed");
        value = static_cast<std::uint8_t>((value << 1) | (item.duration0 == 24));
    }
    return value;
}
void finish(LedOutput &output) { waitResult = ESP_OK; output.service(); waitResult = ESP_ERR_TIMEOUT; }
void boot(LedOutput &output) { check(output.begin(), "begin failed"); output.service(); finish(output); }
void wire_and_completion() {
    LedOutput output;
    check(output.begin(), "begin failed");
    check(configured.gpio_num == GPIO_NUM_15 && configured.channel == RMT_CHANNEL_0 && configured.clk_div == 2 &&
          configured.mem_block_num == 1, "fixed hardware configuration changed");
    check(interruptFlags == ESP_INTR_FLAG_LOWMED && installs == 1, "RMT must use SDK low/medium interrupts");
    check(!configured.tx_config.loop_en && !configured.tx_config.carrier_en &&
          configured.tx_config.idle_output_en && configured.tx_config.idle_level == RMT_IDLE_LEVEL_LOW, "unsafe idle configuration");
    output.service();
    check(sent.size() == 23 * 24 + 1, "wrong full-strip frame size");
    for (std::size_t i = 0; i < 23 * 3; ++i) check(decode(i) == 0, "boot frame not black");
    const auto &latch = sent.back();
    check(latch.level0 == 0 && latch.level1 == 0 && latch.duration0 == 6000 && latch.duration1 == 6000,
          "300 us low latch missing");
    output.service(); check(!output.snapshot().ready && output.snapshot().pending, "write acceptance fabricated ready");
    finish(output); check(output.snapshot().ready && output.snapshot().off, "TX_DONE did not confirm boot black");
    check(output.start(Effect::Ripple), "ripple start rejected"); output.service();
    check(decode(11 * 3) == 8 && decode(11 * 3 + 1) == 8 && decode(11 * 3 + 2) == 32, "center GRB serialization wrong");
    output.service(); check(output.snapshot().pending, "busy RMT acknowledged start");
    finish(output); check(!output.snapshot().pending, "completed RMT did not acknowledge start");
}
void bad_counter() {
    frequency = 20000000;
    LedOutput output; check(!output.begin(), "wrong RMT clock accepted");
    check(output.snapshot().fault && uninstalls == 1, "bad counter did not release driver and fail closed");
}
void write_failure() {
    LedOutput output; boot(output);
    writeResult = ESP_FAIL; check(output.start(Effect::Twinkle), "start failed"); output.service();
    check(output.snapshot().fault && output.snapshot().pending, "write error acknowledged");
    writeResult = ESP_OK; output.service();
    check(stops == 1 && uninstalls == 1 && installs == 2, "write failure did not reset SDK driver");
    for (std::size_t i = 0; i < 23 * 3; ++i) check(decode(i) == 0, "recovery frame not black");
    finish(output); check(output.snapshot().fault && output.snapshot().off && !output.snapshot().ready, "recovery lost sticky fault");
}
void wait_failure() {
    LedOutput output; boot(output); check(output.start(Effect::Twinkle), "start failed"); output.service();
    waitResult = ESP_FAIL; output.service();
    check(output.snapshot().fault && output.snapshot().appliedRevision == 1, "failed TX_DONE fabricated application");
    waitResult = ESP_ERR_TIMEOUT; output.service(); finish(output);
    check(stops == 1 && output.snapshot().off && output.snapshot().fault, "completion failure did not recover to black");
}
#ifdef LARP_S3_LED_TEST
void board_rgb() {
    auto &bus = cameraLabBus();
    check(bus.begin(), "board bus begin failed");
    check(static_cast<unsigned>(configured.gpio_num) == 21, "Waveshare board LED pin incorrect");
    Frame frame{};
    frame[3].red = 12; frame[9].green = 7; frame[22].blue = 31;
    check(bus.submit(frame), "board frame rejected");
    check(sent.size() == 25, "board must transmit exactly one pixel and latch");
    check(decode(0) == 12 && decode(1) == 7 && decode(2) == 31, "board RGB order/peak projection incorrect");
    check(sent.back().duration0 == 6000 && sent.back().duration1 == 6000, "board latch missing");
    check(bus.poll() == TxResult::Busy, "board fabricated completion");
    waitResult = ESP_OK; check(bus.poll() == TxResult::Complete, "board completion missing");
    frame[22].red = 33;
    check(!bus.submit(frame), "board exceeded brightness bound");
}
#endif
}
unsigned long millis() { return currentTime; }
esp_err_t rmt_config(const rmt_config_t *config) { configured = *config; return ESP_OK; }
esp_err_t rmt_driver_install(rmt_channel_t channel, std::size_t receiveBuffer, int flags) {
    check(channel == RMT_CHANNEL_0 && receiveBuffer == 0, "unexpected channel/RX allocation");
    ++installs; interruptFlags = flags; return ESP_OK;
}
esp_err_t rmt_get_counter_clock(rmt_channel_t, std::uint32_t *value) { *value = frequency; return ESP_OK; }
esp_err_t rmt_driver_uninstall(rmt_channel_t) { ++uninstalls; return ESP_OK; }
esp_err_t rmt_write_items(rmt_channel_t channel, const rmt_item32_t *items, int count, bool wait) {
    check(channel == RMT_CHANNEL_0 && !wait, "RMT submission blocks the command owner");
    if (writeResult == ESP_OK) { sent.assign(items, items + count); retained = items; }
    return writeResult;
}
esp_err_t rmt_wait_tx_done(rmt_channel_t channel, unsigned timeout) {
    check(channel == RMT_CHANNEL_0 && timeout == 0, "RMT completion poll blocks the command owner");
    for (std::size_t i = 0; i < sent.size(); ++i)
        check(retained[i].duration0 == sent[i].duration0 && retained[i].duration1 == sent[i].duration1 &&
              retained[i].level0 == sent[i].level0 && retained[i].level1 == sent[i].level1, "DMA frame storage changed before completion");
    return waitResult;
}
esp_err_t rmt_tx_stop(rmt_channel_t) { ++stops; return ESP_OK; }
int main(int argc, char **argv) {
    if (argc != 2) return EXIT_FAILURE;
    try {
        const std::string name = argv[1];
        if (name == "wire_and_completion") wire_and_completion();
        else if (name == "bad_counter") bad_counter();
        else if (name == "write_failure") write_failure();
        else if (name == "wait_failure") wait_failure();
#ifdef LARP_S3_LED_TEST
        else if (name == "board_rgb") board_rgb();
#endif
        else return EXIT_FAILURE;
        std::cout << name << " passed\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
}
