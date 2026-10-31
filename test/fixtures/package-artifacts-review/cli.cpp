#include "Support.h"
#include <list>
#include <memory>
struct CatalogMemory {
    xa::Catalog value;
#define POOL(type, field, count) std::vector<type> field;
    POOL(char, strings, 65534)
    POOL(std::uint32_t, offsets, 8192)
    POOL(xa::Artifact, artifacts, 512)
    POOL(xa::Station, stations, 8)
    POOL(xr::Identity, epochs, 256)
    POOL(xa::Permission, permissions, 2048)
    POOL(xa::Port, ports, 512)
    POOL(larp::configuration::module_ir::Id, portResources, 4096)
    POOL(xa::Subject, subjects, 128)
    POOL(xa::Profile, profiles, 64)
    POOL(xa::Resource, resources, 1024)
    POOL(xa::Rule, rules, 512)
    POOL(xa::Dependency, dependencies, 4096)
    POOL(xa::Requirement, requirements, 128)
    POOL(xa::Requirement, contentRequirements, 128)
    POOL(xa::ContentSubject, contentSubjects, 128)
    POOL(xr::RuleBinding, runtimeRules, 64)
    POOL(xr::SubjectBinding, runtimeSubjects, 128)
    POOL(xr::PortBinding, runtimePorts, 128)
    POOL(const char *, runtimeResources, 4096)
    POOL(std::uint64_t, closure, 65534)
#undef POOL
    CatalogMemory() {
        storage(value.strings, strings, 65534);
        storage(value.offsets, offsets, 8192);
        storage(value.artifacts, artifacts, 512);
        storage(value.stations, stations, 8);
        storage(value.epochs, epochs, 256);
        storage(value.permissions, permissions, 2048);
        storage(value.ports, ports, 512);
        storage(value.portResources, portResources, 4096);
        storage(value.subjects, subjects, 128);
        storage(value.profiles, profiles, 64);
        storage(value.resources, resources, 1024);
        storage(value.rules, rules, 512);
        storage(value.dependencies, dependencies, 4096);
        storage(value.requirements, requirements, 128);
        storage(value.contentRequirements, contentRequirements, 128);
        storage(value.contentSubjects, contentSubjects, 128);
        storage(value.runtimeRules, runtimeRules, 64);
        storage(value.runtimeSubjects, runtimeSubjects, 128);
        storage(value.runtimePorts, runtimePorts, 128);
        storage(value.runtimeResources, runtimeResources, 4096);
        storage(value.closure, closure, 65534);
    }
};
std::string unhex(const std::string &s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); i += 2)
        out += static_cast<char>(std::stoul(s.substr(i, 2), nullptr, 16));
    return out;
}
template <typename T> std::string hex(const T &value) {
    const char digits[] = "0123456789abcdef";
    std::string out;
    for (auto b : value.bytes) {
        out += digits[b >> 4];
        out += digits[b & 15];
    }
    return out;
}
xr::Identity identity(const std::string &s) {
    xr::Identity value;
    const auto bytes = unhex(s);
    if (bytes.size() != 16)
        throw 2;
    std::memcpy(value.bytes, bytes.data(), 16);
    return value;
}
struct File {
    std::string path, bytes;
};
struct Pin {
    std::string digest, path;
    std::uint32_t bytes;
};
class Sources final : public xa::SourceProvider {
  public:
    std::vector<File> files;
    std::vector<Pin> pins;
    std::list<std::unique_ptr<Bytes>> opened;
    std::string failurePath;
    long long failure = -1;
    unsigned opens = 0, closes = 0;
    bool failedOpenWithSource = false;
    bool open(const char *path, IPackageByteSource *&source) override {
        source = nullptr;
        for (const auto &file : files)
            if (file.path == path) {
                opened.emplace_back(new Bytes);
                auto &b = *opened.back();
                b.bytes = file.bytes;
                if (file.path == failurePath && failure >= 0)
                    b.failure = static_cast<std::size_t>(failure);
                source = &b;
                ++opens;
                return !failedOpenWithSource;
            }
        return false;
    }
    bool openContentResource(const xa::ContentResourcePin &pin,
                             IPackageByteSource *&source) override {
        for (const auto &p : pins)
            if (p.digest == hex(pin.digest) && p.bytes == pin.bytes)
                return open(p.path.c_str(), source);
        source = nullptr;
        return false;
    }
    void close(IPackageByteSource &source) override {
        for (auto i = opened.begin(); i != opened.end(); ++i)
            if (i->get() == &source) {
                opened.erase(i);
                ++closes;
                return;
            }
        throw 3;
    }
    std::string bytes(const std::string &path) const {
        for (auto &f : files)
            if (f.path == path)
                return f.bytes;
        throw 4;
    }
};
const char *errorName(xa::Error e) {
    switch (e) {
    case xa::Error::None:
        return "None";
    case xa::Error::InvalidArgument:
        return "InvalidArgument";
    case xa::Error::CanonicalInput:
        return "CanonicalInput";
    case xa::Error::SourceFailure:
        return "SourceFailure";
    case xa::Error::CryptoFailure:
        return "CryptoFailure";
    case xa::Error::FieldSet:
        return "FieldSet";
    case xa::Error::Type:
        return "Type";
    case xa::Error::Range:
        return "Range";
    case xa::Error::Value:
        return "Value";
    case xa::Error::Order:
        return "Order";
    case xa::Error::Reference:
        return "Reference";
    case xa::Error::Conflict:
        return "Conflict";
    case xa::Error::Path:
        return "Path";
    case xa::Error::DigestMismatch:
        return "DigestMismatch";
    case xa::Error::LengthMismatch:
        return "LengthMismatch";
    case xa::Error::ProfileMismatch:
        return "ProfileMismatch";
    case xa::Error::BindingMismatch:
        return "BindingMismatch";
    case xa::Error::MetadataLimit:
        return "MetadataLimit";
    case xa::Error::OperatingProfile:
        return "OperatingProfile";
    case xa::Error::Unsupported:
        return "Unsupported";
    case xa::Error::Compiler:
        return "Compiler";
    case xa::Error::Program:
        return "Program";
    case xa::Error::LegacyProfile:
        return "LegacyProfile";
    }
    return "InvalidError";
}
class AuditModules final : public Provider {
  public:
    bool failedOpenWithSource = false;
    bool open(const ModulePin &pin, IPackageByteSource *&source) override {
        const bool ok = Provider::open(pin, source);
        return ok && !failedOpenWithSource;
    }
};
class ArtifactFaultHash final : public IPackageDigest {
    Hash real;
    int mode_ = 0, target_ = 1;

  public:
    unsigned begins = 0, hits = 0;
    void configure(int mode, int target) {
        mode_ = mode;
        target_ = target;
        begins = hits = 0;
    }
    bool selected() const {
        return static_cast<int>(begins) == target_;
    }
    bool begin() override {
        ++begins;
        if (selected() && mode_ == 1) {
            ++hits;
            return false;
        }
        return real.begin();
    }
    bool update(const std::uint8_t *bytes, std::size_t count) override {
        if (selected() && mode_ == 2) {
            ++hits;
            return false;
        }
        return real.update(bytes, count);
    }
    bool finish(std::uint8_t digest[32], std::size_t &written) override {
        if (selected() && mode_ == 3) {
            ++hits;
            return false;
        }
        const bool ok = real.finish(digest, written);
        if (selected() && mode_ == 4) {
            written = 31;
            ++hits;
        }
        return ok;
    }
    bool abort() override {
        const bool ok = real.abort();
        if (selected() && mode_ == 5) {
            ++hits;
            return false;
        }
        return ok;
    }
};
int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--layout") {
        std::cout << "Validator " << sizeof(xa::Validator) << " Result " << sizeof(xa::Result)
                  << " Catalog " << sizeof(xa::Catalog) << " Metadata "
                  << sizeof(PackageSourceMetadata) << " Program " << sizeof(xr::Program)
                  << " LegacyProfile " << sizeof(larp::configuration::Profile) << '\n';
        return 0;
    }
    xa::Validator validator;
    CompilerMemory compiler;
    ProgramMemory program;
    CatalogMemory catalog;
    larp::configuration::Profile legacy;
    ArtifactFaultHash compilerHash, artifactHash;
    std::string line;
    while (std::getline(std::cin, line)) {
        unsigned nmodules = 0, npins = 0, nfiles = 0;
        long long failure = -1;
        int hashSide = 0, hashMode = 0, hashDocument = 1, seam = 0;
        std::istringstream h(line);
        h >> nmodules >> npins >> nfiles >> failure >> hashSide >> hashMode >> hashDocument >> seam;
        std::string station, event, box, epoch, deploymentPath, contentPath;
        std::getline(std::cin, station);
        std::getline(std::cin, event);
        std::getline(std::cin, box);
        std::getline(std::cin, epoch);
        std::getline(std::cin, deploymentPath);
        std::getline(std::cin, contentPath);
        Sources sources;
        std::getline(std::cin, sources.failurePath);
        sources.failure = failure;
        AuditModules modules;
        modules.failedOpenWithSource = seam == 3;
        sources.failedOpenWithSource = seam == 2;
        modules.records.resize(nmodules);
        for (auto &m : modules.records) {
            std::getline(std::cin, m.id);
            std::getline(std::cin, m.version);
            std::getline(std::cin, m.digest);
            std::getline(std::cin, line);
            m.source.bytes = unhex(line);
        }
        for (unsigned i = 0; i < npins; ++i) {
            Pin p;
            std::getline(std::cin, p.digest);
            std::getline(std::cin, line);
            p.bytes = static_cast<std::uint32_t>(std::stoul(line));
            std::getline(std::cin, p.path);
            sources.pins.push_back(p);
        }
        for (unsigned i = 0; i < nfiles; ++i) {
            File f;
            std::getline(std::cin, f.path);
            std::getline(std::cin, line);
            f.bytes = unhex(line);
            sources.files.push_back(f);
        }
        Bytes deployment, content;
        deployment.bytes = sources.bytes(deploymentPath);
        content.bytes = sources.bytes(contentPath);
        if (sources.failurePath == deploymentPath && failure >= 0)
            deployment.failure = static_cast<std::size_t>(failure);
        if (sources.failurePath == contentPath && failure >= 0)
            content.failure = static_cast<std::size_t>(failure);
        for (unsigned i = 0; i < nmodules; ++i)
            for (const auto &f : sources.files)
                if (f.path == sources.failurePath && f.bytes == modules.records[i].source.bytes &&
                    failure >= 0)
                    modules.records[i].source.failure = static_cast<std::size_t>(failure);
        xa::Selection selection;
        selection.station = station.c_str();
        selection.event = identity(event);
        selection.box = identity(box);
        selection.epoch = identity(epoch);
        compilerHash.configure(hashSide == 1 ? hashMode : 0, hashDocument);
        artifactHash.configure(hashSide == 2 ? hashMode : 0, hashDocument);
        if (seam == 1)
            program.program.ready = true;
        const auto r = validator.validate(deployment, content, modules, sources, compilerHash,
                                          seam == 1 ? static_cast<IPackageDigest &>(compilerHash)
                                                    : static_cast<IPackageDigest &>(artifactHash),
                                          selection, catalog.value, compiler.workspace,
                                          compiler.expanded, program.program, legacy);
        if (sources.opens != sources.closes || !sources.opened.empty())
            throw 5;
        for (const auto &m : modules.records)
            if (m.opened != m.closed)
                throw 6;
        std::cout << (r.scope == xa::Scope::IntegrityCheckedUnauthenticated ? 'A' : 'R') << ' '
                  << errorName(r.error) << ' ' << program.program.ready << ' ' << r.metadataBytes
                  << ' ' << hex(r.deployment) << ' ' << hex(r.content) << ' '
                  << hex(program.program.context.event) << ' ' << hex(program.program.context.run)
                  << ' ' << hex(program.program.context.station) << ' '
                  << hex(program.program.context.box) << ' '
                  << hex(program.program.context.sourceEpoch) << ' ' << errorName(r.compiler.error)
                  << ' ' << int(r.program.error) << ' ' << r.offset << ' ' << compilerHash.begins
                  << ' ' << artifactHash.begins << ' ' << compilerHash.hits << ' '
                  << artifactHash.hits << ' ' << int(r.source) << ' ' << r.module << ' '
                  << (r.artifact ? xa::name(catalog.value,
                                            catalog.value.artifacts.data[r.artifact - 1].path)
                                 : "-")
                  << ' ' << (r.contentResource ? xa::name(catalog.value, r.contentResource) : "-")
                  << ' ' << r.path << '\n';
    }
}
