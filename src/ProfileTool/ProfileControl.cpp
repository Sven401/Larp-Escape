#include "ProfileControl.h"
#include "Companion/ProfileControlFrames.h"
#include <cstdio>
#include <cstring>
namespace {
bool number(const char *text, std::uint32_t maximum, std::uint32_t &value) {
    value = 0;
    if (!text || !*text)
        return false;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9' || value > (maximum - static_cast<unsigned>(*p - '0')) / 10)
            return false;
        value = value * 10 + static_cast<unsigned>(*p - '0');
    }
    return value != 0;
}
} // namespace
int runProfileControl(const char *transfer, const char *request, const char *operation) {
    using namespace larp::companion;
    std::uint32_t t = 0, id = 0;
    if (!number(transfer, 65535, t) || !number(request, UINT32_MAX, id))
        return 2;
    ProfileControlFrame command;
    command.transfer = static_cast<std::uint16_t>(t);
    command.request = id;
    if (std::strcmp(operation, "finish") == 0)
        command.operation = ProfileOperation::Finish;
    else if (std::strcmp(operation, "abort") == 0)
        command.operation = ProfileOperation::Abort;
    else if (std::strcmp(operation, "apply") == 0)
        command.operation = ProfileOperation::Apply;
    else
        return 2;
    char bytes[12];
    std::size_t written = 0;
    if (!encodeProfileControl(command, bytes, sizeof(bytes), written))
        return 1;
    for (std::size_t i = 0; i < written; ++i)
        if (std::printf("%02X", static_cast<unsigned char>(bytes[i])) != 2)
            return 1;
    return std::fputc(10, stdout) != EOF && std::fflush(stdout) == 0 ? 0 : 1;
}
