#pragma once
#include "DeviceRuntime/ProfileJournal.h"
#include <cstdio>
// Process-lifetime sole owner of the labelled LittleFS partition and fixed VFS
// mount. begin never formats/grows/erases it and refuses an existing mount. No
// other caller may unmount/write this filesystem while the journal is in use.
// Blocking file operations belong to the stopped application context, NOT a
// worker/network callback. stdio/LittleFS have their own heap and latency costs.
class Esp32ProfileStore : public larp::devices::IJournalStore {
  public:
    ~Esp32ProfileStore();
    Esp32ProfileStore() = default;
    Esp32ProfileStore(const Esp32ProfileStore &) = delete;
    Esp32ProfileStore &operator=(const Esp32ProfileStore &) = delete;
    bool begin();
    larp::devices::JournalOpen openRead(std::uint8_t slot) override;
    larp::configuration::ReadStatus readByte(std::uint8_t &byte) override;
    bool closeRead() override;
    bool beginWrite(std::uint8_t slot) override;
    bool write(const char *bytes, std::size_t count) override;
    larp::devices::JournalCommit finishWrite() override;
    void abortWrite() override;

  private:
    std::FILE *file_ = nullptr;
    bool mounted_ = false, writing_ = false;
};
