#pragma once
#include "GameConfiguration/PackageMetadata.h"
#include "GameConfiguration/ProfileReader.h"
#include "GameExecution/RulesPlanner.h"

namespace larp {
namespace execution {
namespace artifacts {
using configuration::module_ir::Id;
using configuration::module_ir::Pool;
using rules::Digest;
using rules::Identity;
enum class Error : std::uint8_t {
    None,
    InvalidArgument,
    CanonicalInput,
    SourceFailure,
    CryptoFailure,
    FieldSet,
    Type,
    Range,
    Value,
    Order,
    Reference,
    Conflict,
    Path,
    DigestMismatch,
    LengthMismatch,
    ProfileMismatch,
    BindingMismatch,
    MetadataLimit,
    OperatingProfile,
    Unsupported,
    Compiler,
    Program,
    LegacyProfile
};
enum class Scope : std::uint8_t { Rejected, IntegrityCheckedUnauthenticated };
enum class InputSource : std::uint8_t { Deployment, Content, Module, Artifact, ContentResource };
struct Result {
    Scope scope = Scope::Rejected;
    Error error = Error::None;
    InputSource source = InputSource::Deployment;
    Id artifact = 0;
    Id contentResource = 0;
    std::uint16_t module = 0;
    std::uint64_t offset = 0, artifactBytes = 0, sourceBytes = 0, metadataBytes = 0;
    char path[256] = {};
    Digest deployment, content;
    configuration::ModuleCompileResult compiler;
    rules::Result program;
};
struct ContentResourcePin {
    const char *id = nullptr;
    Digest digest;
    std::uint32_t bytes = 0;
    configuration::module_ir::Type kind = configuration::module_ir::Type::Invalid;
};
// Keys are exact checked virtual relative ASCII paths; implementations enforce
// physical root/symlink confinement. Sources remain immutable until close.
// Any non-null source returned by a failed open is still closed by the caller.
class SourceProvider {
  public:
    virtual ~SourceProvider() = default;
    virtual bool open(const char *path, configuration::IPackageByteSource *&source) = 0;
    virtual bool openContentResource(const ContentResourcePin &pin,
                                     configuration::IPackageByteSource *&source) = 0;
    virtual void close(configuration::IPackageByteSource &source) = 0;
};
enum class ProfileFormat : std::uint8_t { Invalid, Rules, Legacy };
enum class Verification : std::uint8_t { Invalid, DeviceReadback, OperatorInventory };
struct Artifact {
    Id path = 0;
    Digest expected, actual;
    std::uint64_t bytes = 0, actualBytes = 0;
    std::uint16_t roles = 0;
    Id irStation = 0;
    bool hasLength = false, opened = false, verified = false, legacy = false;
};
struct Station {
    std::uint64_t offset = 0;
    Id id = 0, bindingPath = 0;
    Identity station, box, ruleset;
    Digest binding;
    std::uint64_t generation = 0;
    std::uint32_t firstEpoch = 0, firstPermission = 0;
    std::uint16_t epochCount = 0, permissionCount = 0;
    bool generated = false;
};
struct Permission {
    std::uint64_t offset = 0;
    Id station = 0;
    std::uint8_t type = 0;
    Identity subject;
};
struct Port {
    std::uint64_t offset = 0;
    Id station = 0, id = 0;
    std::uint16_t component = 0, channel = 0;
    std::uint32_t firstResource = 0;
    std::uint16_t resourceCount = 0;
    bool used = false;
};
struct Subject {
    std::uint64_t offset = 0;
    Id station = 0, id = 0;
    Identity subject;
    bool used = false;
};
struct Profile {
    std::uint64_t offset = 0;
    Id station = 0, artifact = 0;
    Digest digest;
    std::uint32_t bytes = 0;
    ProfileFormat format = ProfileFormat::Invalid;
    bool used = false;
};
struct Resource {
    std::uint64_t offset = 0;
    Id station = 0, id = 0, artifact = 0;
    Digest digest;
    std::uint32_t bytes = 0;
    std::uint16_t bank = 0, track = 0, preset = 0;
    Verification verification = Verification::Invalid;
    configuration::module_ir::Type kind = configuration::module_ir::Type::Invalid;
    bool content = false, used = false;
};
struct Rule {
    std::uint64_t offset = 0;
    Id station = 0, expanded = 0, fallback = 0;
    Identity id;
    std::uint32_t generation = 0;
    std::uint8_t policy = 0;
    bool used = false;
};
struct Dependency {
    std::uint64_t offset = 0;
    Id station = 0, origin = 0, subject = 0;
    Identity rule;
    bool used = false;
};
struct Requirement {
    std::uint64_t offset = 0;
    Id name = 0, major = 0;
};
struct ContentSubject {
    Id id = 0, station = 0;
};
// All storage is caller-owned. No source bytes or runtime projection may be
// substituted for these records; they are populated from the actual artifacts.
struct Catalog {
    Pool<char> strings;
    Pool<std::uint32_t> offsets;
    Pool<Artifact> artifacts;
    Pool<Station> stations;
    Pool<Identity> epochs;
    Pool<Permission> permissions;
    Pool<Port> ports;
    Pool<Id> portResources;
    Pool<Subject> subjects;
    Pool<Profile> profiles;
    Pool<Resource> resources;
    Pool<Rule> rules;
    Pool<Dependency> dependencies;
    Pool<Requirement> requirements, contentRequirements;
    Pool<ContentSubject> contentSubjects;
    Pool<rules::RuleBinding> runtimeRules;
    Pool<rules::SubjectBinding> runtimeSubjects;
    Pool<rules::PortBinding> runtimePorts;
    Pool<const char *> runtimeResources;
    Pool<std::uint64_t> closure;
};
struct Selection {
    const char *station = nullptr;
    Identity event, box, epoch;
};
struct Limits {
    std::uint64_t bindingBytes = 262144;
    std::size_t closureWork = 1048576;
};
// Synchronous, exclusive, non-reentrant bundle validator. Both hash objects,
// sources, all pools, legacy scratch and program staging must be distinct and
// remain stable. Providers/callbacks must not retain borrows or mutate inputs.
class Validator : private configuration::ICompilerSourceObserver {
  public:
    Validator() = default;
    Validator(const Validator &) = delete;
    Validator &operator=(const Validator &) = delete;
    Validator(Validator &&) = delete;
    Validator &operator=(Validator &&) = delete;
    Result validate(configuration::IPackageByteSource &deployment,
                    configuration::IPackageByteSource &content,
                    configuration::IModuleSourceProvider &modules, SourceProvider &artifacts,
                    configuration::IPackageDigest &compilerHash,
                    configuration::IPackageDigest &artifactHash, const Selection &selection,
                    Catalog &catalog, configuration::ModuleCompileWorkspace &sourceWorkspace,
                    configuration::ExpandedStationWorkspace &expandedWorkspace,
                    rules::Program &programStaging, configuration::Profile &legacyScratch,
                    Limits limits = Limits{});

  private:
    enum class Kind : std::uint8_t {
        Skip,
        Root,
        Stations,
        Station,
        Ports,
        Port,
        PortResources,
        Subjects,
        Subject,
        Profiles,
        Profile,
        Resources,
        Resource,
        Locator,
        Rules,
        Rule,
        Offline,
        Dependencies,
        Dependency,
        Requires,
        Requirement,
        SourceRoot,
        SourceRequires,
        SourceRequirement
    };
    struct Frame {
        Kind kind = Kind::Skip;
        std::uint16_t seen = 0;
        Id index = 0;
        char key[49] = {};
    };
    class ByteSink final : public configuration::IExpandedPackageSink {
      public:
        explicit ByteSink(Validator &owner) : owner_(owner) {}
        bool begin() override;
        bool beginStation(const char *station) override;
        bool write(const char *bytes, std::size_t count) override;
        bool endStation() override;
        bool commit() override;
        void abort() override;

      private:
        Validator &owner_;
    } bytes_{*this};
    class TypedSink final : public configuration::IExpandedStationConsumer {
      public:
        explicit TypedSink(Validator &owner) : owner_(owner) {}
        bool begin(const std::uint8_t contentDigest[32]) override;
        bool station(const configuration::ModuleCompileWorkspace &source,
                     const configuration::ExpandedStationWorkspace &expanded, Id station) override;
        bool prepare() override;
        void commit() override;
        void abort() override;

      private:
        Validator &owner_;
    } typed_{*this};
    Catalog *catalog_ = nullptr;
    SourceProvider *provider_ = nullptr;
    configuration::IPackageDigest *hash_ = nullptr;
    configuration::Profile *legacy_ = nullptr;
    rules::Program *program_ = nullptr;
    Selection selection_;
    Limits limits_;
    Result result_;
    InputSource source_ = InputSource::Deployment;
    configuration::PackageModuleCompiler compiler_;
    configuration::PackageSourceMetadata metadata_;
    configuration::CanonicalPackageJsonReader json_;
    Frame frames_[32];
    std::size_t depth_ = 0, length_ = 0;
    std::uint64_t offset_ = 0;
    configuration::CanonicalJsonToken scalar_ = configuration::CanonicalJsonToken::Key;
    char text_[1025] = {};
    char diagnostic_[128] = {};
    bool sourceMode_ = false, sourceModule_ = false, streamingMajor_ = false;
    Id major_ = 0, currentStation_ = 0, selectedStation_ = 0;
    Id currentProfile_ = 0, currentArtifact_ = 0;
    Identity event_, run_;
    Digest content_;
    bool contentCatalog_ = false;
    configuration::IPackageByteSource *profileSource_ = nullptr;
    std::uint64_t profileOffset_ = 0;
    bool profileEnded_ = false, profileHashFailed_ = false;
    std::uint8_t hashBuffer_[256] = {};
    std::size_t hashBuffered_ = 0;
    alignas(rules::ProgramBuilder) unsigned char builderStorage_[sizeof(rules::ProgramBuilder)];
    rules::ProgramBuilder *builder_ = nullptr;
    void beginSource(bool module, std::uint16_t source) override;
    void endSource(configuration::CanonicalJsonResult result) override;
    void token(configuration::CanonicalJsonToken token, std::size_t offset) override;
    void scalarByte(std::uint8_t byte) override;
    void scalarEnd(std::size_t offset) override;
    void scalar();
    void close();
    Kind child(bool array);
    void fields(std::uint16_t expected);
    void fail(Error error);
    void where(const char *field, std::size_t index, std::uint64_t offset, Id artifact = 0);
    Id intern(const char *text);
    const char *string(Id id) const;
    Id findStation(Id id) const;
    Id path(Id text, const Digest &digest, std::uint64_t bytes, bool length, std::uint16_t role,
            Id station = 0);
    bool identifier() const;
    bool identity(Identity &id) const;
    bool digest(Digest &value) const;
    bool uint(std::uint64_t maximum, std::uint64_t &value) const;
    bool equals(const char *text) const;
    bool resolve();
    bool readBindings();
    bool readBinding(Id station);
    bool hashArtifact(Id artifact);
    bool legacyArtifact(Id artifact);
    bool contentResources(const configuration::ModuleCompileWorkspace &source);
    bool contentCatalog(const configuration::ModuleCompileWorkspace &source);
    bool expanded(const configuration::ModuleCompileWorkspace &source,
                  const configuration::ExpandedStationWorkspace &expanded, Id station);
    bool dependencies(const configuration::ModuleCompileWorkspace &source,
                      const configuration::ExpandedStationWorkspace &expanded, Id station);
    bool prepare();
    bool hashBegin();
    bool hashByte(std::uint8_t byte);
    bool hashEnd(Digest &digest);
    void closeProfile();
    template <typename T> Id append(Pool<T> &pool) {
        if (!pool.data || pool.count >= pool.capacity || pool.count >= 65534) {
            fail(Error::OperatingProfile);
            return 0;
        }
        pool.data[pool.count] = T{};
        return static_cast<Id>(++pool.count);
    }
};
const char *name(const Catalog &catalog, Id id);
} // namespace artifacts
} // namespace execution
} // namespace larp
