#pragma once
#include <cstddef>
#include <cstdint>

namespace larp {
namespace configuration {
enum class PackageReadStatus : std::uint8_t { Byte, End, Failure };
class IPackageByteSource {
  public:
    virtual ~IPackageByteSource() = default;
    virtual PackageReadStatus readByte(std::uint8_t &byte) = 0;
};
enum class CanonicalJsonError : std::uint8_t {
    None,
    InvalidArgument,
    InputFailure,
    UnexpectedEnd,
    UnexpectedToken,
    InvalidText,
    InvalidUtf8,
    InvalidEscape,
    InvalidNumber,
    KeyTooLong,
    KeyOrder,
    DuplicateKey,
    DepthLimit,
    DocumentLimit,
    TrailingData
};
struct CanonicalJsonResult {
    CanonicalJsonError error = CanonicalJsonError::None;
    std::size_t offset = 0;
    explicit operator bool() const {
        return error == CanonicalJsonError::None;
    }
};
// This bounded operating profile is not a schema/module/capability validator.
// One exclusive synchronous call; source bytes must remain stable during it.
// No source pointer is retained after return. No heap, DOM or recursive descent.
class CanonicalPackageJsonReader {
  public:
    static const std::size_t DocumentBytes = 262144;
    static const std::size_t ContainerDepth = 32;
    static const std::size_t KeyBytes = 48;
    CanonicalJsonResult validate(IPackageByteSource &source);
    CanonicalJsonResult validate(const char *bytes, std::size_t count);

  private:
    enum class State : std::uint8_t {
        ObjectKeyOrEnd,
        ObjectKey,
        ObjectColon,
        ObjectValue,
        ObjectCommaOrEnd,
        ArrayValueOrEnd,
        ArrayValue,
        ArrayCommaOrEnd
    };
    struct Frame {
        State state = State::ObjectKeyOrEnd;
        std::uint8_t keyLength = 0;
        bool hasKey = false;
        std::uint8_t key[KeyBytes] = {};
    };
    IPackageByteSource *source_ = nullptr;
    CanonicalJsonResult result_;
    std::size_t offset_ = 0;
    std::size_t depth_ = 0;
    std::uint8_t byte_ = 0;
    bool buffered_ = false;
    bool ended_ = false;
    Frame frames_[ContainerDepth];

    void reset(IPackageByteSource *source);
    bool fail(CanonicalJsonError error, std::size_t offset);
    bool peek();
    bool need();
    void consume();
    bool push(State state);
    bool string(std::uint8_t *key = nullptr, std::size_t *length = nullptr);
    bool utf8();
    bool readKey(Frame &frame);
    bool integer();
    bool literal(const char *text);
    bool value();
    bool document();
};
} // namespace configuration
} // namespace larp
