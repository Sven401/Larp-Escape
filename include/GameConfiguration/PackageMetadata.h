#pragma once
#include "GameConfiguration/PackageModules.h"

namespace larp {
namespace configuration {
// MetadataSourceProfile-v1, a precise conservative maintainer operating profile.
// Counts complete canonical content and distinct pinned module sources, excluding
// ONLY content.stations[].rules, module.rules and module.constraints array values.
class PackageSourceMetadata final : public ICompilerSourceObserver {
  public:
    static const std::uint64_t Limit = 32768;
    void reset();
    void beginSource(bool module, std::uint16_t source) override;
    void endSource(CanonicalJsonResult result) override;
    void token(CanonicalJsonToken token, std::size_t offset) override;
    void scalarByte(std::uint8_t byte) override;
    void scalarEnd(std::size_t offset) override;
    std::uint64_t metadataBytes() const {
        return total_;
    }
    std::uint64_t rawBytes() const {
        return raw_;
    }
    bool exceeded() const {
        return total_ > Limit;
    }

  private:
    enum class Kind : std::uint8_t { Other, Content, Module, Stations, Station };
    struct Frame {
        Kind kind = Kind::Other;
        char key[49] = {};
    };
    Frame frames_[32];
    std::size_t depth_ = 0, keyLength_ = 0, excludedDepth_ = 0;
    std::uint64_t excludedStart_ = 0, excluded_ = 0, total_ = 0, raw_ = 0;
    bool module_ = false, key_ = false;
};
} // namespace configuration
} // namespace larp
