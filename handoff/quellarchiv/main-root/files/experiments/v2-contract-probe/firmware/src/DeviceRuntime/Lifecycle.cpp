#include "DeviceRuntime/Lifecycle.h"
namespace larp {
namespace devices {
bool sameControlTicket(const ControlTicket &a, const ControlTicket &b) {
    return a.generation == b.generation && a.sequence == b.sequence && a.owner == b.owner &&
           a.kind == b.kind;
}
const char *controlKindName(ControlKind value) {
    static const char *const names[] = {"park", "begin", "component", "commit", "resume"};
    const unsigned index = static_cast<unsigned>(value);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown_control";
}
} // namespace devices
} // namespace larp
