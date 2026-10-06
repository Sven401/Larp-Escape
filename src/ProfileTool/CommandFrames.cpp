#include "CommandFrames.h"
#include "Companion/CommandFrame.h"
#include <algorithm>
#include <array>
#include <cstdio>
int runCommandFrames(const char *path) {
    using namespace larp::companion;
    auto *input = std::fopen(path, "rb");
    if (!input) {
        std::perror("Cannot open command");
        return 1;
    }
    std::array<char, maxGameCommandDocumentBytes + 1> document = {};
    const auto count = std::fread(document.data(), 1, document.size(), input);
    const bool okay = !std::ferror(input);
    const bool closed = std::fclose(input) == 0;
    GameCommand command;
    const auto decoded = decodeGameCommand(document.data(), count, command);
    if (!okay || !closed || !decoded) {
        std::fprintf(stderr, "Command rejected: error=%s offset=%lu\n",
                     commandDecodeErrorName(decoded.error),
                     static_cast<unsigned long>(decoded.offset));
        return 1;
    }
    std::array<char, maxGameCommandJsonBytes> canonical = {};
    std::size_t written = 0;
    if (encodeGameCommand(command, canonical.data(), canonical.size(), written) !=
        ConfiguredEncodeResult::Success)
        return 1;
    // 20-byte prototype values: 8-byte header + at most 12 payload bytes.
    std::array<char, 20> frame = {};
    for (std::size_t offset = 0; offset < written;) {
        const auto length = std::min<std::size_t>(12, written - offset);
        std::size_t framed = 0;
        if (!encodeCommandFrame(1, static_cast<std::uint16_t>(written),
                                static_cast<std::uint16_t>(offset), canonical.data() + offset,
                                length, frame.data(), frame.size(), framed))
            return 1;
        for (std::size_t i = 0; i < framed; ++i)
            if (std::printf("%02X", static_cast<unsigned char>(frame[i])) < 0)
                return 1;
        if (std::putchar('\n') == EOF)
            return 1;
        offset += length;
    }
    return std::fflush(stdout) == 0 ? 0 : 1;
}
