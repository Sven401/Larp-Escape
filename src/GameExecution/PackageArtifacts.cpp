#include "GameExecution/PackageArtifacts.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

namespace larp {
namespace execution {
namespace artifacts {
namespace {
using configuration::CanonicalJsonToken;
using configuration::PackageReadStatus;
using configuration::module_ir::SymbolKind;
using configuration::module_ir::Type;
const char *const rootFields[] = {"contentDigest", "dependencies", "event", "format", "profiles",
                                  "requires",      "resources",    "rules", "run",    "stations"};
const char *const stationFields[] = {"bindingDigest", "bindingPath", "boxId",   "generation", "id",
                                     "ports",         "stationId",   "subjects"};
const char *const portFields[] = {"channel", "componentId", "id", "resources"};
const char *const subjectFields[] = {"id", "subjectId"};
const char *const profileFields[] = {"bytes", "digest", "format", "path", "station"};
const char *const resourceFields[] = {"bytes", "digest",  "id",          "locator",
                                      "path",  "station", "verification"};
const char *const locatorFields[] = {"bank", "preset", "track"};
const char *const ruleFields[] = {"expandedId", "generation", "offline", "ruleId", "station"};
const char *const offlineFields[] = {"policy", "rule"};
const char *const dependencyFields[] = {"originStation", "ruleId", "station", "subject"};
const char *const requirementFields[] = {"name", "version"};
template <std::size_t N> bool equal(const std::uint8_t (&a)[N], const std::uint8_t (&b)[N]) {
    return std::memcmp(a, b, N) == 0;
}
bool equal(const Identity &a, const Identity &b) {
    return equal(a.bytes, b.bytes);
}
bool equal(const Digest &a, const Digest &b) {
    return equal(a.bytes, b.bytes);
}
bool nonzero(const Identity &a) {
    for (auto byte : a.bytes)
        if (byte)
            return true;
    return false;
}
unsigned hex(char c) {
    return c >= '0' && c <= '9'   ? static_cast<unsigned>(c - '0')
           : c >= 'a' && c <= 'f' ? static_cast<unsigned>(c - 'a' + 10)
                                  : 16;
}
const char *sourceName(const configuration::ModuleCompileWorkspace &s, Id id) {
    if (!id || id > s.stringOffsets.count || !s.stringOffsets.data || !s.strings.data)
        return "";
    const auto offset = s.stringOffsets.data[id - 1];
    if (offset >= s.strings.count ||
        !std::memchr(s.strings.data + offset, 0, s.strings.count - offset))
        return "";
    return s.strings.data + offset;
}
std::uint16_t field(const char *key, const char *const *fields, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        if (!std::strcmp(key, fields[i]))
            return static_cast<std::uint16_t>(1u << i);
    return 0;
}
bool checkedPath(const char *s) {
    if (!s || !*s)
        return false;
    const char *start = s;
    for (const char *p = s;; ++p) {
        const unsigned ch = static_cast<unsigned char>(*p);
        if (ch > 127 || ch == ':' || ch == '\\' || (ch && ch < 32))
            return false;
        if (ch == '/' || !ch) {
            const std::size_t n = static_cast<std::size_t>(p - start);
            if (!n || (n == 1 && start[0] == '.') || (n == 2 && start[0] == '.' && start[1] == '.'))
                return false;
            if (!ch)
                return true;
            start = p + 1;
        }
    }
}
bool expandedName(const char *s) {
    if (!s || *s != '/')
        return false;
    unsigned depth = 0, count = 0;
    for (++s;; ++s) {
        const unsigned c = static_cast<unsigned char>(*s);
        if (c && c != '/' && (c < 32 || c > 127))
            return false;
        if (!c || c == '/') {
            if (!count || count > 48 || ++depth > 5)
                return false;
            if (!c)
                return true;
            count = 0;
        } else {
            if (c == '~') {
                if (s[1] != '0' && s[1] != '1')
                    return false;
                ++s;
            }
            ++count;
        }
    }
}
bool digestText(const char *s, Digest &out) {
    if (std::strlen(s) != 64)
        return false;
    for (unsigned i = 0; i < 32; ++i) {
        const unsigned a = hex(s[i * 2]), b = hex(s[i * 2 + 1]);
        if (a == 16 || b == 16)
            return false;
        out.bytes[i] = static_cast<std::uint8_t>(a * 16 + b);
    }
    return true;
}
constexpr std::uint16_t BindingRole = 1, RulesRole = 2, LegacyRole = 4, ResourceRole = 8;
} // namespace
const char *name(const Catalog &c, Id id) {
    if (!id || id > c.offsets.count || !c.offsets.data || !c.strings.data)
        return "";
    const auto at = c.offsets.data[id - 1];
    return at < c.strings.count ? c.strings.data + at : "";
}
const char *Validator::string(Id id) const {
    return name(*catalog_, id);
}
void Validator::fail(Error error) {
    if (result_.error != Error::None && error != Error::SourceFailure)
        return;
    result_.error = error;
    result_.source = source_;
    result_.artifact = currentArtifact_;
    result_.offset = offset_;
    result_.path[0] = 0;
    if (diagnostic_[0]) {
        std::strcpy(result_.path, diagnostic_);
        return;
    }
    std::size_t used = 0;
    for (std::size_t i = 0; i < depth_; ++i) {
        const auto &f = frames_[i];
        if (!f.key[0])
            continue;
        if (used + 1 < sizeof(result_.path))
            result_.path[used++] = '/';
        for (const char *p = f.key; *p; ++p) {
            const bool escaped = *p == '~' || *p == '/';
            if (used + (escaped ? 2 : 1) >= sizeof(result_.path))
                break;
            if (escaped) {
                result_.path[used++] = '~';
                result_.path[used++] = *p == '~' ? '0' : '1';
            } else
                result_.path[used++] = *p;
        }
    }
    result_.path[used] = 0;
}
void Validator::where(const char *field, std::size_t index, std::uint64_t offset, Id artifact) {
    currentArtifact_ = artifact;
    offset_ = offset;
    source_ = artifact                              ? InputSource::Artifact
              : !std::strncmp(field, "content/", 8) ? InputSource::Content
                                                    : InputSource::Deployment;
    if (result_.error == Error::None)
        result_.module = 0;
    std::snprintf(diagnostic_, sizeof(diagnostic_), "/%s/%lu", field,
                  static_cast<unsigned long>(index));
}
Id Validator::intern(const char *s) {
    if (!s) {
        fail(Error::Reference);
        return 0;
    }
    for (std::size_t i = 0; i < catalog_->offsets.count; ++i)
        if (!std::strcmp(string(static_cast<Id>(i + 1)), s))
            return static_cast<Id>(i + 1);
    const auto count = std::strlen(s) + 1;
    if (!catalog_->strings.data || count > catalog_->strings.capacity - catalog_->strings.count) {
        fail(Error::OperatingProfile);
        return 0;
    }
    const Id id = append(catalog_->offsets);
    if (!id)
        return 0;
    catalog_->offsets.data[id - 1] = static_cast<std::uint32_t>(catalog_->strings.count);
    std::memcpy(catalog_->strings.data + catalog_->strings.count, s, count);
    catalog_->strings.count += count;
    return id;
}
bool Validator::equals(const char *s) const {
    return length_ < sizeof(text_) && std::strlen(s) == length_ && !std::memcmp(s, text_, length_);
}
bool Validator::identifier() const {
    if (scalar_ != CanonicalJsonToken::String || !length_ || length_ > 48)
        return false;
    for (std::size_t i = 0; i < length_; ++i)
        if (static_cast<unsigned char>(text_[i]) < 32 || static_cast<unsigned char>(text_[i]) > 127)
            return false;
    return true;
}
bool Validator::identity(Identity &out) const {
    if (scalar_ != CanonicalJsonToken::String || length_ != 32)
        return false;
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned a = hex(text_[2 * i]), b = hex(text_[2 * i + 1]);
        if (a == 16 || b == 16)
            return false;
        out.bytes[i] = static_cast<std::uint8_t>(a * 16 + b);
    }
    return nonzero(out);
}
bool Validator::digest(Digest &out) const {
    if (scalar_ != CanonicalJsonToken::String || length_ != 64)
        return false;
    for (unsigned i = 0; i < 32; ++i) {
        const unsigned a = hex(text_[2 * i]), b = hex(text_[2 * i + 1]);
        if (a == 16 || b == 16)
            return false;
        out.bytes[i] = static_cast<std::uint8_t>(a * 16 + b);
    }
    return true;
}
bool Validator::uint(std::uint64_t maximum, std::uint64_t &out) const {
    if (scalar_ != CanonicalJsonToken::Integer || !length_ || length_ >= sizeof(text_))
        return false;
    out = 0;
    for (std::size_t i = 0; i < length_; ++i) {
        if (text_[i] < '0' || text_[i] > '9')
            return false;
        const auto digit = static_cast<unsigned>(text_[i] - '0');
        if (out > maximum / 10 || (out == maximum / 10 && digit > maximum % 10))
            return false;
        out = out * 10 + digit;
    }
    return true;
}
Id Validator::findStation(Id id) const {
    for (std::size_t i = 0; i < catalog_->stations.count; ++i)
        if (catalog_->stations.data[i].id == id)
            return static_cast<Id>(i + 1);
    return 0;
}
Id Validator::path(Id text, const Digest &hash, std::uint64_t bytes, bool length,
                   std::uint16_t role, Id station) {
    if (!checkedPath(string(text))) {
        fail(Error::Path);
        return 0;
    }
    for (std::size_t i = 0; i < catalog_->artifacts.count; ++i) {
        auto &a = catalog_->artifacts.data[i];
        if (a.path != text)
            continue;
        if (!equal(a.expected, hash) || (length && a.hasLength && a.bytes != bytes)) {
            fail(Error::Conflict);
            return 0;
        }
        if (length) {
            a.hasLength = true;
            a.bytes = bytes;
        }
        a.roles |= role;
        if (role == RulesRole) {
            if (a.irStation && a.irStation != station) {
                fail(Error::Conflict);
                return 0;
            }
            a.irStation = station;
        }
        return static_cast<Id>(i + 1);
    }
    const Id id = append(catalog_->artifacts);
    if (!id)
        return 0;
    auto &a = catalog_->artifacts.data[id - 1];
    a.path = text;
    a.expected = hash;
    a.bytes = bytes;
    a.hasLength = length;
    a.roles = role;
    a.irStation = role == RulesRole ? station : 0;
    return id;
}
void Validator::fields(std::uint16_t mask) {
    if (frames_[depth_ - 1].seen != mask)
        fail(Error::FieldSet);
}
Validator::Kind Validator::child(bool array) {
    if (!depth_)
        return sourceMode_ ? Kind::SourceRoot : Kind::Root;
    const auto &f = frames_[depth_ - 1];
    const char *k = f.key;
    if (f.kind == Kind::Skip)
        return Kind::Skip;
    if (sourceMode_) {
        if (sourceModule_)
            return Kind::Skip;
        if (f.kind == Kind::SourceRoot && array && !std::strcmp(k, "requires"))
            return Kind::SourceRequires;
        if (f.kind == Kind::SourceRequires && !array)
            return Kind::SourceRequirement;
        return Kind::Skip;
    }
    if (array) {
        if (f.kind == Kind::Root) {
            if (!std::strcmp(k, "stations"))
                return Kind::Stations;
            if (!std::strcmp(k, "profiles"))
                return Kind::Profiles;
            if (!std::strcmp(k, "resources"))
                return Kind::Resources;
            if (!std::strcmp(k, "rules"))
                return Kind::Rules;
            if (!std::strcmp(k, "dependencies"))
                return Kind::Dependencies;
            if (!std::strcmp(k, "requires"))
                return Kind::Requires;
        }
        if (f.kind == Kind::Station) {
            if (!std::strcmp(k, "ports"))
                return Kind::Ports;
            if (!std::strcmp(k, "subjects"))
                return Kind::Subjects;
        }
        if (f.kind == Kind::Port && !std::strcmp(k, "resources"))
            return Kind::PortResources;
    } else {
        switch (f.kind) {
        case Kind::Stations:
            return Kind::Station;
        case Kind::Ports:
            return Kind::Port;
        case Kind::Subjects:
            return Kind::Subject;
        case Kind::Profiles:
            return Kind::Profile;
        case Kind::Resources:
            return Kind::Resource;
        case Kind::Rules:
            return Kind::Rule;
        case Kind::Dependencies:
            return Kind::Dependency;
        case Kind::Requires:
            return Kind::Requirement;
        default:
            break;
        }
        if (f.kind == Kind::Resource && !std::strcmp(k, "locator"))
            return Kind::Locator;
        if (f.kind == Kind::Rule && !std::strcmp(k, "offline"))
            return Kind::Offline;
    }
    fail(Error::Type);
    return Kind::Skip;
}
void Validator::beginSource(bool module, std::uint16_t source) {
    sourceMode_ = true;
    sourceModule_ = module;
    source_ = module ? InputSource::Module : InputSource::Content;
    diagnostic_[0] = 0;
    depth_ = length_ = 0;
    offset_ = 0;
    if (result_.error == Error::None)
        result_.module = source;
    streamingMajor_ = false;
    metadata_.beginSource(module, source);
}
void Validator::endSource(configuration::CanonicalJsonResult result) {
    metadata_.endSource(result);
    result_.sourceBytes = metadata_.rawBytes();
    result_.metadataBytes = metadata_.metadataBytes();
    if (result && metadata_.exceeded())
        fail(Error::MetadataLimit);
}
void Validator::token(CanonicalJsonToken t, std::size_t offset) {
    if (sourceMode_)
        metadata_.token(t, offset);
    offset_ = offset;
    if (result_.error != Error::None)
        return;
    if (t == CanonicalJsonToken::ObjectBegin || t == CanonicalJsonToken::ArrayBegin) {
        Frame f;
        f.kind = child(t == CanonicalJsonToken::ArrayBegin);
        if (result_.error != Error::None)
            return;
        if (depth_ == 32) {
            fail(Error::OperatingProfile);
            return;
        }
        if (f.kind == Kind::Station) {
            if (catalog_->stations.count == 8) {
                fail(Error::OperatingProfile);
                return;
            }
            f.index = append(catalog_->stations);
            currentStation_ = f.index;
        } else if (f.kind == Kind::Port) {
            f.index = append(catalog_->ports);
            if (f.index) {
                auto &p = catalog_->ports.data[f.index - 1];
                p.station = currentStation_;
                p.firstResource = static_cast<std::uint32_t>(catalog_->portResources.count);
            }
        } else if (f.kind == Kind::Subject) {
            f.index = append(catalog_->subjects);
            if (f.index)
                catalog_->subjects.data[f.index - 1].station = currentStation_;
        } else if (f.kind == Kind::Profile)
            f.index = append(catalog_->profiles);
        else if (f.kind == Kind::Resource)
            f.index = append(catalog_->resources);
        else if (f.kind == Kind::Locator)
            f.index = frames_[depth_ - 1].index;
        else if (f.kind == Kind::Rule)
            f.index = append(catalog_->rules);
        else if (f.kind == Kind::Offline)
            f.index = frames_[depth_ - 1].index;
        else if (f.kind == Kind::Dependency)
            f.index = append(catalog_->dependencies);
        else if (f.kind == Kind::Requirement)
            f.index = append(catalog_->requirements);
        else if (f.kind == Kind::SourceRequirement)
            f.index = append(catalog_->contentRequirements);
        if (f.index) {
            switch (f.kind) {
            case Kind::Station:
                catalog_->stations.data[f.index - 1].offset = offset_;
                break;
            case Kind::Port:
                catalog_->ports.data[f.index - 1].offset = offset_;
                break;
            case Kind::Subject:
                catalog_->subjects.data[f.index - 1].offset = offset_;
                break;
            case Kind::Profile:
                catalog_->profiles.data[f.index - 1].offset = offset_;
                break;
            case Kind::Resource:
                catalog_->resources.data[f.index - 1].offset = offset_;
                break;
            case Kind::Rule:
                catalog_->rules.data[f.index - 1].offset = offset_;
                break;
            case Kind::Dependency:
                catalog_->dependencies.data[f.index - 1].offset = offset_;
                break;
            case Kind::Requirement:
                catalog_->requirements.data[f.index - 1].offset = offset_;
                break;
            case Kind::SourceRequirement:
                catalog_->contentRequirements.data[f.index - 1].offset = offset_;
                break;
            default:
                break;
            }
        }
        frames_[depth_++] = f;
        return;
    }
    if (t == CanonicalJsonToken::ObjectEnd || t == CanonicalJsonToken::ArrayEnd) {
        if (depth_) {
            close();
            --depth_;
        }
        return;
    }
    scalar_ = t;
    length_ = 0;
    text_[0] = 0;
    streamingMajor_ = false;
    if (t == CanonicalJsonToken::Integer && depth_ &&
        (frames_[depth_ - 1].kind == Kind::Requirement ||
         frames_[depth_ - 1].kind == Kind::SourceRequirement) &&
        !std::strcmp(frames_[depth_ - 1].key, "version")) {
        major_ = append(catalog_->offsets);
        if (major_) {
            catalog_->offsets.data[major_ - 1] =
                static_cast<std::uint32_t>(catalog_->strings.count);
            streamingMajor_ = true;
        }
    }
}
void Validator::scalarByte(std::uint8_t byte) {
    if (sourceMode_)
        metadata_.scalarByte(byte);
    if (result_.error != Error::None)
        return;
    if (length_ < sizeof(text_) - 1)
        text_[length_] = static_cast<char>(byte);
    ++length_;
    if (streamingMajor_) {
        if (!catalog_->strings.data || catalog_->strings.count == catalog_->strings.capacity) {
            fail(Error::OperatingProfile);
            return;
        }
        catalog_->strings.data[catalog_->strings.count++] = static_cast<char>(byte);
    }
}
void Validator::scalarEnd(std::size_t offset) {
    if (sourceMode_)
        metadata_.scalarEnd(offset);
    if (result_.error != Error::None || !depth_)
        return;
    text_[length_ < sizeof(text_) ? length_ : sizeof(text_) - 1] = 0;
    auto &f = frames_[depth_ - 1];
    if (f.kind == Kind::Skip || f.kind == Kind::SourceRequires)
        return;
    if (f.kind == Kind::SourceRoot) {
        if (scalar_ == CanonicalJsonToken::Key && length_ <= 48)
            std::strcpy(f.key, text_);
        return;
    }
    if (streamingMajor_) {
        if (!length_ || text_[0] < '1' || text_[0] > '9') {
            fail(Error::Range);
            return;
        }
        if (!catalog_->strings.data || catalog_->strings.count == catalog_->strings.capacity) {
            fail(Error::OperatingProfile);
            return;
        }
        catalog_->strings.data[catalog_->strings.count++] = 0;
        (f.kind == Kind::Requirement ? catalog_->requirements.data[f.index - 1]
                                     : catalog_->contentRequirements.data[f.index - 1])
            .major = major_;
        streamingMajor_ = false;
        return;
    }
    if (scalar_ != CanonicalJsonToken::Key) {
        scalar();
        return;
    }
    if (length_ > 48) {
        fail(Error::FieldSet);
        return;
    }
    std::strcpy(f.key, text_);
    const char *const *keys = nullptr;
    std::size_t count = 0;
    switch (f.kind) {
    case Kind::Root:
        keys = rootFields;
        count = 10;
        break;
    case Kind::Station:
        keys = stationFields;
        count = 8;
        break;
    case Kind::Port:
        keys = portFields;
        count = 4;
        break;
    case Kind::Subject:
        keys = subjectFields;
        count = 2;
        break;
    case Kind::Profile:
        keys = profileFields;
        count = 5;
        break;
    case Kind::Resource:
        keys = resourceFields;
        count = 7;
        break;
    case Kind::Locator:
        keys = locatorFields;
        count = 3;
        break;
    case Kind::Rule:
        keys = ruleFields;
        count = 5;
        break;
    case Kind::Offline:
        keys = offlineFields;
        count = 2;
        break;
    case Kind::Dependency:
        keys = dependencyFields;
        count = 4;
        break;
    case Kind::Requirement:
    case Kind::SourceRequirement:
        keys = requirementFields;
        count = 2;
        break;
    default:
        fail(Error::Type);
        return;
    }
    const auto b = field(f.key, keys, count);
    if (!b)
        fail(Error::FieldSet);
    f.seen |= b;
}
void Validator::scalar() {
    auto &f = frames_[depth_ - 1];
    const char *k = f.key;
    std::uint64_t value = 0;
    if (length_ >= sizeof(text_)) {
        fail(Error::OperatingProfile);
        return;
    }
    auto id = [&]() {
        if (!identifier()) {
            fail(Error::Value);
            return static_cast<Id>(0);
        }
        return intern(text_);
    };
    auto number = [&](std::uint64_t maximum, bool positive) {
        if (!uint(maximum, value) || (positive && !value))
            fail(Error::Range);
    };
    if (f.kind == Kind::Root) {
        if (!std::strcmp(k, "contentDigest")) {
            if (!digest(content_))
                fail(Error::Value);
        } else if (!std::strcmp(k, "event")) {
            if (!identity(event_))
                fail(Error::Value);
        } else if (!std::strcmp(k, "run")) {
            if (!identity(run_))
                fail(Error::Value);
        } else if (!std::strcmp(k, "format")) {
            if (scalar_ != CanonicalJsonToken::String || !equals("larp.deployment.v1"))
                fail(Error::Value);
        } else
            fail(Error::Type);
        return;
    }
    if (f.kind == Kind::Station) {
        auto &s = catalog_->stations.data[f.index - 1];
        if (!std::strcmp(k, "id"))
            s.id = id();
        else if (!std::strcmp(k, "stationId")) {
            if (!identity(s.station))
                fail(Error::Value);
        } else if (!std::strcmp(k, "boxId")) {
            if (!identity(s.box))
                fail(Error::Value);
        } else if (!std::strcmp(k, "generation")) {
            number(std::numeric_limits<std::uint64_t>::max(), true);
            s.generation = value;
        } else if (!std::strcmp(k, "bindingDigest")) {
            if (!digest(s.binding))
                fail(Error::Value);
        } else if (!std::strcmp(k, "bindingPath")) {
            if (scalar_ != CanonicalJsonToken::String || !checkedPath(text_))
                fail(Error::Path);
            s.bindingPath = intern(text_);
        } else
            fail(Error::Type);
        return;
    }
    if (f.kind == Kind::Port) {
        auto &p = catalog_->ports.data[f.index - 1];
        if (!std::strcmp(k, "id"))
            p.id = id();
        else if (!std::strcmp(k, "componentId")) {
            number(65535, true);
            p.component = static_cast<std::uint16_t>(value);
        } else if (!std::strcmp(k, "channel")) {
            number(65535, false);
            p.channel = static_cast<std::uint16_t>(value);
        } else
            fail(Error::Type);
        return;
    }
    if (f.kind == Kind::PortResources) {
        const Id resource = id();
        const Id index = append(catalog_->portResources);
        if (index)
            catalog_->portResources.data[index - 1] = resource;
        auto &p = catalog_->ports.data[frames_[depth_ - 2].index - 1];
        if (p.resourceCount == 65534)
            fail(Error::OperatingProfile);
        else
            ++p.resourceCount;
        return;
    }
    if (f.kind == Kind::Subject) {
        auto &s = catalog_->subjects.data[f.index - 1];
        if (!std::strcmp(k, "id"))
            s.id = id();
        else if (!std::strcmp(k, "subjectId")) {
            if (!identity(s.subject))
                fail(Error::Value);
        } else
            fail(Error::Type);
        return;
    }
    if (f.kind == Kind::Profile) {
        auto &p = catalog_->profiles.data[f.index - 1];
        if (!std::strcmp(k, "station"))
            p.station = id();
        else if (!std::strcmp(k, "path")) {
            if (scalar_ != CanonicalJsonToken::String || !checkedPath(text_))
                fail(Error::Path);
            p.artifact = intern(text_);
        } else if (!std::strcmp(k, "digest")) {
            if (!digest(p.digest))
                fail(Error::Value);
        } else if (!std::strcmp(k, "bytes")) {
            number(4294967295ULL, true);
            p.bytes = static_cast<std::uint32_t>(value);
        } else if (!std::strcmp(k, "format")) {
            p.format = equals("larp.rules.v1") ? ProfileFormat::Rules
                       : equals("profile.v1")  ? ProfileFormat::Legacy
                                               : ProfileFormat::Invalid;
            if (p.format == ProfileFormat::Invalid)
                fail(Error::Value);
        } else
            fail(Error::Type);
        return;
    }
    if (f.kind == Kind::Resource) {
        auto &r = catalog_->resources.data[f.index - 1];
        if (!std::strcmp(k, "station"))
            r.station = id();
        else if (!std::strcmp(k, "id"))
            r.id = id();
        else if (!std::strcmp(k, "path")) {
            if (scalar_ != CanonicalJsonToken::String || !checkedPath(text_))
                fail(Error::Path);
            r.artifact = intern(text_);
        } else if (!std::strcmp(k, "digest")) {
            if (!digest(r.digest))
                fail(Error::Value);
        } else if (!std::strcmp(k, "bytes")) {
            number(4294967295ULL, true);
            r.bytes = static_cast<std::uint32_t>(value);
        } else if (!std::strcmp(k, "verification")) {
            r.verification = equals("device_readback")      ? Verification::DeviceReadback
                             : equals("operator_inventory") ? Verification::OperatorInventory
                                                            : Verification::Invalid;
            if (r.verification == Verification::Invalid)
                fail(Error::Value);
        } else
            fail(Error::Type);
        return;
    }
    if (f.kind == Kind::Locator) {
        auto &r = catalog_->resources.data[f.index - 1];
        number(65535, true);
        if (!std::strcmp(k, "bank"))
            r.bank = static_cast<std::uint16_t>(value);
        else if (!std::strcmp(k, "track"))
            r.track = static_cast<std::uint16_t>(value);
        else if (!std::strcmp(k, "preset"))
            r.preset = static_cast<std::uint16_t>(value);
        else
            fail(Error::FieldSet);
        return;
    }
    if (f.kind == Kind::Rule) {
        auto &r = catalog_->rules.data[f.index - 1];
        if (!std::strcmp(k, "station"))
            r.station = id();
        else if (!std::strcmp(k, "ruleId")) {
            if (!identity(r.id))
                fail(Error::Value);
        } else if (!std::strcmp(k, "generation")) {
            number(4294967295ULL, true);
            r.generation = static_cast<std::uint32_t>(value);
        } else if (!std::strcmp(k, "expandedId")) {
            if (scalar_ != CanonicalJsonToken::String || length_ > 485 || !expandedName(text_))
                fail(Error::Value);
            r.expanded = intern(text_);
        } else
            fail(Error::Type);
        return;
    }
    if (f.kind == Kind::Offline) {
        auto &r = catalog_->rules.data[f.index - 1];
        if (!std::strcmp(k, "policy")) {
            r.policy = equals("wait")             ? 1
                       : equals("local_continue") ? 2
                       : equals("local_fallback") ? 3
                                                  : 0;
            if (!r.policy)
                fail(Error::Value);
        } else if (!std::strcmp(k, "rule")) {
            if (scalar_ != CanonicalJsonToken::String || length_ > 485 || !expandedName(text_))
                fail(Error::Value);
            r.fallback = intern(text_);
        } else
            fail(Error::FieldSet);
        return;
    }
    if (f.kind == Kind::Dependency) {
        auto &d = catalog_->dependencies.data[f.index - 1];
        if (!std::strcmp(k, "station"))
            d.station = id();
        else if (!std::strcmp(k, "originStation"))
            d.origin = id();
        else if (!std::strcmp(k, "subject"))
            d.subject = id();
        else if (!std::strcmp(k, "ruleId")) {
            if (!identity(d.rule))
                fail(Error::Value);
        } else
            fail(Error::FieldSet);
        return;
    }
    if (f.kind == Kind::Requirement || f.kind == Kind::SourceRequirement) {
        auto &r = f.kind == Kind::Requirement ? catalog_->requirements.data[f.index - 1]
                                              : catalog_->contentRequirements.data[f.index - 1];
        if (!std::strcmp(k, "name"))
            r.name = id();
        else
            fail(Error::Type);
        return;
    }
    fail(Error::Type);
}
void Validator::close() {
    const auto &f = frames_[depth_ - 1];
    switch (f.kind) {
    case Kind::Root:
        fields(1023);
        break;
    case Kind::Station: {
        fields(255);
        auto &s = catalog_->stations.data[f.index - 1];
        s.bindingPath = path(s.bindingPath, s.binding, 0, false, BindingRole);
        break;
    }
    case Kind::Port:
        fields(15);
        break;
    case Kind::Subject:
        fields(3);
        break;
    case Kind::Profile: {
        fields(31);
        auto &p = catalog_->profiles.data[f.index - 1];
        p.artifact = path(p.artifact, p.digest, p.bytes, true,
                          p.format == ProfileFormat::Rules ? RulesRole : LegacyRole, p.station);
        break;
    }
    case Kind::Resource: {
        fields(127);
        auto &r = catalog_->resources.data[f.index - 1];
        r.artifact = path(r.artifact, r.digest, r.bytes, true, ResourceRole);
        break;
    }
    case Kind::Locator:
        if (f.seen != 2 && f.seen != 5)
            fail(Error::FieldSet);
        break;
    case Kind::Rule:
        fields(31);
        break;
    case Kind::Offline:
        fields(catalog_->rules.data[f.index - 1].policy == 3 ? 3 : 1);
        break;
    case Kind::Dependency:
        fields(15);
        break;
    case Kind::Requirement:
    case Kind::SourceRequirement:
        fields(3);
        break;
    default:
        break;
    }
}
bool Validator::resolve() {
    if (!equal(event_, selection_.event)) {
        fail(Error::BindingMismatch);
        return false;
    }
    for (std::size_t i = 0; i < catalog_->stations.count; ++i) {
        const auto &s = catalog_->stations.data[i];
        if (i && std::strcmp(string(catalog_->stations.data[i - 1].id), string(s.id)) >= 0)
            fail(Error::Order);
        for (std::size_t j = 0; j < i; ++j)
            if (equal(s.station, catalog_->stations.data[j].station))
                fail(Error::Conflict);
        if (!std::strcmp(string(s.id), selection_.station)) {
            if (!equal(s.box, selection_.box))
                fail(Error::BindingMismatch);
            selectedStation_ = static_cast<Id>(i + 1);
        }
    }
    if (!selectedStation_)
        fail(Error::Reference);
    for (std::size_t i = 0; i < catalog_->ports.count; ++i) {
        const auto &p = catalog_->ports.data[i];
        if (i) {
            const auto &old = catalog_->ports.data[i - 1];
            if (old.station == p.station && std::strcmp(string(old.id), string(p.id)) >= 0)
                fail(Error::Order);
        }
        for (unsigned j = 1; j < p.resourceCount; ++j)
            if (std::strcmp(string(catalog_->portResources.data[p.firstResource + j - 1]),
                            string(catalog_->portResources.data[p.firstResource + j])) >= 0)
                fail(Error::Order);
    }
    for (std::size_t i = 0; i < catalog_->subjects.count; ++i) {
        const auto &s = catalog_->subjects.data[i];
        if (i) {
            const auto &old = catalog_->subjects.data[i - 1];
            if (old.station == s.station && std::strcmp(string(old.id), string(s.id)) >= 0)
                fail(Error::Order);
        }
        for (std::size_t j = 0; j < i; ++j)
            if (equal(s.subject, catalog_->subjects.data[j].subject))
                fail(Error::Conflict);
    }
    for (std::size_t i = 0; i < catalog_->profiles.count; ++i) {
        auto &p = catalog_->profiles.data[i];
        p.station = findStation(p.station);
        if (!p.station || !p.artifact) {
            fail(Error::Reference);
            return false;
        }
        auto &a = catalog_->artifacts.data[p.artifact - 1];
        if (p.format == ProfileFormat::Rules) {
            a.irStation = p.station;
            for (std::size_t j = 0; j < i; ++j)
                if (catalog_->profiles.data[j].station == p.station &&
                    catalog_->profiles.data[j].format == ProfileFormat::Rules)
                    fail(Error::Conflict);
        }
        if (i) {
            const auto &old = catalog_->profiles.data[i - 1];
            const int order = std::strcmp(string(catalog_->stations.data[old.station - 1].id),
                                          string(catalog_->stations.data[p.station - 1].id));
            if (order > 0 ||
                (!order && std::strcmp(string(catalog_->artifacts.data[old.artifact - 1].path),
                                       string(a.path)) >= 0))
                fail(Error::Order);
        }
    }
    for (std::size_t s = 0; s < catalog_->stations.count; ++s) {
        unsigned count = 0;
        for (std::size_t i = 0; i < catalog_->profiles.count; ++i)
            count += catalog_->profiles.data[i].station == s + 1 &&
                     catalog_->profiles.data[i].format == ProfileFormat::Rules;
        if (count != 1)
            fail(Error::Reference);
    }
    for (std::size_t i = 0; i < catalog_->resources.count; ++i) {
        auto &r = catalog_->resources.data[i];
        r.station = findStation(r.station);
        if (!r.station || !r.artifact) {
            fail(Error::Reference);
            return false;
        }
        if (i) {
            const auto &old = catalog_->resources.data[i - 1];
            const int order = std::strcmp(string(catalog_->stations.data[old.station - 1].id),
                                          string(catalog_->stations.data[r.station - 1].id));
            if (order > 0 || (!order && std::strcmp(string(old.id), string(r.id)) >= 0))
                fail(Error::Order);
        }
    }
    for (std::size_t i = 0; i < catalog_->rules.count; ++i) {
        auto &r = catalog_->rules.data[i];
        r.station = findStation(r.station);
        if (!r.station) {
            fail(Error::Reference);
            return false;
        }
        if (r.policy == 3) {
            fail(Error::Unsupported);
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            const auto &old = catalog_->rules.data[j];
            if (old.station == r.station && equal(old.id, r.id))
                fail(Error::Conflict);
        }
        if (i) {
            const auto &old = catalog_->rules.data[i - 1];
            const int order = std::strcmp(string(catalog_->stations.data[old.station - 1].id),
                                          string(catalog_->stations.data[r.station - 1].id));
            if (order > 0 || (!order && std::strcmp(string(old.expanded), string(r.expanded)) >= 0))
                fail(Error::Order);
        }
    }
    for (std::size_t i = 0; i < catalog_->dependencies.count; ++i) {
        auto &d = catalog_->dependencies.data[i];
        d.station = findStation(d.station);
        d.origin = findStation(d.origin);
        if (!d.station || !d.origin) {
            fail(Error::Reference);
            return false;
        }
        bool rule = false;
        for (std::size_t j = 0; j < catalog_->rules.count; ++j)
            rule |= catalog_->rules.data[j].station == d.station &&
                    equal(catalog_->rules.data[j].id, d.rule);
        if (!rule)
            fail(Error::Reference);
        if (i) {
            const auto &old = catalog_->dependencies.data[i - 1];
            int order = std::strcmp(string(catalog_->stations.data[old.station - 1].id),
                                    string(catalog_->stations.data[d.station - 1].id));
            if (!order)
                order = std::memcmp(old.rule.bytes, d.rule.bytes, 16);
            if (!order)
                order = std::strcmp(string(catalog_->stations.data[old.origin - 1].id),
                                    string(catalog_->stations.data[d.origin - 1].id));
            if (!order)
                order = std::strcmp(string(old.subject), string(d.subject));
            if (order >= 0)
                fail(Error::Order);
        }
    }
    for (std::size_t i = 1; i < catalog_->requirements.count; ++i)
        if (std::strcmp(string(catalog_->requirements.data[i - 1].name),
                        string(catalog_->requirements.data[i].name)) >= 0)
            fail(Error::Order);
    for (std::size_t i = 0; i < catalog_->artifacts.count; ++i) {
        const auto &a = catalog_->artifacts.data[i];
        if (((a.roles & BindingRole) && (a.roles & (RulesRole | LegacyRole))) ||
            ((a.roles & RulesRole) && (a.roles & LegacyRole)))
            fail(Error::ProfileMismatch);
    }
    return result_.error == Error::None;
}
bool Validator::hashBegin() {
    hashBuffered_ = 0;
    profileHashFailed_ = !hash_->begin();
    if (profileHashFailed_)
        fail(Error::CryptoFailure);
    return !profileHashFailed_;
}
bool Validator::hashByte(std::uint8_t byte) {
    if (profileHashFailed_)
        return false;
    hashBuffer_[hashBuffered_++] = byte;
    if (hashBuffered_ == sizeof(hashBuffer_)) {
        if (!hash_->update(hashBuffer_, hashBuffered_)) {
            profileHashFailed_ = true;
            fail(Error::CryptoFailure);
        }
        hashBuffered_ = 0;
    }
    return !profileHashFailed_;
}
bool Validator::hashEnd(Digest &out) {
    std::memset(out.bytes, 0, 32);
    if (!profileHashFailed_ && hashBuffered_ && !hash_->update(hashBuffer_, hashBuffered_))
        profileHashFailed_ = true;
    std::size_t written = 0;
    const bool complete = !profileHashFailed_ && hash_->finish(out.bytes, written) && written == 32;
    const bool clean = hash_->abort();
    hashBuffered_ = 0;
    if (!complete || !clean) {
        std::memset(out.bytes, 0, 32);
        fail(Error::CryptoFailure);
        return false;
    }
    return true;
}
bool Validator::readBindings() {
    for (std::size_t i = 0; i < catalog_->stations.count && result_.error == Error::None; ++i)
        readBinding(static_cast<Id>(i + 1));
    return result_.error == Error::None;
}
bool Validator::readBinding(Id stationId) {
    auto &station = catalog_->stations.data[stationId - 1];
    currentArtifact_ = station.bindingPath;
    source_ = InputSource::Artifact;
    diagnostic_[0] = 0;
    result_.module = 0;
    offset_ = 0;
    auto &artifact = catalog_->artifacts.data[currentArtifact_ - 1];
    if (artifact.opened) {
        fail(Error::BindingMismatch);
        return false;
    }
    artifact.opened = true;
    configuration::IPackageByteSource *source = nullptr;
    if (!provider_->open(string(artifact.path), source) || !source) {
        if (source)
            provider_->close(*source);
        fail(Error::SourceFailure);
        return false;
    }
    hashBegin();
    struct Reader {
        Reader(Validator &owner, configuration::IPackageByteSource &source) : v(owner), s(source) {}
        Validator &v;
        configuration::IPackageByteSource &s;
        bool ended = false;
        bool byte(std::uint8_t &b) {
            const auto status = s.readByte(b);
            if (status == PackageReadStatus::Failure) {
                ended = true;
                v.fail(Error::SourceFailure);
                return false;
            }
            if (status == PackageReadStatus::End) {
                ended = true;
                v.fail(Error::CanonicalInput);
                return false;
            }
            ++v.offset_;
            v.hashByte(b);
            if (v.offset_ > v.limits_.bindingBytes) {
                v.fail(Error::OperatingProfile);
                return false;
            }
            return true;
        }
        bool head(unsigned major, std::uint64_t &value) {
            std::uint8_t b = 0;
            if (!byte(b))
                return false;
            if (static_cast<unsigned>(b >> 5) != major) {
                v.fail(Error::CanonicalInput);
                return false;
            }
            const unsigned info = b & 31;
            if (info < 24) {
                value = info;
                return true;
            }
            const unsigned count = info == 24   ? 1
                                   : info == 25 ? 2
                                   : info == 26 ? 4
                                   : info == 27 ? 8
                                                : 0;
            if (!count) {
                v.fail(Error::CanonicalInput);
                return false;
            }
            value = 0;
            for (unsigned i = 0; i < count; ++i) {
                if (!byte(b))
                    return false;
                value = (value << 8) | b;
            }
            const std::uint64_t minimum = count == 1   ? 24
                                          : count == 2 ? 256
                                          : count == 4 ? 65536
                                                       : 4294967296ULL;
            if (value < minimum) {
                v.fail(Error::CanonicalInput);
                return false;
            }
            return true;
        }
        bool id(Identity &out) {
            std::uint64_t size = 0;
            if (!head(2, size))
                return false;
            if (size != 16) {
                v.fail(Error::CanonicalInput);
                return false;
            }
            for (auto &b : out.bytes)
                if (!byte(b))
                    return false;
            if (!nonzero(out)) {
                v.fail(Error::Value);
                return false;
            }
            return true;
        }
        bool literal(unsigned major, std::uint64_t expected) {
            std::uint64_t value = 0;
            if (!head(major, value))
                return false;
            if (value != expected) {
                v.fail(Error::CanonicalInput);
                return false;
            }
            return true;
        }
        void tail(bool valid) {
            while (!ended && v.offset_ <= v.limits_.bindingBytes) {
                std::uint8_t b = 0;
                const auto status = s.readByte(b);
                if (status == PackageReadStatus::End) {
                    ended = true;
                    return;
                }
                if (status == PackageReadStatus::Failure) {
                    ended = true;
                    v.fail(Error::SourceFailure);
                    return;
                }
                ++v.offset_;
                v.hashByte(b);
                v.fail(valid ? Error::CanonicalInput : Error::CanonicalInput);
                if (v.offset_ > v.limits_.bindingBytes) {
                    v.fail(Error::OperatingProfile);
                    return;
                }
            }
        }
    } reader(*this, *source);
    bool parsed = reader.literal(5, 8);
    for (unsigned key = 0; key < 8 && parsed; ++key) {
        parsed = reader.literal(0, key);
        if (!parsed)
            break;
        std::uint64_t value = 0;
        Identity id;
        if (key == 0)
            parsed = reader.literal(0, 1);
        else if (key == 1) {
            parsed = reader.id(id);
            if (parsed && !equal(id, event_))
                fail(Error::BindingMismatch);
        } else if (key == 2) {
            parsed = reader.id(id);
            if (parsed && !equal(id, station.station))
                fail(Error::BindingMismatch);
        } else if (key == 3) {
            parsed = reader.head(0, value);
            if (parsed && (!value || value != station.generation))
                fail(Error::BindingMismatch);
        } else if (key == 4) {
            parsed = reader.id(id);
            if (parsed && !equal(id, station.box))
                fail(Error::BindingMismatch);
        } else if (key == 5)
            parsed = reader.id(station.ruleset);
        else if (key == 6) {
            parsed = reader.head(4, value);
            station.firstEpoch = static_cast<std::uint32_t>(catalog_->epochs.count);
            if (parsed &&
                (value > 65534 || value > catalog_->epochs.capacity - catalog_->epochs.count)) {
                fail(Error::OperatingProfile);
                parsed = false;
            }
            Identity previous;
            for (std::uint64_t j = 0; j < value && parsed; ++j) {
                parsed = reader.id(id);
                if (parsed) {
                    if (j && std::memcmp(previous.bytes, id.bytes, 16) >= 0)
                        fail(Error::Order);
                    const Id at = append(catalog_->epochs);
                    if (!at) {
                        parsed = false;
                        break;
                    }
                    catalog_->epochs.data[at - 1] = id;
                    previous = id;
                    ++station.epochCount;
                }
            }
        } else if (key == 7) {
            parsed = reader.head(4, value);
            station.firstPermission = static_cast<std::uint32_t>(catalog_->permissions.count);
            if (parsed && (value > 65534 ||
                           value > catalog_->permissions.capacity - catalog_->permissions.count)) {
                fail(Error::OperatingProfile);
                parsed = false;
            }
            Permission previous;
            for (std::uint64_t j = 0; j < value && parsed; ++j) {
                parsed = reader.literal(4, 2);
                std::uint64_t type = 0;
                if (parsed)
                    parsed = reader.head(0, type);
                if (parsed && (type < 1 || type > 3)) {
                    fail(Error::Value);
                    parsed = false;
                }
                if (parsed)
                    parsed = reader.id(id);
                if (parsed) {
                    if (type == 3 && !equal(id, station.box))
                        fail(Error::BindingMismatch);
                    if (j && (type < previous.type ||
                              (type == previous.type &&
                               std::memcmp(previous.subject.bytes, id.bytes, 16) >= 0)))
                        fail(Error::Order);
                    const Id at = append(catalog_->permissions);
                    if (!at) {
                        parsed = false;
                        break;
                    }
                    auto &p = catalog_->permissions.data[at - 1];
                    p.station = stationId;
                    p.type = static_cast<std::uint8_t>(type);
                    p.subject = id;
                    previous = p;
                    ++station.permissionCount;
                }
            }
        }
        if (result_.error != Error::None)
            parsed = false;
    }
    reader.tail(parsed);
    artifact.actualBytes = offset_;
    result_.artifactBytes += offset_;
    if (result_.error == Error::None && artifact.hasLength &&
        artifact.actualBytes != artifact.bytes)
        fail(Error::LengthMismatch);
    if (result_.error == Error::None) {
        if (hashEnd(artifact.actual) && !equal(artifact.actual, artifact.expected))
            fail(Error::DigestMismatch);
    } else
        hash_->abort();
    provider_->close(*source);
    artifact.verified = result_.error == Error::None;
    if (stationId == selectedStation_ && result_.error == Error::None) {
        bool allowed = false;
        for (unsigned i = 0; i < station.epochCount; ++i)
            allowed |= equal(catalog_->epochs.data[station.firstEpoch + i], selection_.epoch);
        if (!allowed)
            fail(Error::BindingMismatch);
    }
    return result_.error == Error::None;
}
bool Validator::hashArtifact(Id index) {
    auto &a = catalog_->artifacts.data[index - 1];
    if (a.verified)
        return true;
    currentArtifact_ = index;
    source_ = InputSource::Artifact;
    diagnostic_[0] = 0;
    result_.module = 0;
    offset_ = 0;
    if (a.opened) {
        fail(Error::Conflict);
        return false;
    }
    a.opened = true;
    configuration::IPackageByteSource *source = nullptr;
    if (!provider_->open(string(a.path), source) || !source) {
        if (source)
            provider_->close(*source);
        fail(Error::SourceFailure);
        return false;
    }
    hashBegin();
    bool ended = false;
    while (!ended) {
        std::uint8_t b = 0;
        const auto status = source->readByte(b);
        if (status == PackageReadStatus::End) {
            ended = true;
            break;
        }
        if (status == PackageReadStatus::Failure) {
            ended = true;
            fail(Error::SourceFailure);
            break;
        }
        ++offset_;
        hashByte(b);
        if (a.hasLength && offset_ > a.bytes) {
            fail(Error::LengthMismatch);
            break;
        }
    }
    a.actualBytes = offset_;
    result_.artifactBytes += offset_;
    if (ended && result_.error == Error::None && a.hasLength && a.bytes != offset_)
        fail(Error::LengthMismatch);
    if (ended && result_.error == Error::None) {
        if (hashEnd(a.actual) && !equal(a.actual, a.expected))
            fail(Error::DigestMismatch);
    } else
        hash_->abort();
    provider_->close(*source);
    a.verified = result_.error == Error::None;
    return a.verified;
}
bool Validator::legacyArtifact(Id index) {
    auto &a = catalog_->artifacts.data[index - 1];
    if (a.verified && a.legacy)
        return true;
    currentArtifact_ = index;
    source_ = InputSource::Artifact;
    diagnostic_[0] = 0;
    result_.module = 0;
    offset_ = 0;
    if (a.opened) {
        fail(Error::Conflict);
        return false;
    }
    a.opened = true;
    configuration::IPackageByteSource *source = nullptr;
    if (!provider_->open(string(a.path), source) || !source) {
        if (source)
            provider_->close(*source);
        fail(Error::SourceFailure);
        return false;
    }
    hashBegin();
    class LegacySource final : public configuration::IProfileReader {
      public:
        LegacySource(Validator &v, configuration::IPackageByteSource &s) : v_(v), s_(s) {}
        bool ended = false;
        configuration::ReadStatus readByte(std::uint8_t &byte) override {
            const auto status = s_.readByte(byte);
            if (status == PackageReadStatus::Failure) {
                ended = true;
                v_.fail(Error::SourceFailure);
                return configuration::ReadStatus::Failure;
            }
            if (status == PackageReadStatus::End) {
                ended = true;
                return configuration::ReadStatus::End;
            }
            ++v_.offset_;
            v_.hashByte(byte);
            return configuration::ReadStatus::Byte;
        }

      private:
        Validator &v_;
        configuration::IPackageByteSource &s_;
    } legacy(*this, *source);
    const auto imported = configuration::readDraftProfile(legacy, *legacy_);
    if (!imported)
        fail(imported.error == configuration::ImportError::InputFailure ? Error::SourceFailure
                                                                        : Error::LegacyProfile);
    while (!legacy.ended && offset_ <= a.bytes &&
           offset_ <= configuration::ImportLimits::DocumentBytes) {
        std::uint8_t byte = 0;
        legacy.readByte(byte);
    }
    a.actualBytes = offset_;
    result_.artifactBytes += offset_;
    if (result_.error == Error::None && a.hasLength && a.bytes != offset_)
        fail(Error::LengthMismatch);
    if (result_.error == Error::None) {
        if (hashEnd(a.actual) && !equal(a.actual, a.expected))
            fail(Error::DigestMismatch);
    } else
        hash_->abort();
    provider_->close(*source);
    a.verified = result_.error == Error::None;
    a.legacy = a.verified;
    return a.verified;
}
bool Validator::contentCatalog(const configuration::ModuleCompileWorkspace &s) {
    if (contentCatalog_)
        return true;
    contentCatalog_ = true;
    std::size_t stations = 0;
    for (std::size_t i = 0; i < s.definitions.count; ++i) {
        const auto &d = s.definitions.data[i];
        if (d.module)
            continue;
        ++stations;
        const Id id = intern(sourceName(s, d.id));
        if (!findStation(id)) {
            where("content/stations", i, 0);
            fail(Error::Reference);
        }
    }
    if (stations != catalog_->stations.count)
        fail(Error::Reference);
    for (std::size_t i = 0; i < s.symbols.count && result_.error == Error::None; ++i) {
        const auto &v = s.symbols.data[i];
        if (v.kind != SymbolKind::Subject || v.owner)
            continue;
        const Id logical = intern(sourceName(s, v.id)),
                 origin = findStation(intern(sourceName(s, v.auxiliary)));
        if (!origin) {
            fail(Error::Reference);
            break;
        }
        const Id at = append(catalog_->contentSubjects);
        if (!at)
            break;
        auto &subject = catalog_->contentSubjects.data[at - 1];
        subject.id = logical;
        subject.station = origin;
        unsigned matches = 0;
        for (std::size_t j = 0; j < catalog_->subjects.count; ++j) {
            auto &mapping = catalog_->subjects.data[j];
            if (mapping.station == origin && mapping.id == logical) {
                mapping.used = true;
                ++matches;
            }
        }
        if (matches != 1) {
            where("content/subjects", i, v.location.offset);
            fail(Error::Reference);
        }
    }
    for (std::size_t i = 0; i < catalog_->subjects.count && result_.error == Error::None; ++i)
        if (!catalog_->subjects.data[i].used) {
            where("subjects", i, catalog_->subjects.data[i].offset);
            fail(Error::Reference);
        }
    for (std::size_t i = 0; i < catalog_->permissions.count && result_.error == Error::None; ++i) {
        const auto &p = catalog_->permissions.data[i];
        if (p.type == 3)
            continue;
        bool known = false;
        for (std::size_t j = 0; j < catalog_->subjects.count; ++j)
            known |= catalog_->subjects.data[j].station == p.station &&
                     equal(catalog_->subjects.data[j].subject, p.subject);
        if (!known) {
            where("bindingPermissions", i, p.offset,
                  catalog_->stations.data[p.station - 1].bindingPath);
            fail(Error::OperatingProfile);
        }
    }
    for (std::size_t i = 0; i < catalog_->contentRequirements.count && result_.error == Error::None;
         ++i) {
        const auto &required = catalog_->contentRequirements.data[i];
        bool found = false;
        for (std::size_t j = 0; j < catalog_->requirements.count; ++j) {
            const auto &deployed = catalog_->requirements.data[j];
            if (deployed.name == required.name) {
                found = true;
                if (std::strcmp(string(deployed.major), string(required.major))) {
                    where("requires", j, deployed.offset);
                    fail(Error::Reference);
                }
            }
        }
        if (!found) {
            where("content/requires", i, required.offset);
            fail(Error::Reference);
        }
    }
    return result_.error == Error::None && contentResources(s);
}
bool Validator::contentResources(const configuration::ModuleCompileWorkspace &s) {
    for (std::size_t i = 0; i < s.symbols.count && result_.error == Error::None; ++i) {
        const auto &v = s.symbols.data[i];
        if (v.kind != SymbolKind::Resource || v.owner)
            continue;
        const Id logical = intern(sourceName(s, v.id));
        Digest expected;
        if (!v.resourceBytes || !digestText(sourceName(s, v.resourceDigest), expected)) {
            fail(Error::Reference);
            break;
        }
        bool mapped = false;
        for (std::size_t j = 0; j < catalog_->resources.count; ++j) {
            auto &r = catalog_->resources.data[j];
            if (r.id != logical)
                continue;
            mapped = true;
            r.content = true;
            r.kind = v.type;
            where("resources", j, r.offset);
            if (!equal(r.digest, expected) || r.bytes != v.resourceBytes)
                fail(Error::DigestMismatch);
            if ((v.type == Type::Audio && (!r.bank || !r.track || r.preset)) ||
                (v.type == Type::Light && (!r.preset || r.bank || r.track)))
                fail(Error::Type);
        }
        if (mapped || result_.error != Error::None)
            continue;
        currentArtifact_ = 0;
        source_ = InputSource::ContentResource;
        result_.module = 0;
        result_.contentResource = logical;
        offset_ = 0;
        std::snprintf(diagnostic_, sizeof(diagnostic_), "/content/resources/%s", string(logical));
        ContentResourcePin pin;
        pin.id = string(logical);
        pin.digest = expected;
        pin.bytes = v.resourceBytes;
        pin.kind = v.type;
        configuration::IPackageByteSource *source = nullptr;
        if (!provider_->openContentResource(pin, source) || !source) {
            if (source)
                provider_->close(*source);
            fail(Error::SourceFailure);
            break;
        }
        hashBegin();
        bool ended = false;
        while (!ended) {
            std::uint8_t b = 0;
            const auto status = source->readByte(b);
            if (status == PackageReadStatus::End) {
                ended = true;
                break;
            }
            if (status == PackageReadStatus::Failure) {
                ended = true;
                fail(Error::SourceFailure);
                break;
            }
            ++offset_;
            hashByte(b);
            if (offset_ > pin.bytes) {
                fail(Error::LengthMismatch);
                break;
            }
        }
        result_.artifactBytes += offset_;
        Digest actual;
        if (ended && result_.error == Error::None && offset_ != pin.bytes)
            fail(Error::LengthMismatch);
        if (ended && result_.error == Error::None) {
            if (hashEnd(actual) && !equal(actual, expected))
                fail(Error::DigestMismatch);
        } else
            hash_->abort();
        provider_->close(*source);
        if (result_.error == Error::None)
            result_.contentResource = 0;
    }
    for (std::size_t i = 0; i < catalog_->resources.count && result_.error == Error::None; ++i)
        if (!catalog_->resources.data[i].content) {
            where("resources", i, catalog_->resources.data[i].offset);
            fail(Error::Reference);
        }
    return result_.error == Error::None;
}
namespace {
bool qualified(const configuration::ModuleCompileWorkspace &s,
               const configuration::module_ir::Name &n, char text[486]) {
    std::size_t used = 0;
    if (!n.count || n.count > 5)
        return false;
    for (unsigned i = 0; i < n.count; ++i) {
        const char *segment = sourceName(s, n.segments[i]);
        if (!*segment || std::strlen(segment) > 48)
            return false;
        text[used++] = '/';
        for (; *segment; ++segment) {
            if (*segment == '/' || *segment == '~') {
                text[used++] = '~';
                text[used++] = *segment == '/' ? '1' : '0';
            } else
                text[used++] = *segment;
        }
    }
    text[used] = 0;
    return true;
}
} // namespace
bool Validator::expanded(const configuration::ModuleCompileWorkspace &s,
                         const configuration::ExpandedStationWorkspace &e, Id definition) {
    if (!contentCatalog(s))
        return false;
    const Id stationId = findStation(intern(sourceName(s, s.definitions.data[definition - 1].id)));
    if (!stationId) {
        fail(Error::Reference);
        return false;
    }
    auto &station = catalog_->stations.data[stationId - 1];
    if (station.generated) {
        fail(Error::Conflict);
        return false;
    }
    station.generated = true;
    for (std::size_t i = 0; i < s.symbols.count && result_.error == Error::None; ++i) {
        const auto &v = s.symbols.data[i];
        if (v.owner != definition || v.kind != SymbolKind::Port)
            continue;
        const Id logical = intern(sourceName(s, v.id));
        unsigned found = 0;
        for (std::size_t j = 0; j < catalog_->ports.count; ++j) {
            auto &p = catalog_->ports.data[j];
            if (p.station != stationId || p.id != logical)
                continue;
            p.used = true;
            ++found;
            where("ports", j, p.offset);
            for (unsigned k = 0; k < p.resourceCount; ++k) {
                const Id resource = catalog_->portResources.data[p.firstResource + k];
                bool mapped = false;
                for (std::size_t r = 0; r < catalog_->resources.count; ++r) {
                    auto &row = catalog_->resources.data[r];
                    if (row.station == stationId && row.id == resource) {
                        mapped = true;
                        row.used = true;
                        if (v.input || row.kind != v.type)
                            fail(Error::Type);
                    }
                }
                if (!mapped)
                    fail(Error::Reference);
            }
        }
        if (found != 1) {
            where("stations", stationId - 1, station.offset);
            fail(Error::Reference);
        }
    }
    for (std::size_t i = 0; i < catalog_->ports.count && result_.error == Error::None; ++i)
        if (catalog_->ports.data[i].station == stationId && !catalog_->ports.data[i].used) {
            where("ports", i, catalog_->ports.data[i].offset);
            fail(Error::Reference);
        }
    for (std::size_t i = 0; i < e.rules.count && result_.error == Error::None; ++i) {
        char text[486];
        if (!qualified(s, e.rules.data[i].name, text)) {
            fail(Error::Reference);
            break;
        }
        const Id logical = intern(text);
        const auto &r = e.rules.data[i].rule;
        unsigned found = 0;
        for (std::size_t j = 0; j < catalog_->rules.count; ++j) {
            auto &row = catalog_->rules.data[j];
            if (row.station != stationId || row.expanded != logical)
                continue;
            ++found;
            row.used = true;
            where("rules", j, row.offset);
            if (row.generation != r.generation || row.policy != r.policy || row.fallback)
                fail(Error::BindingMismatch);
            auto permission = [&](Id subject, bool emit) {
                const Id subjectName = intern(sourceName(s, subject));
                Id origin = 0;
                Identity actual;
                for (std::size_t k = 0; k < catalog_->contentSubjects.count; ++k)
                    if (catalog_->contentSubjects.data[k].id == subjectName)
                        origin = catalog_->contentSubjects.data[k].station;
                for (std::size_t k = 0; k < catalog_->subjects.count; ++k)
                    if (catalog_->subjects.data[k].station == origin &&
                        catalog_->subjects.data[k].id == subjectName)
                        actual = catalog_->subjects.data[k].subject;
                bool allowed = false;
                for (std::size_t k = 0; k < catalog_->permissions.count; ++k)
                    allowed |= catalog_->permissions.data[k].station == origin &&
                               (catalog_->permissions.data[k].type == 1 ||
                                (!emit && catalog_->permissions.data[k].type == 2)) &&
                               equal(catalog_->permissions.data[k].subject, actual);
                if (!origin || !allowed)
                    fail(Error::BindingMismatch);
            };
            for (unsigned k = 0; k < r.triggerCount; ++k)
                if (r.trigger == 3)
                    permission(e.triggers.data[r.firstTrigger + k], false);
            for (unsigned k = 0; k < r.actionCount; ++k) {
                const auto &a = e.actions.data[r.firstAction + k];
                if (a.op == configuration::module_ir::Op::Emit) {
                    permission(a.target, true);
                    if (!r.once)
                        fail(Error::Reference);
                    for (std::size_t prior = 0; prior < i; ++prior) {
                        const auto &other = e.rules.data[prior].rule;
                        for (unsigned l = 0; l < other.actionCount; ++l) {
                            const auto &b = e.actions.data[other.firstAction + l];
                            if (b.op == configuration::module_ir::Op::Emit && b.target == a.target)
                                fail(Error::Conflict);
                        }
                    }
                }
                if (a.op == configuration::module_ir::Op::Effect) {
                    const Id portName = intern(sourceName(s, a.target)),
                             resource = intern(sourceName(s, a.resource));
                    bool allowed = false;
                    for (std::size_t p = 0; p < catalog_->ports.count; ++p) {
                        const auto &port = catalog_->ports.data[p];
                        if (port.station != stationId || port.id != portName)
                            continue;
                        for (unsigned x = 0; x < port.resourceCount; ++x)
                            allowed |=
                                catalog_->portResources.data[port.firstResource + x] == resource;
                        for (std::size_t prior = 0; prior < i; ++prior) {
                            const auto &other = e.rules.data[prior].rule;
                            if (r.trigger != 3 && other.trigger != 3)
                                continue;
                            for (unsigned l = 0; l < other.actionCount; ++l) {
                                const auto &b = e.actions.data[other.firstAction + l];
                                if (b.op != configuration::module_ir::Op::Effect)
                                    continue;
                                const Id otherName = intern(sourceName(s, b.target));
                                for (std::size_t q = 0; q < catalog_->ports.count; ++q)
                                    if (catalog_->ports.data[q].station == stationId &&
                                        catalog_->ports.data[q].id == otherName &&
                                        catalog_->ports.data[q].component == port.component)
                                        fail(Error::Conflict);
                            }
                        }
                    }
                    if (!allowed)
                        fail(Error::Reference);
                }
            }
        }
        if (found != 1) {
            where("stations", stationId - 1, station.offset);
            fail(Error::Reference);
        }
    }
    // Every fact expression, including local rules and otherwise unused branches,
    // needs an actual permitted originating subject.
    for (std::size_t i = 0; i < e.nodes.count && result_.error == Error::None; ++i)
        if (e.nodes.data[i].op == configuration::module_ir::Op::Fact) {
            const Id logical = intern(sourceName(s, e.nodes.data[i].ref));
            Id origin = 0;
            Identity subject;
            for (std::size_t j = 0; j < catalog_->contentSubjects.count; ++j)
                if (catalog_->contentSubjects.data[j].id == logical)
                    origin = catalog_->contentSubjects.data[j].station;
            for (std::size_t j = 0; j < catalog_->subjects.count; ++j)
                if (catalog_->subjects.data[j].station == origin &&
                    catalog_->subjects.data[j].id == logical)
                    subject = catalog_->subjects.data[j].subject;
            bool permitted = false;
            for (std::size_t j = 0; j < catalog_->permissions.count; ++j)
                permitted |= catalog_->permissions.data[j].station == origin &&
                             (catalog_->permissions.data[j].type == 1 ||
                              catalog_->permissions.data[j].type == 2) &&
                             equal(catalog_->permissions.data[j].subject, subject);
            if (!origin || !permitted)
                fail(Error::BindingMismatch);
        }
    if (result_.error != Error::None || !dependencies(s, e, stationId))
        return false;
    if (stationId != selectedStation_)
        return true;
    for (std::size_t i = 0; i < catalog_->rules.count; ++i) {
        const auto &row = catalog_->rules.data[i];
        if (row.station != stationId)
            continue;
        const Id at = append(catalog_->runtimeRules);
        if (!at)
            return false;
        auto &binding = catalog_->runtimeRules.data[at - 1];
        binding.qualified = string(row.expanded);
        binding.ruleId = row.id;
    }
    for (std::size_t i = 0; i < catalog_->contentSubjects.count; ++i) {
        const auto &content = catalog_->contentSubjects.data[i];
        const auto &origin = catalog_->stations.data[content.station - 1];
        Identity subject;
        for (std::size_t j = 0; j < catalog_->subjects.count; ++j)
            if (catalog_->subjects.data[j].station == content.station &&
                catalog_->subjects.data[j].id == content.id)
                subject = catalog_->subjects.data[j].subject;
        const Id at = append(catalog_->runtimeSubjects);
        if (!at)
            return false;
        auto &binding = catalog_->runtimeSubjects.data[at - 1];
        binding.name = string(content.id);
        binding.subject = subject;
        binding.station = origin.station;
        binding.box = origin.box;
        binding.binding = origin.binding;
        binding.epochs.data =
            origin.epochCount ? catalog_->epochs.data + origin.firstEpoch : nullptr;
        binding.epochs.count = origin.epochCount;
    }
    for (std::size_t i = 0; i < catalog_->ports.count; ++i) {
        const auto &row = catalog_->ports.data[i];
        if (row.station != stationId)
            continue;
        bool output = false;
        for (std::size_t j = 0; j < s.symbols.count; ++j) {
            const auto &v = s.symbols.data[j];
            if (v.owner == definition && v.kind == SymbolKind::Port &&
                !std::strcmp(sourceName(s, v.id), string(row.id)))
                output = !v.input;
        }
        if (!output)
            continue;
        const Id at = append(catalog_->runtimePorts);
        if (!at)
            return false;
        auto &binding = catalog_->runtimePorts.data[at - 1];
        binding.name = string(row.id);
        binding.exclusive.bytes[14] = static_cast<std::uint8_t>(row.component >> 8);
        binding.exclusive.bytes[15] = static_cast<std::uint8_t>(row.component);
        const auto first = catalog_->runtimeResources.count;
        for (unsigned j = 0; j < row.resourceCount; ++j) {
            const Id r = append(catalog_->runtimeResources);
            if (!r)
                return false;
            catalog_->runtimeResources.data[r - 1] =
                string(catalog_->portResources.data[row.firstResource + j]);
        }
        binding.resources.data =
            row.resourceCount ? catalog_->runtimeResources.data + first : nullptr;
        binding.resources.count = row.resourceCount;
    }
    rules::Context context;
    context.event = event_;
    context.run = run_;
    context.box = station.box;
    context.station = station.station;
    context.binding = station.binding;
    context.deployment = result_.deployment;
    context.content = content_;
    context.sourceEpoch = selection_.epoch;
    rules::Span<rules::RuleBinding> rb;
    rb.data = catalog_->runtimeRules.data;
    rb.count = catalog_->runtimeRules.count;
    rules::Span<rules::SubjectBinding> sb;
    sb.data = catalog_->runtimeSubjects.data;
    sb.count = catalog_->runtimeSubjects.count;
    rules::Span<rules::PortBinding> pb;
    pb.data = catalog_->runtimePorts.data;
    pb.count = catalog_->runtimePorts.count;
    builder_ = new (&builderStorage_)
        rules::ProgramBuilder(*program_, selection_.station, context, rb, sb, pb);
    if (!builder_->begin(result_.content.bytes) || !builder_->station(s, e, definition)) {
        result_.program = builder_->result();
        fail(Error::Program);
        return false;
    }
    return true;
}
bool Validator::dependencies(const configuration::ModuleCompileWorkspace &s,
                             const configuration::ExpandedStationWorkspace &e, Id stationId) {
    const std::size_t words = (catalog_->contentSubjects.count + 63) / 64;
    if (!words)
        return true;
    const std::size_t rows = e.nodes.count + e.values.count + e.timers.count + e.rules.count + 2;
    if (words > std::numeric_limits<std::size_t>::max() / rows || !catalog_->closure.data ||
        rows * words > catalog_->closure.capacity) {
        fail(Error::OperatingProfile);
        return false;
    }
    catalog_->closure.count = rows * words;
    std::memset(catalog_->closure.data, 0, catalog_->closure.count * sizeof(std::uint64_t));
    struct Analysis {
        Analysis(Validator &owner, const configuration::ModuleCompileWorkspace &source,
                 const configuration::ExpandedStationWorkspace &expanded, std::size_t count,
                 std::uint64_t *data)
            : v(owner), s(source), e(expanded), words(count), nodes(data),
              values(data + expanded.nodes.count * count),
              timers(data + (expanded.nodes.count + expanded.values.count) * count),
              rules(data + (expanded.nodes.count + expanded.values.count + expanded.timers.count) *
                               count) {}
        Validator &v;
        const configuration::ModuleCompileWorkspace &s;
        const configuration::ExpandedStationWorkspace &e;
        std::size_t words, work = 0;
        std::uint64_t *nodes;
        std::uint64_t *values;
        std::uint64_t *timers;
        std::uint64_t *rules;
        bool memo[512] = {};
        bool merge(std::uint64_t *to, const std::uint64_t *from) {
            bool changed = false;
            for (std::size_t i = 0; i < words; ++i) {
                if (++work > v.limits_.closureWork) {
                    v.fail(Error::OperatingProfile);
                    return false;
                }
                const auto next = to[i] | from[i];
                changed |= next != to[i];
                to[i] = next;
            }
            return changed;
        }
        void fact(std::uint64_t *to, Id ref) {
            const char *logical = sourceName(s, ref);
            for (std::size_t i = 0; i < v.catalog_->contentSubjects.count; ++i)
                if (!std::strcmp(v.string(v.catalog_->contentSubjects.data[i].id), logical)) {
                    to[i / 64] |= 1ULL << (i % 64);
                    return;
                }
            v.fail(Error::Reference);
        }
        const std::uint64_t *node(Id index) {
            auto *mask = nodes + (index - 1) * words;
            if (memo[index - 1])
                return mask;
            memo[index - 1] = true;
            const auto &n = e.nodes.data[index - 1];
            if (n.op == configuration::module_ir::Op::Fact)
                fact(mask, n.ref);
            else if (n.op == configuration::module_ir::Op::Value)
                merge(mask, values + (n.ref - 1) * words);
            for (unsigned i = 0; i < n.count; ++i)
                merge(mask, node(n.children[i]));
            return mask;
        }
    } analysis(*this, s, e, words, catalog_->closure.data);
    auto *base = analysis.rules + e.rules.count * words;
    auto *cause = base + words;
    bool changed = true;
    while (changed && result_.error == Error::None) {
        changed = false;
        std::memset(analysis.memo, 0, sizeof(analysis.memo));
        std::memset(analysis.nodes, 0, e.nodes.count * words * sizeof(std::uint64_t));
        for (std::size_t ri = 0; ri < e.rules.count && result_.error == Error::None; ++ri) {
            const auto &r = e.rules.data[ri].rule;
            std::memset(base, 0, words * sizeof(std::uint64_t));
            if (r.trigger == 3)
                for (unsigned j = 0; j < r.triggerCount; ++j)
                    analysis.fact(base, e.triggers.data[r.firstTrigger + j]);
            if (r.trigger == 2)
                analysis.merge(base, analysis.timers + (r.timer - 1) * words);
            analysis.merge(base, analysis.node(r.guard));
            changed |= analysis.merge(analysis.rules + ri * words, base);
            for (unsigned ai = 0; ai < r.actionCount; ++ai) {
                const auto &a = e.actions.data[r.firstAction + ai];
                std::memcpy(cause, base, words * sizeof(std::uint64_t));
                if (a.expr)
                    analysis.merge(cause, analysis.node(a.expr));
                if (a.op == configuration::module_ir::Op::Add)
                    analysis.merge(cause, analysis.values + (a.target - 1) * words);
                changed |= analysis.merge(analysis.rules + ri * words, cause);
                if (a.op == configuration::module_ir::Op::Set ||
                    a.op == configuration::module_ir::Op::Add)
                    changed |= analysis.merge(analysis.values + (a.target - 1) * words, cause);
                if (a.op == configuration::module_ir::Op::Arm)
                    changed |= analysis.merge(analysis.timers + (a.target - 1) * words, cause);
            }
        }
    }
    for (std::size_t ri = 0; ri < e.rules.count && result_.error == Error::None; ++ri) {
        char text[486];
        if (!qualified(s, e.rules.data[ri].name, text)) {
            fail(Error::Reference);
            break;
        }
        Rule *row = nullptr;
        for (std::size_t i = 0; i < catalog_->rules.count; ++i)
            if (catalog_->rules.data[i].station == stationId &&
                !std::strcmp(string(catalog_->rules.data[i].expanded), text))
                row = &catalog_->rules.data[i];
        if (!row) {
            fail(Error::Reference);
            break;
        }
        for (std::size_t subject = 0; subject < catalog_->contentSubjects.count; ++subject) {
            if (!(analysis.rules[ri * words + subject / 64] & (1ULL << (subject % 64))))
                continue;
            const auto &origin = catalog_->contentSubjects.data[subject];
            bool found = false;
            for (std::size_t i = 0; i < catalog_->dependencies.count; ++i) {
                auto &d = catalog_->dependencies.data[i];
                if (d.station == stationId && equal(d.rule, row->id) &&
                    d.origin == origin.station && d.subject == origin.id) {
                    found = true;
                    d.used = true;
                }
            }
            if (!found) {
                where("rules", static_cast<std::size_t>(row - catalog_->rules.data), row->offset);
                fail(Error::Reference);
            }
        }
    }
    return result_.error == Error::None;
}
bool Validator::prepare() {
    for (std::size_t i = 0; i < catalog_->stations.count && result_.error == Error::None; ++i)
        if (!catalog_->stations.data[i].generated) {
            where("stations", i, catalog_->stations.data[i].offset);
            fail(Error::Reference);
        }
    for (std::size_t i = 0; i < catalog_->rules.count && result_.error == Error::None; ++i)
        if (!catalog_->rules.data[i].used) {
            where("rules", i, catalog_->rules.data[i].offset);
            fail(Error::Reference);
        }
    for (std::size_t i = 0; i < catalog_->dependencies.count && result_.error == Error::None; ++i)
        if (!catalog_->dependencies.data[i].used) {
            where("dependencies", i, catalog_->dependencies.data[i].offset);
            fail(Error::Reference);
        }
    // Reuse caller scratch for a sorted catalog of actual locator uses. The
    // box/component key is physical; channel independence is not presumed.
    catalog_->closure.count = 0;
    auto compareUse = [&](std::uint64_t a, std::uint64_t b) {
        const auto &x = catalog_->resources.data[static_cast<Id>(a) - 1];
        const auto &y = catalog_->resources.data[static_cast<Id>(b) - 1];
        int order = std::memcmp(catalog_->stations.data[x.station - 1].box.bytes,
                                catalog_->stations.data[y.station - 1].box.bytes, 16);
        if (order)
            return order;
        const auto ca = a >> 32, cb = b >> 32;
        if (ca != cb)
            return ca < cb ? -1 : 1;
        if (x.kind != y.kind)
            return static_cast<unsigned>(x.kind) < static_cast<unsigned>(y.kind) ? -1 : 1;
        if (x.bank != y.bank)
            return x.bank < y.bank ? -1 : 1;
        if (x.track != y.track)
            return x.track < y.track ? -1 : 1;
        if (x.preset != y.preset)
            return x.preset < y.preset ? -1 : 1;
        return 0;
    };
    for (std::size_t p = 0; p < catalog_->ports.count && result_.error == Error::None; ++p) {
        const auto &port = catalog_->ports.data[p];
        for (unsigned r = 0; r < port.resourceCount && result_.error == Error::None; ++r) {
            const Id logical = catalog_->portResources.data[port.firstResource + r];
            for (std::size_t i = 0; i < catalog_->resources.count; ++i) {
                const auto &resource = catalog_->resources.data[i];
                if (resource.station != port.station || resource.id != logical)
                    continue;
                const std::uint64_t key =
                    (static_cast<std::uint64_t>(port.component) << 32) | (i + 1);
                std::size_t first = 0, last = catalog_->closure.count;
                while (first < last) {
                    const auto middle = (first + last) / 2;
                    if (compareUse(catalog_->closure.data[middle], key) < 0)
                        first = middle + 1;
                    else
                        last = middle;
                }
                if (first < catalog_->closure.count &&
                    !compareUse(catalog_->closure.data[first], key)) {
                    const auto &old = catalog_->resources
                                          .data[static_cast<Id>(catalog_->closure.data[first]) - 1];
                    if (!equal(old.digest, resource.digest) || old.bytes != resource.bytes ||
                        old.kind != resource.kind) {
                        where("resources", i, resource.offset);
                        fail(Error::Conflict);
                    }
                    continue;
                }
                if (!catalog_->closure.data ||
                    catalog_->closure.count == catalog_->closure.capacity) {
                    fail(Error::OperatingProfile);
                    break;
                }
                for (std::size_t j = catalog_->closure.count; j > first; --j)
                    catalog_->closure.data[j] = catalog_->closure.data[j - 1];
                catalog_->closure.data[first] = key;
                ++catalog_->closure.count;
            }
        }
    }
    for (std::size_t i = 0; i < catalog_->artifacts.count && result_.error == Error::None; ++i) {
        auto &a = catalog_->artifacts.data[i];
        if (a.roles & RulesRole) {
            if (!a.verified)
                fail(Error::ProfileMismatch);
        } else if (a.roles & LegacyRole)
            legacyArtifact(static_cast<Id>(i + 1));
        else if (!a.verified)
            hashArtifact(static_cast<Id>(i + 1));
    }
    for (std::size_t i = 0; i < catalog_->profiles.count && result_.error == Error::None; ++i) {
        auto &p = catalog_->profiles.data[i];
        if (!catalog_->artifacts.data[p.artifact - 1].verified) {
            where("profiles", i, p.offset);
            fail(Error::Reference);
        }
        p.used = true;
    }
    if (result_.error == Error::None && (!builder_ || !builder_->prepare())) {
        if (builder_)
            result_.program = builder_->result();
        fail(Error::Program);
    }
    return result_.error == Error::None;
}
bool Validator::ByteSink::begin() {
    if (owner_.metadata_.exceeded())
        owner_.fail(Error::MetadataLimit);
    return owner_.result_.error == Error::None;
}
bool Validator::ByteSink::beginStation(const char *station) {
    auto &v = owner_;
    const Id logical = v.intern(station), id = v.findStation(logical);
    v.currentProfile_ = 0;
    for (std::size_t i = 0; i < v.catalog_->profiles.count; ++i)
        if (v.catalog_->profiles.data[i].station == id &&
            v.catalog_->profiles.data[i].format == ProfileFormat::Rules)
            v.currentProfile_ = static_cast<Id>(i + 1);
    if (!v.currentProfile_) {
        v.fail(Error::Reference);
        return false;
    }
    auto &profile = v.catalog_->profiles.data[v.currentProfile_ - 1];
    v.currentArtifact_ = profile.artifact;
    v.source_ = InputSource::Artifact;
    v.result_.module = 0;
    v.diagnostic_[0] = 0;
    v.offset_ = v.profileOffset_ = 0;
    v.profileEnded_ = false;
    auto &a = v.catalog_->artifacts.data[profile.artifact - 1];
    if (a.opened) {
        v.fail(Error::Conflict);
        return false;
    }
    a.opened = true;
    v.profileSource_ = nullptr;
    if (!v.provider_->open(v.string(a.path), v.profileSource_) || !v.profileSource_) {
        if (v.profileSource_)
            v.provider_->close(*v.profileSource_);
        v.profileSource_ = nullptr;
        v.fail(Error::SourceFailure);
        return false;
    }
    v.hashBegin();
    return true;
}
bool Validator::ByteSink::write(const char *bytes, std::size_t count) {
    auto &v = owner_;
    if (!v.profileSource_)
        return false;
    auto &a = v.catalog_->artifacts.data[v.currentArtifact_ - 1];
    for (std::size_t i = 0; i < count; ++i) {
        if (v.profileEnded_)
            continue;
        std::uint8_t actual = 0;
        v.offset_ = v.profileOffset_;
        const auto status = v.profileSource_->readByte(actual);
        if (status == PackageReadStatus::Failure) {
            v.profileEnded_ = true;
            v.fail(Error::SourceFailure);
            continue;
        }
        if (status == PackageReadStatus::End) {
            v.profileEnded_ = true;
            v.fail(Error::LengthMismatch);
            continue;
        }
        ++v.profileOffset_;
        v.hashByte(actual);
        if (actual != static_cast<std::uint8_t>(bytes[i]))
            v.fail(Error::ProfileMismatch);
        if (v.profileOffset_ > a.bytes) {
            v.offset_ = v.profileOffset_;
            v.fail(Error::LengthMismatch);
            v.profileEnded_ = true;
        }
    }
    return true;
}
bool Validator::ByteSink::endStation() {
    auto &v = owner_;
    if (!v.profileSource_)
        return false;
    auto &a = v.catalog_->artifacts.data[v.currentArtifact_ - 1];
    while (!v.profileEnded_ && v.profileOffset_ <= a.bytes) {
        std::uint8_t byte = 0;
        v.offset_ = v.profileOffset_;
        const auto status = v.profileSource_->readByte(byte);
        if (status == PackageReadStatus::Failure) {
            v.profileEnded_ = true;
            v.fail(Error::SourceFailure);
            break;
        }
        if (status == PackageReadStatus::End) {
            v.profileEnded_ = true;
            break;
        }
        ++v.profileOffset_;
        v.hashByte(byte);
        v.fail(Error::ProfileMismatch);
        if (v.profileOffset_ > a.bytes) {
            v.fail(Error::LengthMismatch);
            break;
        }
    }
    v.offset_ = v.profileOffset_;
    a.actualBytes = v.profileOffset_;
    v.result_.artifactBytes += v.profileOffset_;
    if (v.result_.error == Error::None && a.actualBytes != a.bytes)
        v.fail(Error::LengthMismatch);
    if (v.result_.error == Error::None) {
        if (v.hashEnd(a.actual) && !equal(a.actual, a.expected))
            v.fail(Error::DigestMismatch);
    } else
        v.hash_->abort();
    a.verified = v.result_.error == Error::None;
    v.closeProfile();
    return a.verified;
}
bool Validator::ByteSink::commit() {
    return owner_.result_.error == Error::None;
}
void Validator::closeProfile() {
    if (profileSource_) {
        provider_->close(*profileSource_);
        profileSource_ = nullptr;
    }
}
void Validator::ByteSink::abort() {
    if (owner_.profileSource_) {
        owner_.hash_->abort();
        owner_.closeProfile();
    }
}
bool Validator::TypedSink::begin(const std::uint8_t hash[32]) {
    auto &v = owner_;
    if (std::memcmp(hash, v.content_.bytes, 32)) {
        v.source_ = InputSource::Content;
        v.fail(Error::DigestMismatch);
        return false;
    }
    std::memcpy(v.result_.content.bytes, hash, 32);
    return v.result_.error == Error::None;
}
bool Validator::TypedSink::station(const configuration::ModuleCompileWorkspace &s,
                                   const configuration::ExpandedStationWorkspace &e, Id station) {
    return owner_.expanded(s, e, station);
}
bool Validator::TypedSink::prepare() {
    return owner_.prepare();
}
void Validator::TypedSink::commit() {
    if (owner_.builder_)
        owner_.builder_->commit();
}
void Validator::TypedSink::abort() {
    if (owner_.builder_)
        owner_.builder_->abort();
    if (owner_.program_)
        owner_.program_->ready = false;
}
Result Validator::validate(configuration::IPackageByteSource &deployment,
                           configuration::IPackageByteSource &content,
                           configuration::IModuleSourceProvider &modules, SourceProvider &artifacts,
                           configuration::IPackageDigest &compilerHash,
                           configuration::IPackageDigest &artifactHash, const Selection &selection,
                           Catalog &catalog, configuration::ModuleCompileWorkspace &sourceWorkspace,
                           configuration::ExpandedStationWorkspace &expandedWorkspace,
                           rules::Program &programStaging, configuration::Profile &legacyScratch,
                           Limits limits) {
    programStaging.ready = false;
    result_ = {};
    catalog_ = &catalog;
    provider_ = &artifacts;
    hash_ = &artifactHash;
    legacy_ = &legacyScratch;
    program_ = &programStaging;
    selection_ = selection;
    limits_ = limits;
    source_ = InputSource::Deployment;
    depth_ = length_ = 0;
    offset_ = 0;
    sourceMode_ = sourceModule_ = streamingMajor_ = false;
    currentStation_ = selectedStation_ = currentProfile_ = currentArtifact_ = major_ = 0;
    profileSource_ = nullptr;
    profileOffset_ = 0;
    profileEnded_ = profileHashFailed_ = false;
    hashBuffered_ = 0;
    contentCatalog_ = false;
    diagnostic_[0] = 0;
    builder_ = nullptr;
    event_ = {};
    run_ = {};
    content_ = {};
    metadata_.reset();
    auto finish = [&]() {
        if (profileSource_) {
            hash_->abort();
            closeProfile();
        }
        if (builder_) {
            if (result_.error != Error::None)
                builder_->abort();
            builder_->~ProgramBuilder();
            builder_ = nullptr;
        }
        if (result_.error != Error::None) {
            programStaging.ready = false;
            result_.deployment = {};
            result_.content = {};
        }
        const Result out = result_;
        catalog_ = nullptr;
        provider_ = nullptr;
        hash_ = nullptr;
        legacy_ = nullptr;
        program_ = nullptr;
        selection_ = {};
        return out;
    };
    if (&compilerHash == &artifactHash || &deployment == &content || !selection.station ||
        !nonzero(selection.event) || !nonzero(selection.box) || !nonzero(selection.epoch) ||
        !limits.bindingBytes) {
        fail(Error::InvalidArgument);
        return finish();
    }
    catalog.strings.count = catalog.offsets.count = catalog.artifacts.count =
        catalog.stations.count = catalog.epochs.count = catalog.permissions.count =
            catalog.ports.count = catalog.portResources.count = catalog.subjects.count =
                catalog.profiles.count = catalog.resources.count = catalog.rules.count =
                    catalog.dependencies.count = catalog.requirements.count =
                        catalog.contentRequirements.count = catalog.contentSubjects.count =
                            catalog.runtimeRules.count = catalog.runtimeSubjects.count =
                                catalog.runtimePorts.count = catalog.runtimeResources.count =
                                    catalog.closure.count = 0;
    class DeploymentSource final : public configuration::IPackageByteSource {
      public:
        DeploymentSource(Validator &v, configuration::IPackageByteSource &s) : v_(v), s_(s) {}
        std::uint64_t count = 0;
        PackageReadStatus readByte(std::uint8_t &byte) override {
            const auto status = s_.readByte(byte);
            if (status == PackageReadStatus::Byte) {
                ++count;
                v_.hashByte(byte);
            } else if (status == PackageReadStatus::Failure) {
                v_.offset_ = count;
                v_.fail(Error::SourceFailure);
            }
            return status;
        }

      private:
        Validator &v_;
        configuration::IPackageByteSource &s_;
    } input(*this, deployment);
    hashBegin();
    const auto canonical = json_.validate(input, this);
    result_.artifactBytes = input.count;
    if (!canonical) {
        offset_ = canonical.offset;
        fail(canonical.error == configuration::CanonicalJsonError::InputFailure
                 ? Error::SourceFailure
                 : Error::CanonicalInput);
        hash_->abort();
        return finish();
    }
    if (result_.error == Error::None)
        hashEnd(result_.deployment);
    else
        hash_->abort();
    if (result_.error != Error::None)
        return finish();
    if (!resolve() || !readBindings())
        return finish();
    sourceMode_ = true;
    currentArtifact_ = 0;
    diagnostic_[0] = 0;
    result_.compiler = compiler_.compile(content, modules, compilerHash, bytes_, sourceWorkspace,
                                         expandedWorkspace, &typed_, this);
    result_.sourceBytes = metadata_.rawBytes();
    result_.metadataBytes = metadata_.metadataBytes();
    if (result_.compiler.scope != configuration::ModuleScope::ExpandedUnauthenticated) {
        if (result_.compiler.error == configuration::ModuleError::CanonicalInput) {
            source_ = result_.compiler.source ? InputSource::Module : InputSource::Content;
            currentArtifact_ = 0;
            offset_ = result_.compiler.offset;
            diagnostic_[0] = 0;
            result_.module = result_.compiler.source;
            fail(result_.compiler.canonical.error == configuration::CanonicalJsonError::InputFailure
                     ? Error::SourceFailure
                     : Error::CanonicalInput);
        } else if (result_.error == Error::None) {
            source_ = result_.compiler.source ? InputSource::Module : InputSource::Content;
            currentArtifact_ = 0;
            offset_ = result_.compiler.offset;
            diagnostic_[0] = 0;
            result_.module = result_.compiler.source;
            fail(result_.compiler.error == configuration::ModuleError::UnsupportedScope
                     ? Error::Unsupported
                 : result_.compiler.error == configuration::ModuleError::CryptoFailure
                     ? Error::CryptoFailure
                 : result_.compiler.error == configuration::ModuleError::DigestMismatch
                     ? Error::DigestMismatch
                     : Error::Compiler);
        }
        return finish();
    }
    if (result_.error == Error::None && programStaging.ready) {
        result_.scope = Scope::IntegrityCheckedUnauthenticated;
        result_.source = InputSource::Deployment;
        result_.artifact = result_.contentResource = 0;
        result_.module = 0;
        result_.offset = canonical.offset;
        result_.path[0] = 0;
    } else if (result_.error == Error::None)
        fail(Error::Program);
    return finish();
}
} // namespace artifacts
} // namespace execution
} // namespace larp
