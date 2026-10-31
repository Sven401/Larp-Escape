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
// Optional synchronous instrumentation. No pointers are retained or supplied.
// Events can be incomplete on failure. Callbacks must not reenter the reader or
// mutate its source. No event constitutes accepted package data.
enum class CanonicalJsonToken : std::uint8_t {
    ObjectBegin,
    ObjectEnd,
    ArrayBegin,
    ArrayEnd,
    Key,
    String,
    Integer,
    Boolean
};
class ICanonicalJsonObserver {
  public:
    virtual ~ICanonicalJsonObserver() = default;
    virtual void token(CanonicalJsonToken token, std::size_t offset) = 0;
    virtual void scalarByte(std::uint8_t byte) = 0;
    virtual void scalarEnd(std::size_t offset) = 0;
};
// This bounded operating profile is not a schema/module/capability validator.
// One exclusive synchronous call; source bytes must remain stable during it.
// No source pointer is retained after return. No heap, DOM or recursive descent.
class CanonicalPackageJsonReader {
  public:
    static const std::size_t DocumentBytes = 262144;
    static const std::size_t ContainerDepth = 32;
    static const std::size_t KeyBytes = 48;
    CanonicalJsonResult validate(IPackageByteSource &source,
                                 ICanonicalJsonObserver *observer = nullptr);
    CanonicalJsonResult validate(const char *bytes, std::size_t count,
                                 ICanonicalJsonObserver *observer = nullptr);

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
    ICanonicalJsonObserver *observer_ = nullptr;
    CanonicalJsonResult result_;
    std::size_t offset_ = 0;
    std::size_t depth_ = 0;
    std::uint8_t byte_ = 0;
    bool buffered_ = false;
    bool ended_ = false;
    Frame frames_[ContainerDepth];

    void reset(IPackageByteSource *source);
    void token(CanonicalJsonToken value);
    void scalarByte(std::uint8_t byte);
    void scalarEnd();
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
