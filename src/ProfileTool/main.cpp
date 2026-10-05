// Host-side profile export/inspection. This is not an embedded entry point.
#include "Activation.h"
#include "GameConfiguration/ProfileReader.h"
#include "GameConfiguration/ProfileWriter.h"
#include "Play.h"
#include "Preview.h"
#include <cstdio>
#include <cstring>
#include <memory>

namespace {
class FileWriter : public larp::configuration::IProfileWriter {
  public:
    explicit FileWriter(std::FILE *file) : file_(file) {}
    bool write(const char *bytes, std::size_t count) override {
        return std::fwrite(bytes, 1, count, file_) == count;
    }

  private:
    std::FILE *file_;
};
class FileReader : public larp::configuration::IProfileReader {
  public:
    explicit FileReader(std::FILE *file) : file_(file) {}
    larp::configuration::ReadStatus readByte(std::uint8_t &byte) override {
        const int value = std::fgetc(file_);
        if (value == EOF)
            return std::ferror(file_) ? larp::configuration::ReadStatus::Failure
                                      : larp::configuration::ReadStatus::End;
        byte = static_cast<std::uint8_t>(value);
        return larp::configuration::ReadStatus::Byte;
    }

  private:
    std::FILE *file_;
};
} // namespace
int main(int argc, char **argv) {
    using namespace larp::configuration;
    bool summary = false;
    bool example = false;
    const char *inputPath = nullptr;
    const char *previewPath = nullptr;
    const char *activationPath = nullptr;
    const char *exportActivePath = nullptr;
    bool playable = false, queued = false;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--summary") == 0 && !summary)
            summary = true;
        else if (std::strcmp(argv[index], "--example") == 0 && !example && inputPath == nullptr)
            example = true;
        else if (std::strcmp(argv[index], "--import") == 0 && inputPath == nullptr && !example &&
                 index + 1 < argc)
            inputPath = argv[++index];
        else if (std::strcmp(argv[index], "--active-export") == 0 && exportActivePath == nullptr &&
                 index + 1 < argc)
            exportActivePath = argv[++index];
        else if (std::strcmp(argv[index], "--activate") == 0 && activationPath == nullptr &&
                 index + 1 < argc)
            activationPath = argv[++index];
        else if ((std::strcmp(argv[index], "--preview") == 0 ||
                  std::strcmp(argv[index], "--play") == 0 ||
                  std::strcmp(argv[index], "--play-queued") == 0) &&
                 previewPath == nullptr && index + 1 < argc) {
            queued = std::strcmp(argv[index], "--play-queued") == 0;
            playable = queued || std::strcmp(argv[index], "--play") == 0;
            previewPath = argv[++index];
        } else {
            std::fputs("Usage: profile-tools [--summary | --preview SCRIPT | --play SCRIPT | "
                       "--play-queued SCRIPT | --activate FILE] "
                       "[--example | --import FILE] [--active-export FILE (activation only)]\n",
                       stderr);
            return 2;
        }
    }
    if ((summary && (previewPath || activationPath)) || (previewPath && activationPath)) {
        std::fputs("Summary, preview/play and activation are exclusive\n", stderr);
        return 2;
    }
    if (exportActivePath && !activationPath) {
        std::fputs("Active export requires activation mode\n", stderr);
        return 2;
    }
    if (activationPath)
        return runActivation(inputPath, example, activationPath, exportActivePath);
    std::unique_ptr<Profile> profile(new Profile());
    if (inputPath != nullptr) {
        std::FILE *input = std::fopen(inputPath, "rb");
        if (input == nullptr) {
            std::perror("Cannot open profile");
            return 1;
        }
        // Host allocation; embedded callers provide their own two storage slots.
        std::unique_ptr<Profile> scratch(new Profile());
        FileReader reader(input);
        const ImportResult result = readProfile(reader, *scratch, *profile);
        const bool closeOk = std::fclose(input) == 0;
        if (!result) {
            std::fprintf(
                stderr,
                "Profile import rejected: error=%s offset=%lu section=%u object=%u field=%s "
                "validation=%u detail=%u\n",
                importErrorName(result.error), static_cast<unsigned long>(result.offset),
                static_cast<unsigned>(result.section), result.object, result.field,
                static_cast<unsigned>(result.validation.error), result.validation.detail);
            return 1;
        }
        if (!closeOk)
            return 1;
    } else if (!(example ? makeHintExampleProfile(*profile) : makeStandardProfile(*profile))) {
        const ValidationResult result = validateProfile(*profile);
        std::fprintf(stderr,
                     "Profile construction rejected: error=%u section=%u object=%u detail=%u\n",
                     static_cast<unsigned>(result.error), static_cast<unsigned>(result.section),
                     result.object, result.detail);
        return 1;
    }
    if (previewPath != nullptr) {
        std::FILE *input = std::fopen(previewPath, "rb");
        if (input == nullptr) {
            std::perror("Cannot open preview script");
            return 1;
        }
        const int status =
            playable ? runPlay(*profile, input, queued) : runPreview(*profile, input);
        return std::fclose(input) == 0 ? status : 1;
    }
    if (summary) {
        std::printf("{\"profileBytes\":%lu,\"components\":%lu,\"roles\":%lu,\"states\":%lu,"
                    "\"transitions\":%lu,\"conditions\":%lu,\"actions\":%lu,\"effects\":%lu}\n",
                    static_cast<unsigned long>(sizeof(Profile)),
                    static_cast<unsigned long>(profile->components.size()),
                    static_cast<unsigned long>(profile->roles.size()),
                    static_cast<unsigned long>(profile->states.size()),
                    static_cast<unsigned long>(profile->transitions.size()),
                    static_cast<unsigned long>(profile->conditions.size()),
                    static_cast<unsigned long>(profile->actions.size()),
                    static_cast<unsigned long>(profile->effects.size()));
        return std::fflush(stdout) == 0 ? 0 : 1;
    }
    FileWriter writer(stdout);
    if (!writeProfile(*profile, writer) || std::fflush(stdout) != 0)
        return 1;
    return 0;
}
