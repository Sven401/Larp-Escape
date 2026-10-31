#pragma once
#include "GameConfiguration/CanonicalPackageJson.h"

namespace larp {
namespace configuration {
enum class DeclarationError : std::uint8_t {
    None,
    CanonicalInput,
    FieldSet,
    Type,
    Value,
    Range,
    ListOrder,
    ModuleConflict,
    Capacity,
    MetadataLimit
};
enum class DeclarationScope : std::uint8_t { Rejected, DeclarationsOnly };
struct DeclarationResult {
    DeclarationScope scope = DeclarationScope::Rejected;
    DeclarationError error = DeclarationError::None;
    CanonicalJsonResult canonical;
    std::size_t offset = 0;
    char path[256] = {};
    // Deliberately no bool conversion: this is never full package validity.
};
// Synchronous declaration-only validation, with no accepted package output.
// Rules/instances are only byte-canonical and bounded, not semantically checked.
// Exclusive, non-reentrant call; source must remain stable. No heap or DOM.
// Canonical/source errors take precedence over the first declaration error.
class PackageDeclarationReader : private ICanonicalJsonObserver {
  public:
    DeclarationResult validate(IPackageByteSource &source,
                               ICanonicalJsonObserver *observer = nullptr);
    DeclarationResult validate(const char *bytes, std::size_t count,
                               ICanonicalJsonObserver *observer = nullptr);
    static const std::size_t MetadataBytes = 32768;

  private:
    enum class Kind : std::uint8_t {
        Root,
        Requirement,
        Module,
        Subject,
        Resource,
        Station,
        Value,
        Timer,
        Port,
        Requirements,
        Modules,
        Subjects,
        Resources,
        Stations,
        Values,
        Timers,
        Ports,
        Skip
    };
    struct Frame {
        Kind kind = Kind::Skip;
        std::uint16_t seen = 0;
        int field = -1;
        std::size_t count = 0;
        char key[49] = {};
        char previous[49] = {};
        CanonicalJsonToken initialType = CanonicalJsonToken::ObjectBegin;
        bool booleanValue = false;
    };
    struct ModuleIdentity {
        char id[49] = {};
        char version[18] = {};
        char digest[65] = {};
    };
    ModuleIdentity modules_[16];
    ModuleIdentity module_;
    std::size_t moduleCount_ = 0;
    CanonicalPackageJsonReader canonical_;
    ICanonicalJsonObserver *downstream_ = nullptr;
    DeclarationResult result_;
    Frame frames_[CanonicalPackageJsonReader::ContainerDepth];
    std::size_t depth_ = 0;
    std::size_t tokenOffset_ = 0;
    std::size_t length_ = 0;
    std::size_t skippedBytes_ = 0;
    std::size_t skipBegin_ = 0;
    std::size_t skipDepth_ = 0;
    char text_[97] = {};
    CanonicalJsonToken scalar_ = CanonicalJsonToken::Key;
    void reset();
    void token(CanonicalJsonToken value, std::size_t offset) override;
    void scalarByte(std::uint8_t byte) override;
    void scalarEnd(std::size_t offset) override;
    void fail(DeclarationError error, std::size_t offset);
    void path();
    Kind child(bool array);
    void scalar();
    bool string() const;
    bool identifier() const;
    bool integer(std::int64_t low, std::int64_t high) const;
    bool digest() const;
    bool semver() const;
    bool equals(const char *text) const;
    void identity();
    DeclarationResult finish(CanonicalJsonResult canonical);
};
} // namespace configuration
} // namespace larp
