#include "GameConfiguration/CanonicalPackageJson.h"

namespace larp {
namespace configuration {
namespace {
class MemorySource : public IPackageByteSource {
  public:
    MemorySource(const char *bytes, std::size_t count) : bytes_(bytes), count_(count) {}
    PackageReadStatus readByte(std::uint8_t &byte) override {
        if (position_ == count_)
            return PackageReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes_[position_++]);
        return PackageReadStatus::Byte;
    }

  private:
    const char *bytes_;
    std::size_t count_;
    std::size_t position_ = 0;
};
bool digit(std::uint8_t byte) {
    return byte >= '0' && byte <= '9';
}
} // namespace

void CanonicalPackageJsonReader::reset(IPackageByteSource *source) {
    source_ = source;
    result_ = {};
    offset_ = 0;
    depth_ = 0;
    byte_ = 0;
    buffered_ = false;
    ended_ = false;
}
bool CanonicalPackageJsonReader::fail(CanonicalJsonError error, std::size_t offset) {
    if (result_.error == CanonicalJsonError::None) {
        result_.error = error;
        result_.offset = offset;
    }
    return false;
}
bool CanonicalPackageJsonReader::peek() {
    if (result_.error != CanonicalJsonError::None || ended_)
        return false;
    if (buffered_)
        return true;
    const PackageReadStatus status = source_->readByte(byte_);
    if (status == PackageReadStatus::Failure)
        return fail(CanonicalJsonError::InputFailure, offset_);
    if (status == PackageReadStatus::End) {
        ended_ = true;
        return false;
    }
    if (offset_ == DocumentBytes)
        return fail(CanonicalJsonError::DocumentLimit, offset_);
    buffered_ = true;
    return true;
}
bool CanonicalPackageJsonReader::need() {
    return peek() || fail(CanonicalJsonError::UnexpectedEnd, offset_);
}
void CanonicalPackageJsonReader::consume() {
    buffered_ = false;
    ++offset_;
}
bool CanonicalPackageJsonReader::push(State state) {
    if (depth_ == ContainerDepth)
        return fail(CanonicalJsonError::DepthLimit, offset_);
    frames_[depth_] = {};
    frames_[depth_++].state = state;
    consume();
    return true;
}
bool CanonicalPackageJsonReader::utf8() {
    const std::uint8_t lead = byte_;
    std::size_t count = 0;
    if (lead >= 0xc2 && lead <= 0xdf)
        count = 1;
    else if (lead >= 0xe0 && lead <= 0xef)
        count = 2;
    else if (lead >= 0xf0 && lead <= 0xf4)
        count = 3;
    else
        return fail(CanonicalJsonError::InvalidUtf8, offset_);
    consume();
    for (std::size_t index = 0; index < count; ++index) {
        if (!need())
            return false;
        std::uint8_t minimum = 0x80;
        std::uint8_t maximum = 0xbf;
        if (index == 0) {
            if (lead == 0xe0)
                minimum = 0xa0;
            else if (lead == 0xed)
                maximum = 0x9f;
            else if (lead == 0xf0)
                minimum = 0x90;
            else if (lead == 0xf4)
                maximum = 0x8f;
        }
        if (byte_ < minimum || byte_ > maximum)
            return fail(CanonicalJsonError::InvalidUtf8, offset_);
        consume();
    }
    return true;
}
bool CanonicalPackageJsonReader::string(std::uint8_t *key, std::size_t *length) {
    std::size_t size = 0;
    consume(); // opening quote
    while (need()) {
        if (byte_ == '"') {
            consume();
            if (length)
                *length = size;
            return true;
        }
        if (byte_ < 0x20)
            return fail(CanonicalJsonError::InvalidText, offset_);
        const std::size_t characterOffset = offset_;
        std::uint8_t decoded = byte_;
        if (byte_ == '\\') {
            consume();
            if (!need())
                return false;
            if (byte_ != '"' && byte_ != '\\')
                return fail(CanonicalJsonError::InvalidEscape, offset_);
            decoded = byte_;
        } else if (byte_ >= 0x80) {
            if (key)
                return fail(CanonicalJsonError::InvalidText, offset_);
            if (!utf8())
                return false;
            continue;
        }
        if (key) {
            if (size == KeyBytes)
                return fail(CanonicalJsonError::KeyTooLong, characterOffset);
            key[size++] = decoded;
        }
        consume();
    }
    return false;
}
bool CanonicalPackageJsonReader::readKey(Frame &frame) {
    const std::size_t begin = offset_;
    std::uint8_t key[KeyBytes];
    std::size_t length = 0;
    if (!string(key, &length))
        return false;
    if (frame.hasKey) {
        std::size_t common = 0;
        while (common < length && common < frame.keyLength && key[common] == frame.key[common])
            ++common;
        if (common == length && common == frame.keyLength)
            return fail(CanonicalJsonError::DuplicateKey, begin);
        if (common == length || (common < frame.keyLength && key[common] < frame.key[common]))
            return fail(CanonicalJsonError::KeyOrder, begin);
    }
    for (std::size_t index = 0; index < length; ++index)
        frame.key[index] = key[index];
    frame.keyLength = static_cast<std::uint8_t>(length);
    frame.hasKey = true;
    return true;
}
bool CanonicalPackageJsonReader::integer() {
    if (byte_ == '-') {
        consume();
        if (!need())
            return false;
        if (!digit(byte_) || byte_ == '0')
            return fail(CanonicalJsonError::InvalidNumber, offset_);
    }
    if (byte_ == '0') {
        consume();
        if (peek() && digit(byte_))
            return fail(CanonicalJsonError::InvalidNumber, offset_);
    } else {
        do {
            consume();
        } while (peek() && digit(byte_));
    }
    if (peek() && (byte_ == '.' || byte_ == 'e' || byte_ == 'E' || byte_ == '+'))
        return fail(CanonicalJsonError::InvalidNumber, offset_);
    return result_.error == CanonicalJsonError::None;
}
bool CanonicalPackageJsonReader::literal(const char *text) {
    for (std::size_t index = 0; text[index] != '\0'; ++index) {
        if (!need())
            return false;
        if (byte_ != static_cast<std::uint8_t>(text[index]))
            return fail(CanonicalJsonError::UnexpectedToken, offset_);
        consume();
    }
    return true;
}
bool CanonicalPackageJsonReader::value() {
    if (!need())
        return false;
    if (byte_ == '{')
        return push(State::ObjectKeyOrEnd);
    if (byte_ == '[')
        return push(State::ArrayValueOrEnd);
    if (byte_ == '"')
        return string();
    if (byte_ == 't')
        return literal("true");
    if (byte_ == 'f')
        return literal("false");
    if (byte_ == '+' || byte_ == '.')
        return fail(CanonicalJsonError::InvalidNumber, offset_);
    if (byte_ == '-' || digit(byte_))
        return integer();
    return fail(CanonicalJsonError::UnexpectedToken, offset_);
}
bool CanonicalPackageJsonReader::document() {
    if (!need())
        return false;
    if (byte_ != '{')
        return fail(CanonicalJsonError::UnexpectedToken, offset_);
    if (!push(State::ObjectKeyOrEnd))
        return false;
    while (depth_ != 0) {
        if (!need())
            return false;
        Frame &frame = frames_[depth_ - 1];
        switch (frame.state) {
        case State::ObjectKeyOrEnd:
        case State::ObjectKey:
            if (byte_ == '}' && frame.state == State::ObjectKeyOrEnd) {
                consume();
                --depth_;
            } else {
                if (byte_ != '"')
                    return fail(CanonicalJsonError::UnexpectedToken, offset_);
                if (!readKey(frame))
                    return false;
                frame.state = State::ObjectColon;
            }
            break;
        case State::ObjectColon:
            if (byte_ != ':')
                return fail(CanonicalJsonError::UnexpectedToken, offset_);
            consume();
            frame.state = State::ObjectValue;
            break;
        case State::ObjectValue:
            frame.state = State::ObjectCommaOrEnd;
            if (!value())
                return false;
            break;
        case State::ArrayValueOrEnd:
        case State::ArrayValue:
            if (byte_ == ']' && frame.state == State::ArrayValueOrEnd) {
                consume();
                --depth_;
            } else {
                frame.state = State::ArrayCommaOrEnd;
                if (!value())
                    return false;
            }
            break;
        case State::ObjectCommaOrEnd:
        case State::ArrayCommaOrEnd: {
            const bool object = frame.state == State::ObjectCommaOrEnd;
            if (byte_ == ',') {
                consume();
                frame.state = object ? State::ObjectKey : State::ArrayValue;
            } else if (byte_ == (object ? '}' : ']')) {
                consume();
                --depth_;
            } else {
                return fail(CanonicalJsonError::UnexpectedToken, offset_);
            }
            break;
        }
        }
    }
    if (peek())
        return fail(CanonicalJsonError::TrailingData, offset_);
    return result_.error == CanonicalJsonError::None;
}
CanonicalJsonResult CanonicalPackageJsonReader::validate(IPackageByteSource &source) {
    reset(&source);
    if (document())
        result_.offset = offset_;
    source_ = nullptr;
    return result_;
}
CanonicalJsonResult CanonicalPackageJsonReader::validate(const char *bytes, std::size_t count) {
    if (!bytes && count != 0) {
        reset(nullptr);
        fail(CanonicalJsonError::InvalidArgument, 0);
        return result_;
    }
    MemorySource source(bytes, count);
    return validate(source);
}
} // namespace configuration
} // namespace larp
