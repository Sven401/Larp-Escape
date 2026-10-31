#include "Esp32ProgressPartition.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace larp::devices;
namespace {
std::array<std::uint8_t, ProgressLimits::StorageBytes> medium;
esp_partition_t partition;
bool found = true, readError = false, writeError = false, wrongReadback = false;
std::size_t writes = 0, reads = 0, checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                   \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
void reset() {
    medium.fill(0xff);
    partition = {};
    partition.type = ESP_PARTITION_TYPE_DATA;
    partition.subtype = ESP_PARTITION_SUBTYPE_DATA_UNDEFINED;
    partition.address = 0x100000;
    partition.size = ProgressLimits::StorageBytes;
    partition.erase_size = ProgressLimits::SlotBytes;
    std::strcpy(partition.label, "larp_progress");
    found = true;
    readError = writeError = wrongReadback = false;
    reads = writes = 0;
}
void rejectedGeometry() {
    for (unsigned mode = 0; mode < 8; ++mode) {
        reset();
        switch (mode) {
        case 0:
            found = false;
            break;
        case 1:
            --partition.size;
            break;
        case 2:
            ++partition.size;
            break;
        case 3:
            ++partition.address;
            break;
        case 4:
            partition.encrypted = true;
            break;
        case 5:
            partition.readonly = true;
            break;
        case 6:
            partition.erase_size = 8192;
            break;
        case 7:
            std::strcpy(partition.label, "spiffs");
            break;
        }
        Esp32ProgressPartition store;
        CHECK(!store.begin());
        CHECK(store.capacity() == 0);
        CHECK(!store.sync());
        CHECK(!store.begin());
        CHECK(reads == 0 && writes == 0);
    }
}
void boundedIoAndPoison() {
    reset();
    Esp32ProgressPartition store;
    std::uint8_t bytes[4] = {1, 2, 3, 4}, readback[4] = {};
    CHECK(!store.program(0, bytes, sizeof(bytes)));
    CHECK(store.begin());
    CHECK(!store.begin());
    CHECK(store.capacity() == ProgressLimits::StorageBytes);
    CHECK(!store.read(0, nullptr, 4));
    CHECK(!store.program(std::numeric_limits<std::size_t>::max(), bytes, 4));
    CHECK(!store.program(ProgressLimits::StorageBytes - 3, bytes, 4));
    CHECK(!store.program(0, bytes, 0));
    CHECK(writes == 0);
    CHECK(store.program(ProgressLimits::StorageBytes - 4, bytes, 4));
    CHECK(store.sync());
    CHECK(store.read(ProgressLimits::StorageBytes - 4, readback, 4));
    CHECK(std::memcmp(bytes, readback, 4) == 0);
    CHECK(!store.program(ProgressLimits::StorageBytes - 4, bytes, 4));
    CHECK(writes == 1);
    CHECK(store.capacity() == 0 && !store.sync());
    CHECK(!store.program(0, bytes, 4));
    for (unsigned mode = 0; mode < 3; ++mode) {
        reset();
        Esp32ProgressPartition fault;
        CHECK(fault.begin());
        readError = mode == 0;
        writeError = mode == 1;
        wrongReadback = mode == 2;
        CHECK(!fault.program(0, bytes, 4));
        readError = writeError = wrongReadback = false;
        CHECK(fault.capacity() == 0 && !fault.sync());
        CHECK(!fault.read(0, readback, 4));
    }
}
struct Fence : IProgressCommitFence {
    bool allow(const ProgressRecord &) override {
        return true;
    }
};
void journalRoundtrip() {
    reset();
    Esp32ProgressPartition store;
    CHECK(store.begin());
    ProgressOrigin origin;
    origin.event[0] = 1;
    origin.box[0] = 2;
    origin.epoch[0] = 3;
    ProgressJournal journal(store);
    CHECK(journal.initialize(origin).outcome == ProgressOutcome::Initialized);
    const std::uint8_t body[] = {5, 6, 7};
    ProgressRequest request;
    request.operationId[0] = 1;
    request.requestDigest[0] = 2;
    request.kind = ProgressKind::Application;
    request.issueCount = 2;
    request.payload = body;
    request.payloadBytes = sizeof(body);
    Fence fence;
    CHECK(journal.commit(request, fence).outcome == ProgressOutcome::Committed);
    auto second = request;
    second.operationId[0] = 3;
    second.expectedRevision = 1;
    second.expectedLastIssued = 2;
    CHECK(journal.commit(second, fence).outcome == ProgressOutcome::Committed);
    const auto programmed = writes;
    Esp32ProgressPartition restarted;
    CHECK(restarted.begin());
    ProgressJournal recovered(restarted);
    CHECK(recovered.recover(origin).outcome == ProgressOutcome::Recovered);
    CHECK(recovered.snapshot().lastIssued == 4);
    CHECK(recovered.commit(request, fence).outcome == ProgressOutcome::Duplicate);
    CHECK(writes == programmed);
    std::uint8_t payload[8] = {};
    ProgressRecord record;
    CHECK(recovered.readRecord(0, record, payload, sizeof(payload)).outcome ==
          ProgressOutcome::Read);
    CHECK(record.firstIssued == 1 && record.lastIssued == 2);
    CHECK(std::memcmp(payload, body, sizeof(body)) == 0);
    const auto savedMedium = medium;
    auto wrongOrigin = origin;
    wrongOrigin.epoch[0] = 9;
    CHECK(recovered.recover(wrongOrigin).outcome == ProgressOutcome::WrongOrigin);
    // A failed open must not discard an already confirmed watermark.
    medium.fill(0xff);
    CHECK(recovered.recover(origin).outcome == ProgressOutcome::Corrupt);
    CHECK(recovered.initialize(origin).outcome == ProgressOutcome::Corrupt);
    CHECK(recovered.commit(request, fence).outcome == ProgressOutcome::Corrupt);
    CHECK(writes == programmed);
    medium = savedMedium;
    Esp32ProgressPartition existingStore;
    CHECK(existingStore.begin());
    ProgressJournal existing(existingStore);
    CHECK(existing.initialize(origin).outcome == ProgressOutcome::Conflict);
    medium.fill(0xff);
    CHECK(existing.recover(origin).outcome == ProgressOutcome::Corrupt);
    CHECK(existing.initialize(origin).outcome == ProgressOutcome::Corrupt);
    CHECK(writes == programmed);
}
} // namespace
const esp_partition_t *esp_partition_find_first(esp_partition_type_t type,
                                                esp_partition_subtype_t subtype,
                                                const char *label) {
    CHECK(type == ESP_PARTITION_TYPE_DATA && subtype == ESP_PARTITION_SUBTYPE_DATA_UNDEFINED);
    CHECK(std::strcmp(label, "larp_progress") == 0);
    return found ? &partition : nullptr;
}
esp_err_t esp_partition_read(const esp_partition_t *p, std::size_t offset, void *bytes,
                             std::size_t count) {
    ++reads;
    CHECK(p == &partition && offset <= medium.size() && count <= medium.size() - offset);
    if (readError)
        return -1;
    std::memcpy(bytes, medium.data() + offset, count);
    if (wrongReadback && writes)
        static_cast<std::uint8_t *>(bytes)[0] ^= 1;
    return ESP_OK;
}
esp_err_t esp_partition_write(const esp_partition_t *p, std::size_t offset, const void *bytes,
                              std::size_t count) {
    ++writes;
    CHECK(p == &partition && offset <= medium.size() && count <= medium.size() - offset);
    const auto *input = static_cast<const std::uint8_t *>(bytes);
    const auto end = writeError ? count / 2 : count;
    for (std::size_t i = 0; i < end; ++i)
        medium[offset + i] &= input[i];
    return writeError ? -1 : ESP_OK;
}
int main() {
    rejectedGeometry();
    boundedIoAndPoison();
    journalRoundtrip();
    std::printf("progress partition: %u assertions passed (synthetic SDK storage)\n",
                static_cast<unsigned>(checks));
}
