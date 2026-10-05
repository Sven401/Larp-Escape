// Host-side profile export/inspection. This is not an embedded entry point.
#include "GameConfiguration/ProfileWriter.h"
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
} // namespace
int main(int argc, char **argv) {
    using namespace larp::configuration;
    bool summary = false;
    bool example = false;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--summary") == 0 && !summary)
            summary = true;
        else if (std::strcmp(argv[index], "--example") == 0 && !example)
            example = true;
        else {
            std::fputs("Usage: profile-tools [--summary] [--example]\n", stderr);
            return 2;
        }
    }
    std::unique_ptr<Profile> profile(new Profile());
    if (!(example ? makeHintExampleProfile(*profile) : makeStandardProfile(*profile))) {
        const ValidationResult result = validateProfile(*profile);
        std::fprintf(stderr,
                     "Profile construction rejected: error=%u section=%u object=%u detail=%u\n",
                     static_cast<unsigned>(result.error), static_cast<unsigned>(result.section),
                     result.object, result.detail);
        return 1;
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
