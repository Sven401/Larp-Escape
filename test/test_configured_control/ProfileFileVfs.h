#pragma once
#include "../../lib/Esp32ProfileStore/Esp32ProfileStagingStore.h"
#include "../../lib/Esp32ProfileStore/Esp32ProfileStore.h"
#include <cerrno>
#include <cstdio>
#include <esp_littlefs.h>
#include <map>
#include <string>
#include <unistd.h>
// Source-local stdio substitution, never a production switch/API. Actual adapter
// bodies run against real tmpfile streams and a deterministic named VFS model.
// Only the three fixed names can be opened; no directory/device IO is performed.
namespace host_profile_vfs {
using std::FILE;
using std::size_t;
using std::uint8_t;
struct Open {
    unsigned index;
    bool writing, error = false;
    Open(unsigned index, bool writing) : index(index), writing(writing) {}
};
std::string records[3];
bool present[3] = {};
std::map<FILE *, Open> opened;
unsigned opens = 0, writes = 0, flushes = 0, closes = 0, removes = 0, reads = 0, unknown = 0,
         journalTouches = 0, syncs = 0, mounts = 0;
bool failWriteOpen = false, failReadOpen = false, failWrite = false, failFlush = false,
     failClose = false, failRemove = false, failRead = false;
int mountResult = 0, readAdjustment = 0;
esp_vfs_littlefs_conf_t lastMount;
int index(const char *path) {
    if (std::string(path) == "/larp-profiles/profile-source.tmp")
        return 0;
    if (std::string(path) == "/larp-profiles/profile-a.larp")
        return 1;
    if (std::string(path) == "/larp-profiles/profile-b.larp")
        return 2;
    ++unknown;
    return -1;
}
void reset() {
    // Every previous fixture must close its real temporary streams first.
    if (!opened.empty())
        std::abort();
    for (unsigned i = 0; i < 3; ++i) {
        records[i].clear();
        present[i] = false;
    }
    opens = writes = flushes = closes = removes = reads = unknown = journalTouches = syncs =
        mounts = 0;
    failWriteOpen = failReadOpen = failWrite = failFlush = failClose = failRemove = failRead =
        false;
    mountResult = readAdjustment = 0;
    lastMount = {};
}
FILE *fopen(const char *path, const char *mode) {
    ++opens;
    const int at = index(path);
    if (at < 0) {
        errno = EINVAL;
        return nullptr;
    }
    if (at)
        ++journalTouches;
    const bool writing = std::string(mode) == "wb";
    if ((!writing && std::string(mode) != "rb") || (writing && failWriteOpen) ||
        (!writing && failReadOpen)) {
        errno = EIO;
        return nullptr;
    }
    if (!writing && !present[at]) {
        errno = ENOENT;
        return nullptr;
    }
    FILE *file = std::tmpfile();
    if (!file)
        return nullptr;
    if (writing) {
        records[at].clear();
        present[at] = true;
    } else {
        if (at == 0 && readAdjustment < 0 && !records[at].empty())
            records[at].pop_back();
        if (at == 0 && readAdjustment > 0)
            records[at] += '!';
        if (std::fwrite(records[at].data(), 1, records[at].size(), file) != records[at].size() ||
            std::fseek(file, 0, SEEK_SET) != 0) {
            std::fclose(file);
            return nullptr;
        }
    }
    opened.emplace(file, Open(static_cast<unsigned>(at), writing));
    return file;
}
size_t fwrite(const void *bytes, size_t size, size_t count, FILE *file) {
    ++writes;
    auto &item = opened.at(file);
    if (!item.writing) {
        item.error = true;
        return 0;
    }
    return std::fwrite(bytes, size, failWrite ? count / 2 : count, file);
}
int fflush(FILE *file) {
    ++flushes;
    const int result = std::fflush(file);
    return failFlush ? EOF : result;
}
int fclose(FILE *file) {
    ++closes;
    const auto item = opened.at(file);
    if (item.writing) {
        std::fflush(file);
        std::fseek(file, 0, SEEK_SET);
        records[item.index].clear();
        for (int value = std::fgetc(file); value != EOF; value = std::fgetc(file))
            records[item.index] += static_cast<char>(value);
    }
    opened.erase(file);
    const int result = std::fclose(file);
    return failClose ? EOF : result;
}
int ferror(FILE *file) {
    return opened.at(file).error ? 1 : std::ferror(file);
}
int fgetc(FILE *file) {
    ++reads;
    auto &item = opened.at(file);
    if (failRead) {
        item.error = true;
        return EOF;
    }
    return std::fgetc(file);
}
int remove(const char *path) {
    ++removes;
    const int at = index(path);
    if (at < 0) {
        errno = EINVAL;
        return -1;
    }
    if (at)
        ++journalTouches;
    if (failRemove) {
        errno = EIO;
        return -1;
    }
    for (const auto &item : opened)
        if (item.second.index == static_cast<unsigned>(at)) {
            errno = EACCES;
            return -1;
        }
    if (!present[at]) {
        errno = ENOENT;
        return -1;
    }
    present[at] = false;
    records[at].clear();
    return 0;
}
int syncFile(int) {
    ++syncs;
    return 0;
} // Journal durability is not measured here.
} // namespace host_profile_vfs
int esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t *configuration) {
    ++host_profile_vfs::mounts;
    host_profile_vfs::lastMount = *configuration;
    return host_profile_vfs::mountResult;
}
// Headers above are already parsed; replace only the actual source bodies' stdio
// names. Alias standard scalar/FILE types so signatures remain exactly the same.
#define std host_profile_vfs
#define fsync host_profile_vfs::syncFile
#include "../../lib/Esp32ProfileStore/Esp32ProfileStagingStore.cpp"
#include "../../lib/Esp32ProfileStore/Esp32ProfileStore.cpp"
#undef fsync
#undef std
