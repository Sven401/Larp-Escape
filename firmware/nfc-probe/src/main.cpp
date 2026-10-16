#include "NfcProbe.h"
#include "Pn532Spi.h"
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
nfcprobe::Pn532Spi reader;
nfcprobe::Session session(reader);
nfcprobe::Snapshot snapshot;
bool initialized = false;
std::uint8_t version[4] = {};
void line(const char *format, ...) {
    char buffer[192]; va_list args; va_start(args, format);
    const int length = vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
    if (length > 0) uart_write_bytes(UART_NUM_0, buffer, std::size_t(length) < sizeof(buffer) ? length : sizeof(buffer) - 1);
    uart_write_bytes(UART_NUM_0, "\r\n", 2);
}
void hex(const char *label, const std::uint8_t *bytes, std::size_t size) {
    char value[129]; const char digits[] = "0123456789ABCDEF";
    for (std::size_t offset = 0; offset < size; offset += 64) {
        const std::size_t count = size - offset > 64 ? 64 : size - offset;
        for (std::size_t i = 0; i < count; ++i) {
            value[2 * i] = digits[bytes[offset + i] >> 4]; value[2 * i + 1] = digits[bytes[offset + i] & 15];
        }
        value[2 * count] = 0; line("NFC %s offset=%u hex=%s", label, unsigned(offset), value);
    }
}
void report(const nfcprobe::Snapshot &s, bool complete) {
    line("NFC TAG type=%s uid_bytes=%u atqa=%02X%02X sak=%02X writable=%u capacity=%u ndef_bytes=%u",
         nfcprobe::name(s.kind), s.target.uidSize, s.target.atqa[0], s.target.atqa[1], s.target.sak,
         s.writable ? 1 : 0, s.capacity, s.size);
    hex("UID", s.target.uid, s.target.uidSize);
    if (complete && s.size && s.offset <= s.rawSize && s.size <= s.rawSize - s.offset) {
        hex("NDEF", s.bytes + s.offset, s.size);
        std::size_t offset = 0; std::uint16_t flags = 0;
        const bool test = nfcprobe::gamePayload(s.bytes + s.offset, s.size, offset, flags);
        line("NFC DATA test_record=%u flags=%u prepared=%u crc_checked=%u", test ? 1 : 0, flags,
             test && flags == 1 ? 1 : 0, test ? 1 : 0);
    }
}
void diagnostics() {
    line("NFC SPI command=%02X phase=%s status=%02X polls=%u", reader.lastCommand(), reader.phase(),
         reader.lastStatus(), reader.statusPolls());
    if (reader.headerSize()) hex("HEADER", reader.header(), reader.headerSize());
}
void execute(const char *command) {
    if (std::strcmp(command, "INFO") && std::strcmp(command, "READ") && std::strcmp(command, "CHANGE") && std::strcmp(command, "OFF")) {
        line("NFC END COMMAND ERROR"); return;
    }
    line("NFC BEGIN %s", command);
    if (!std::strcmp(command, "OFF")) {
        const bool ok = reader.field(false); initialized = false;
        line("NFC DETAIL reader=%s", reader.error());
        diagnostics();
        line("NFC END OFF %s", ok ? "OK" : "ERROR"); return;
    }
    if (!initialized || !std::strcmp(command, "INFO")) initialized = reader.initialize(version);
    if (!initialized) {
        line("NFC DETAIL reader=%s", reader.error()); diagnostics(); line("NFC END %s ERROR", command); return;
    }
    if (!std::strcmp(command, "INFO")) {
        line("NFC INFO build=nfc-probe-v1 chip=%02X firmware=%u.%u support=%02X pins=16,17,23,4 nvs=unused",
             version[0], version[1], version[2], version[3]);
        diagnostics();
        line("NFC END INFO OK"); return;
    }
    // One new activation per explicit operation, even if the same tag stays in place.
    // Keep RF continuous throughout both preimage reads, mutation and verification.
    if (!reader.freshField()) {
        initialized = false; line("NFC DETAIL reader=%s", reader.error()); diagnostics();
        line("NFC END %s ERROR", command); return;
    }
    const auto result = !std::strcmp(command, "READ") ? session.read(snapshot) : session.change(snapshot);
    if (snapshot.target.uidSize) report(snapshot, result == nfcprobe::Result::Ok || result == nfcprobe::Result::Verified);
    line("NFC DETAIL session=%s reader=%s", session.detail(), reader.error());
    diagnostics();
    line("NFC END %s %s", command, nfcprobe::name(result));
    // Reconfigure after a failure, only on the next explicit command. Never retry a write.
    if (result == nfcprobe::Result::Error || result == nfcprobe::Result::Unknown) initialized = false;
}
}
// Own entry point intentionally bypasses initArduino and its NVS auto-repair.
// This image neither initializes nor writes/erases NVS, Wi-Fi, BLE or the key partition.
extern "C" void app_main() {
    uart_config_t config = {};
    config.baud_rate = 115200; config.data_bits = UART_DATA_8_BITS; config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1; config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_APB;
    if (uart_param_config(UART_NUM_0, &config) != ESP_OK ||
        uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0) != ESP_OK) return;
    reader.begin();
    line("NFC READY nfc-probe-v1 commands=INFO,READ,CHANGE,OFF auto_write=0");
    char command[32] = {}; std::size_t used = 0; bool overflow = false;
    while (true) {
        std::uint8_t byte = 0;
        if (uart_read_bytes(UART_NUM_0, &byte, 1, pdMS_TO_TICKS(20)) != 1) continue;
        if (byte == '\r') continue;
        if (byte == '\n') {
            command[used] = 0;
            if (overflow) line("NFC END COMMAND ERROR");
            else if (used) execute(command);
            used = 0; overflow = false;
        } else if (byte < 32 || byte > 126 || used >= sizeof(command) - 1) overflow = true;
        else command[used++] = char(byte);
    }
}
