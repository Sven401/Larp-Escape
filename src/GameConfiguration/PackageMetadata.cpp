#include "GameConfiguration/PackageMetadata.h"
#include <cstring>

namespace larp {
namespace configuration {
void PackageSourceMetadata::reset() {
    depth_ = keyLength_ = excludedDepth_ = 0;
    excludedStart_ = excluded_ = total_ = raw_ = 0;
    module_ = key_ = false;
}
void PackageSourceMetadata::beginSource(bool module, std::uint16_t) {
    module_ = module;
    depth_ = keyLength_ = excludedDepth_ = 0;
    excludedStart_ = excluded_ = 0;
    key_ = false;
}
void PackageSourceMetadata::endSource(CanonicalJsonResult result) {
    if (result) {
        raw_ += result.offset;
        total_ += static_cast<std::uint64_t>(result.offset) - excluded_;
    }
}
void PackageSourceMetadata::token(CanonicalJsonToken t, std::size_t offset) {
    key_ = t == CanonicalJsonToken::Key;
    if (key_) {
        keyLength_ = 0;
        return;
    }
    if (t == CanonicalJsonToken::ObjectBegin || t == CanonicalJsonToken::ArrayBegin) {
        if (depth_ == 32)
            return;
        Frame next;
        if (!depth_)
            next.kind = module_ ? Kind::Module : Kind::Content;
        else {
            const auto &parent = frames_[depth_ - 1];
            if (t == CanonicalJsonToken::ArrayBegin && !excludedDepth_ &&
                ((parent.kind == Kind::Station && !std::strcmp(parent.key, "rules")) ||
                 (parent.kind == Kind::Module && (!std::strcmp(parent.key, "rules") ||
                                                  !std::strcmp(parent.key, "constraints"))))) {
                excludedDepth_ = depth_ + 1;
                excludedStart_ = offset;
            }
            if (parent.kind == Kind::Content && t == CanonicalJsonToken::ArrayBegin &&
                !std::strcmp(parent.key, "stations"))
                next.kind = Kind::Stations;
            if (parent.kind == Kind::Stations && t == CanonicalJsonToken::ObjectBegin)
                next.kind = Kind::Station;
        }
        frames_[depth_++] = next;
    } else if (t == CanonicalJsonToken::ObjectEnd || t == CanonicalJsonToken::ArrayEnd) {
        if (excludedDepth_ == depth_) {
            excluded_ += static_cast<std::uint64_t>(offset) - excludedStart_ + 1;
            excludedDepth_ = 0;
        }
        if (depth_)
            --depth_;
    }
}
void PackageSourceMetadata::scalarByte(std::uint8_t byte) {
    if (key_ && depth_ && keyLength_ < 48)
        frames_[depth_ - 1].key[keyLength_++] = static_cast<char>(byte);
}
void PackageSourceMetadata::scalarEnd(std::size_t) {
    if (key_ && depth_)
        frames_[depth_ - 1].key[keyLength_] = 0;
}
} // namespace configuration
} // namespace larp
