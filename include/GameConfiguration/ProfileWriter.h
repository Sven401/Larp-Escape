#pragma once
#include "GameConfiguration/Profile.h"
#include <cstddef>

namespace larp {
namespace configuration {
class IProfileWriter {
  public:
    virtual ~IProfileWriter() = default;
    // Synchronous sink: bytes are borrowed only until write returns.
    virtual bool write(const char *bytes, std::size_t count) = 0;
};
// Validates before writing. A sink failure can leave partial output: callers
// must discard that document. The profile must not change during serialization.
// No heap allocation, transport dependency or native object-layout persistence.
bool writeProfile(const Profile &profile, IProfileWriter &writer);
} // namespace configuration
} // namespace larp
