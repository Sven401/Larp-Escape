#include "GameConfiguration/PackageModules.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#ifdef PACKAGE_MODULE_REVIEW_PSA
#include "PsaPackageDigest.h"
#else
// Windows types must precede the public BCrypt header.
// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on
#endif
using namespace larp::configuration;
using namespace larp::configuration::module_ir;
class Bytes : public IPackageByteSource {
  public:
    std::string bytes;
    std::size_t index = 0, failure = static_cast<std::size_t>(-1);
    PackageReadStatus readByte(std::uint8_t &byte) override {
        if (index == failure)
            return PackageReadStatus::Failure;
        if (index == bytes.size())
            return PackageReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes[index++]);
        return PackageReadStatus::Byte;
    }
};
#ifdef PACKAGE_MODULE_REVIEW_PSA
using Hash = PsaPackageDigest;
#else
// Independent real SHA-256 backend using the Windows public crypto API.
class Hash : public IPackageDigest {
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;

  public:
    ~Hash() {
        abort();
        if (algorithm_)
            BCryptCloseAlgorithmProvider(algorithm_, 0);
    }
    bool begin() override {
        if (!abort())
            return false;
        if (!algorithm_ &&
            BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            return false;
        return BCryptCreateHash(algorithm_, &hash_, nullptr, 0, nullptr, 0, 0) >= 0;
    }
    bool update(const std::uint8_t *bytes, std::size_t count) override {
        return hash_ &&
               BCryptHashData(hash_, const_cast<PUCHAR>(bytes), static_cast<ULONG>(count), 0) >= 0;
    }
    bool finish(std::uint8_t digest[32], std::size_t &written) override {
        written = 0;
        if (!hash_ || BCryptFinishHash(hash_, digest, 32, 0) < 0)
            return false;
        written = 32;
        return abort();
    }
    bool abort() override {
        if (!hash_)
            return true;
        const NTSTATUS r = BCryptDestroyHash(hash_);
        hash_ = nullptr;
        return r >= 0;
    }
};
#endif
// Faults decorate the real backend and are used only for rejection vectors.
class FaultHash : public IPackageDigest {
    Hash real_;
    int failure_, document_, active_ = 0;
    bool selected(int failure) const {
        return failure_ == failure && (document_ == 0 || active_ == document_);
    }

  public:
    FaultHash(int failure, int document) : failure_(failure), document_(document) {}
    bool begin() override {
        ++active_;
        return !selected(1) && real_.begin();
    }
    bool update(const std::uint8_t *b, std::size_t n) override {
        return !selected(2) && real_.update(b, n);
    }
    bool finish(std::uint8_t d[32], std::size_t &n) override {
        if (selected(3))
            return false;
        const bool ok = real_.finish(d, n);
        if (ok && selected(6))
            std::memset(d, 0, 32);
        if (ok && selected(4))
            n = 31;
        return ok;
    }
    bool abort() override {
        const bool ok = real_.abort();
        return ok && !selected(5);
    }
};
const char *errorName(ModuleError error) {
    switch (error) {
#define MODULE_ERROR(name)                                                                         \
    case ModuleError::name:                                                                        \
        return #name
        MODULE_ERROR(None);
        MODULE_ERROR(CanonicalInput);
        MODULE_ERROR(Declarations);
        MODULE_ERROR(FieldSet);
        MODULE_ERROR(Type);
        MODULE_ERROR(Value);
        MODULE_ERROR(Reference);
        MODULE_ERROR(Range);
        MODULE_ERROR(ListOrder);
        MODULE_ERROR(ModuleConflict);
        MODULE_ERROR(MissingModule);
        MODULE_ERROR(CryptoFailure);
        MODULE_ERROR(DigestMismatch);
        MODULE_ERROR(SourceFailure);
        MODULE_ERROR(Cycle);
        MODULE_ERROR(Budget);
        MODULE_ERROR(OperatingProfileLimit);
        MODULE_ERROR(Constraint);
        MODULE_ERROR(Conflict);
        MODULE_ERROR(UnsupportedScope);
        MODULE_ERROR(SinkFailure);
#undef MODULE_ERROR
    }
    return "InvalidError";
}
struct Record {
    std::string id, version, digest;
    Bytes source;
    bool opened = false, closed = false;
};
class Provider : public IModuleSourceProvider {
  public:
    std::vector<Record> records;
    bool open(const ModulePin &pin, IPackageByteSource *&source) override {
        source = nullptr;
        for (auto &r : records)
            if (r.id == pin.id && r.version == pin.version && r.digest == pin.digest) {
                if (r.opened)
                    return false;
                r.opened = true;
                source = &r.source;
                return true;
            }
        return false;
    }
    void close(IPackageByteSource &source) override {
        for (auto &r : records)
            if (&r.source == &source) {
                if (r.closed)
                    throw 1;
                r.closed = true;
            }
    }
};
class Sink : public IExpandedPackageSink {
    int failure_;

  public:
    explicit Sink(int failure = 0) : failure_(failure) {}
    std::vector<std::string> names, documents;
    bool committed = false, aborted = false;
    bool begin() override {
        return failure_ != 1;
    }
    bool beginStation(const char *id) override {
        if (failure_ == 2)
            return false;
        names.push_back(id);
        documents.emplace_back();
        return true;
    }
    bool write(const char *bytes, std::size_t count) override {
        if (failure_ == 3 || documents.empty())
            return false;
        documents.back().append(bytes, count);
        return true;
    }
    bool endStation() override {
        return failure_ != 4;
    }
    bool commit() override {
        if (failure_ == 5)
            return false;
        committed = true;
        return true;
    }
    void abort() override {
        aborted = true;
        names.clear();
        documents.clear();
    }
};
template <class T> void storage(Pool<T> &p, std::vector<T> &v, std::size_t count) {
    v.resize(count);
    p.data = v.data();
    p.capacity = v.size();
    p.count = 0;
}
int main(int argc, char **argv) {
    PackageModuleCompiler compiler;
    ModuleCompileWorkspace workspace;
    ExpandedStationWorkspace expanded;
    std::vector<char> strings;
    std::vector<std::uint32_t> offsets;
    std::vector<Node> nodes, enodes;
    std::vector<Symbol> symbols;
    std::vector<Action> actions, eactions;
    std::vector<Rule> rules;
    std::vector<Definition> definitions;
    std::vector<Instance> instances;
    std::vector<Binding> bindings;
    std::vector<Constraint> constraints;
    std::vector<Id> triggers, etriggers;
    std::vector<ExpandedRule> erules;
    std::vector<ExpandedSymbol> values, timers;
    storage(workspace.strings, strings, 65534);
    storage(workspace.stringOffsets, offsets, 4096);
    storage(workspace.nodes, nodes, 8192);
    storage(workspace.symbols, symbols, 4096);
    storage(workspace.actions, actions, 4096);
    storage(workspace.rules, rules, 2048);
    storage(workspace.definitions, definitions, 32);
    storage(workspace.instances, instances, 1024);
    storage(workspace.bindings, bindings, 8192);
    storage(workspace.constraints, constraints, 1024);
    storage(workspace.triggers, triggers, 8192);
    storage(expanded.nodes, enodes, 512);
    storage(expanded.actions, eactions, 512);
    storage(expanded.rules, erules, 64);
    storage(expanded.values, values, 32);
    storage(expanded.timers, timers, 16);
    storage(expanded.triggers, etriggers, 4096);
    std::string line;
    while (std::getline(std::cin, line)) {
        Provider provider;
        unsigned count = 0;
        long long contentFailure = -1, moduleFailure = -1;
        int hashFailure = 0, hashDocument = 1, sinkFailure = 0;
        std::istringstream header(line);
        header >> count >> contentFailure >> moduleFailure >> hashFailure >> hashDocument >>
            sinkFailure;
        provider.records.resize(count);
        for (auto &r : provider.records) {
            std::getline(std::cin, r.id);
            std::getline(std::cin, r.version);
            std::getline(std::cin, r.digest);
            std::getline(std::cin, r.source.bytes);
        }
        Bytes content;
        std::getline(std::cin, content.bytes);
        if (argc == 3) {
            if (std::string(argv[1]) == "content-failure")
                content.failure = std::stoul(argv[2]);
            else if (!provider.records.empty())
                provider.records[0].source.failure = std::stoul(argv[2]);
        }
        if (contentFailure >= 0)
            content.failure = static_cast<std::size_t>(contentFailure);
        if (moduleFailure >= 0 && !provider.records.empty())
            provider.records[0].source.failure = static_cast<std::size_t>(moduleFailure);
        FaultHash hash(hashFailure, hashDocument);
        Sink sink(sinkFailure);
        const ModuleCompileResult result =
            compiler.compile(content, provider, hash, sink, workspace, expanded);
        const bool accepted = result.scope == ModuleScope::ExpandedUnauthenticated;
        if (accepted && (result.error != ModuleError::None || !sink.committed || sink.aborted)) {
            std::cerr << "Invalid accepted transaction\n";
            return 4;
        }
        if (accepted) {
            Hash expectedHash;
            std::uint8_t expected[32] = {};
            std::size_t written = 0;
            if (!expectedHash.begin() ||
                !expectedHash.update(reinterpret_cast<const std::uint8_t *>(content.bytes.data()),
                                     content.bytes.size()) ||
                !expectedHash.finish(expected, written) || written != 32 ||
                std::memcmp(expected, result.contentDigest, 32)) {
                std::cerr << "Wrong content digest\n";
                return 7;
            }
        } else {
            for (std::uint8_t byte : result.contentDigest)
                if (byte) {
                    std::cerr << "Rejected digest published\n";
                    return 8;
                }
        }
        if (sinkFailure && !accepted && !sink.aborted) {
            std::cerr << "Failed sink transaction not aborted\n";
            return 9;
        }
        if (!accepted && sink.committed) {
            std::cerr << "Rejected transaction published\n";
            return 5;
        }
        for (const auto &r : provider.records)
            if (r.opened != r.closed) {
                std::cerr << "Source not closed\n";
                return 6;
            }
        std::cout << (accepted ? 'A' : 'R') << ' ' << errorName(result.error) << ' '
                  << int(result.canonical.error) << ' ' << result.source << ' ' << result.offset
                  << ' ' << result.path << '\n'
                  << sink.documents.size() << '\n';
        for (std::size_t i = 0; i < sink.documents.size(); ++i)
            std::cout << sink.names[i] << '\n' << sink.documents[i] << '\n';
    }
}
