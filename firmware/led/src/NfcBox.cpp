#ifdef LARP_NFC_TEST
#include "NfcBox.h"
#include "Pn532Spi.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include <cstring>

namespace larp { namespace nfcble { namespace {
struct Job { std::uint32_t epoch; bool change; nfcprobe::Target expected; };
struct Done { std::uint32_t epoch; Reply reply; };
QueueHandle_t jobs = nullptr, results = nullptr;
std::atomic<std::uint32_t> epoch{1};
std::atomic<bool> working{false};
bool allowed = false;
// Cancellation is checked before EVERY PN532 command, including every page write.
// Once a write has started, failure remains Unknown in the existing transaction core.
class Channel final : public nfcprobe::Channel {
 public:
    nfcprobe::Pn532Spi reader;
    std::uint32_t owner = 0;
    bool command(const std::uint8_t *p, std::size_t n, std::uint8_t *r,
                 std::size_t capacity, std::size_t &size) override {
        if (owner != epoch.load()) { size = 0; return false; }
        return reader.command(p, n, r, capacity, size);
    }
    std::uint32_t now() const override { return reader.now(); }
} channel;
nfcprobe::Session transaction(channel);
nfcprobe::Snapshot snapshot;
void worker(void *) {
    channel.reader.begin();
    Job job{};
    while (true) {
        if (xQueueReceive(jobs, &job, portMAX_DELAY) != pdTRUE) continue;
        Done done{}; done.epoch = job.epoch; channel.owner = job.epoch;
        std::uint8_t version[4]{};
        auto status = nfcprobe::Result::Error;
        snapshot = {};
        if (job.epoch == epoch.load() && channel.reader.initialize(version) &&
            job.epoch == epoch.load() && channel.reader.freshField()) {
            status = job.change ? transaction.change(snapshot, &job.expected) : transaction.read(snapshot);
        }
        // Reader is exclusively owned here. Always park it before delivering a result.
        const bool off = channel.reader.field(false);
        if (!off && status == nfcprobe::Result::Verified) status = nfcprobe::Result::Unknown;
        if (!off && status == nfcprobe::Result::Ok) status = nfcprobe::Result::Error;
        auto &r = done.reply;
        r.outcome = static_cast<std::uint8_t>(status);
        r.kind = static_cast<std::uint8_t>(snapshot.kind);
        r.uidSize = snapshot.target.uidSize;
        std::memcpy(r.uid, snapshot.target.uid, r.uidSize);
        std::size_t offset = 0; std::uint16_t flags = 0;
        if ((status == nfcprobe::Result::Ok || status == nfcprobe::Result::Verified) &&
            snapshot.offset <= snapshot.rawSize && snapshot.size <= snapshot.rawSize - snapshot.offset &&
            nfcprobe::gamePayload(snapshot.bytes + snapshot.offset, snapshot.size, offset, flags)) {
            r.payloadSize = 25;
            std::memcpy(r.payload, snapshot.bytes + snapshot.offset + offset, 25);
        }
        // No new command is admitted until the foreground loop consumes this result.
        xQueueOverwrite(results, &done);
    }
}
class Box final : public Port {
 public:
    bool submit(bool change, const std::uint8_t *uid, std::uint8_t size) override {
        if (!allowed || !jobs || !results || working.exchange(true)) return false;
        Job job{}; job.epoch = epoch.load(); job.change = change; job.expected.uidSize = size;
        std::memcpy(job.expected.uid, uid, size);
        if (xQueueSend(jobs, &job, 0) == pdTRUE) return true;
        working.store(false); return false;
    }
    bool poll(Reply &reply) override {
        Done done{};
        if (!results || xQueueReceive(results, &done, 0) != pdTRUE) return false;
        working.store(false);
        if (done.epoch != epoch.load()) return false;
        reply = done.reply; return true;
    }
    void cancel() override { epoch.fetch_add(1); }
} instance;
} // namespace
bool begin() {
    jobs = xQueueCreate(1, sizeof(Job)); results = xQueueCreate(1, sizeof(Done));
    return jobs && results && xTaskCreate(worker, "nfc-owner", 8192, nullptr, 1, nullptr) == pdPASS;
}
Port &box() { return instance; }
bool busy() { return working.load(); }
void allow(bool value) { allowed = value; }
} }
#endif
