#include "GameConfiguration/ProfileWriter.h"
#include "GameExecution/PackageArtifacts.h"
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <unity.h>
namespace xa = larp::execution::artifacts;
namespace xr = larp::execution::rules;
namespace xc = larp::configuration;
namespace ir = larp::configuration::module_ir;
namespace {
template <typename T, std::size_t N> void pool(ir::Pool<T> &p, T (&data)[N]) {
    p.data = data;
    p.capacity = N;
    p.count = 0;
}
struct Memory {
    xa::Catalog catalog;
    char strings[65534];
    std::uint32_t offsets[4096];
    xa::Artifact artifacts[128];
    xa::Station stations[8];
    xr::Identity epochs[256];
    xa::Permission permissions[256];
    xa::Port ports[256];
    ir::Id portResources[1024];
    xa::Subject subjects[128];
    xa::Profile profiles[32];
    xa::Resource resources[256];
    xa::Rule rules[512];
    xa::Dependency dependencies[1024];
    xa::Requirement requirements[64], contentRequirements[64];
    xa::ContentSubject contentSubjects[128];
    xr::RuleBinding runtimeRules[64];
    xr::SubjectBinding runtimeSubjects[128];
    xr::PortBinding runtimePorts[64];
    const char *runtimeResources[1024];
    std::uint64_t closure[4096];
    xc::ModuleCompileWorkspace source;
    xc::ExpandedStationWorkspace expanded;
    char sourceStrings[32768];
    std::uint32_t sourceOffsets[2048];
    ir::Node nodes[1024], enodes[512];
    ir::Symbol symbols[512];
    ir::Action actions[1024], eactions[512];
    ir::Rule sourceRules[256];
    ir::Definition definitions[24];
    ir::Instance instances[64];
    ir::Binding bindings[512];
    ir::Constraint constraints[128];
    ir::Id triggers[4096], etriggers[4096];
    ir::ExpandedRule erules[64];
    ir::ExpandedSymbol values[32], timers[16];
    xr::Program program;
    char programStrings[16384];
    std::uint32_t programOffsets[2048];
    ir::Node pnodes[512];
    ir::Action pactions[512];
    xr::Rule prules[64];
    xr::Symbol pvalues[32], ptimers[16];
    xr::Port pports[64];
    xr::Subject psubjects[128];
    xr::Resource presources[32];
    xr::Identity pepochs[256];
    ir::Id ptriggers[4096], allowed[1024];
    xc::Profile legacy, example;
    Memory() {
#define P(field, data) pool(catalog.field, data)
        P(strings, strings);
        P(offsets, offsets);
        P(artifacts, artifacts);
        P(stations, stations);
        P(epochs, epochs);
        P(permissions, permissions);
        P(ports, ports);
        P(portResources, portResources);
        P(subjects, subjects);
        P(profiles, profiles);
        P(resources, resources);
        P(rules, rules);
        P(dependencies, dependencies);
        P(requirements, requirements);
        P(contentRequirements, contentRequirements);
        P(contentSubjects, contentSubjects);
        P(runtimeRules, runtimeRules);
        P(runtimeSubjects, runtimeSubjects);
        P(runtimePorts, runtimePorts);
        P(runtimeResources, runtimeResources);
        P(closure, closure);
#undef P
#define P(field, data) pool(source.field, data)
        P(strings, sourceStrings);
        P(stringOffsets, sourceOffsets);
        P(nodes, nodes);
        P(symbols, symbols);
        P(actions, actions);
        P(rules, sourceRules);
        P(definitions, definitions);
        P(instances, instances);
        P(bindings, bindings);
        P(constraints, constraints);
        P(triggers, triggers);
#undef P
#define P(field, data) pool(expanded.field, data)
        P(nodes, enodes);
        P(actions, eactions);
        P(rules, erules);
        P(values, values);
        P(timers, timers);
        P(triggers, etriggers);
#undef P
#define P(field, data) pool(program.field, data)
        P(strings, programStrings);
        P(offsets, programOffsets);
        P(nodes, pnodes);
        P(actions, pactions);
        P(rules, prules);
        P(values, pvalues);
        P(timers, ptimers);
        P(ports, pports);
        P(subjects, psubjects);
        P(resources, presources);
        P(epochs, pepochs);
        P(triggers, ptriggers);
        P(allowedResources, allowed);
#undef P
    }
} memory;
// Deliberately noncryptographic seam. Real SHA and complete byte-integrity
// acceptance are covered by the independent BCrypt and production PSA targets.
class SeamHash : public xc::IPackageDigest {
  public:
    int failure = 0;
    bool begin() override {
        return failure != 1;
    }
    bool update(const std::uint8_t *, std::size_t) override {
        return failure != 2;
    }
    bool finish(std::uint8_t out[32], std::size_t &count) override {
        std::memset(out, 0x42, 32);
        count = 32;
        return failure != 3;
    }
    bool abort() override {
        return true;
    }
};
class Bytes : public xc::IPackageByteSource {
  public:
    std::string bytes;
    std::size_t position = 0, failure = static_cast<std::size_t>(-1);
    xc::PackageReadStatus readByte(std::uint8_t &byte) override {
        if (position == failure)
            return xc::PackageReadStatus::Failure;
        if (position == bytes.size())
            return xc::PackageReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes[position++]);
        return xc::PackageReadStatus::Byte;
    }
};
class NoModules : public xc::IModuleSourceProvider {
  public:
    bool open(const xc::ModulePin &, xc::IPackageByteSource *&s) override {
        s = nullptr;
        return false;
    }
    void close(xc::IPackageByteSource &) override {}
};
class Sources : public xa::SourceProvider {
  public:
    Bytes binding, profile, pin, legacy;
    unsigned opened = 0, closed = 0;
    bool reject = false, nonnull = false;
    bool open(const char *path, xc::IPackageByteSource *&source) override {
        ++opened;
        source = std::strcmp(path, "binding.cbor") == 0   ? &binding
                 : std::strcmp(path, "s.rules") == 0      ? &profile
                 : std::strcmp(path, "zlegacy.json") == 0 ? &legacy
                                                          : nullptr;
        if (reject && !nonnull)
            source = nullptr;
        return source && !reject;
    }
    bool openContentResource(const xa::ContentResourcePin &resource,
                             xc::IPackageByteSource *&source) override {
        ++opened;
        TEST_ASSERT_EQUAL_STRING("unused", resource.id);
        TEST_ASSERT_EQUAL(4, resource.bytes);
        source = &pin;
        return true;
    }
    void close(xc::IPackageByteSource &) override {
        ++closed;
    }
    void reset() {
        binding.position = profile.position = pin.position = legacy.position = 0;
        opened = closed = 0;
    }
};
xr::Identity identity(unsigned byte) {
    xr::Identity id;
    std::memset(id.bytes, static_cast<int>(byte), 16);
    return id;
}
std::string repeatedHex(unsigned byte, unsigned count) {
    const char digits[] = "0123456789abcdef";
    std::string out;
    for (unsigned i = 0; i < count; ++i) {
        out += digits[byte >> 4];
        out += digits[byte & 15];
    }
    return out;
}
void bstr(std::string &out, unsigned byte) {
    out += static_cast<char>(0x50);
    out.append(16, static_cast<char>(byte));
}
std::string replace(std::string text, const std::string &from, const std::string &to) {
    const auto at = text.find(from);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, at);
    text.replace(at, from.size(), to);
    return text;
}
const std::string hash = repeatedHex(0x42, 32);
struct Fixture {
    Bytes deployment, content;
    Sources sources;
    NoModules modules;
    SeamHash compilerHash, artifactHash;
    xa::Selection selection;
    Fixture() {
        selection.station = "s";
        selection.event = identity(1);
        selection.box = identity(4);
        selection.epoch = identity(6);
        content.bytes = "{\"format\":\"larp.package.v1\",\"label\":\"p\",\"modules\":[],"
                        "\"packageId\":\"p\",\"requires\":[],\"resources\":[],\"revision\":1,"
                        "\"stations\":[{\"id\":\"s\",\"instances\":[],\"label\":\"s\",\"ports\":[],"
                        "\"rules\":[],\"timers\":[],\"values\":[]}],\"subjects\":[]}";
        sources.profile.bytes = "{\"format\":\"larp.rules.v1\",\"ports\":[],\"rules\":[],"
                                "\"station\":\"s\",\"timers\":[],\"values\":[]}";
        auto &binding = sources.binding.bytes;
        binding += static_cast<char>(0xa8);
        binding += static_cast<char>(0);
        binding += static_cast<char>(1);
        binding += static_cast<char>(1);
        bstr(binding, 1);
        binding += static_cast<char>(2);
        bstr(binding, 3);
        binding += static_cast<char>(3);
        binding += static_cast<char>(0x1b);
        binding.append(8, static_cast<char>(0xff));
        binding += static_cast<char>(4);
        bstr(binding, 4);
        binding += static_cast<char>(5);
        bstr(binding, 5);
        binding += static_cast<char>(6);
        binding += static_cast<char>(0x81);
        bstr(binding, 6);
        binding += static_cast<char>(7);
        binding += static_cast<char>(0x80);
        deployment.bytes =
            "{\"contentDigest\":\"" + hash + "\",\"dependencies\":[],\"event\":\"" +
            repeatedHex(1, 16) + "\",\"format\":\"larp.deployment.v1\",\"profiles\":[{\"bytes\":" +
            std::to_string(sources.profile.bytes.size()) + ",\"digest\":\"" + hash +
            "\",\"format\":\"larp.rules.v1\",\"path\":\"s.rules\",\"station\":\"s\"}],\"requires\":"
            "[],\"resources\":[],\"rules\":[],\"run\":\"" +
            repeatedHex(2, 16) + "\",\"stations\":[{\"bindingDigest\":\"" + hash +
            "\",\"bindingPath\":\"binding.cbor\",\"boxId\":\"" + repeatedHex(4, 16) +
            "\",\"generation\":18446744073709551615,\"id\":\"s\",\"ports\":[],\"stationId\":\"" +
            repeatedHex(3, 16) + "\",\"subjects\":[]}]}";
        sources.pin.bytes = "data";
    }
};
xa::Validator validator;
xa::Result run(Fixture &f, bool sameHash = false) {
    f.deployment.position = f.content.position = 0;
    f.sources.reset();
    return validator.validate(f.deployment, f.content, f.modules, f.sources, f.compilerHash,
                              sameHash ? f.compilerHash : f.artifactHash, f.selection,
                              memory.catalog, memory.source, memory.expanded, memory.program,
                              memory.legacy);
}
void test_complete_artifacts_and_full_uint64_generation_seam() {
    Fixture f;
    const auto r = run(f);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Scope::IntegrityCheckedUnauthenticated),
                          static_cast<int>(r.scope));
    TEST_ASSERT_TRUE(memory.program.ready);
    TEST_ASSERT_EQUAL_MEMORY(identity(3).bytes, memory.program.context.station.bytes, 16);
    TEST_ASSERT_EQUAL(std::numeric_limits<std::uint64_t>::max(),
                      memory.catalog.stations.data[0].generation);
    TEST_ASSERT_EQUAL(2, f.sources.opened);
    TEST_ASSERT_EQUAL(f.sources.opened, f.sources.closed);
    TEST_ASSERT_EQUAL(0, memory.program.rules.count);
}
void test_early_failure_clears_old_ready_and_hash_outputs() {
    Fixture f;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Scope::IntegrityCheckedUnauthenticated),
                          static_cast<int>(run(f).scope));
    auto r = run(f, true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Error::InvalidArgument), static_cast<int>(r.error));
    TEST_ASSERT_FALSE(memory.program.ready);
    TEST_ASSERT_EQUAL(0, f.sources.opened);
    for (auto byte : r.deployment.bytes)
        TEST_ASSERT_EQUAL(0, byte);
    for (auto byte : r.content.bytes)
        TEST_ASSERT_EQUAL(0, byte);
    f.deployment.bytes = "{}";
    r = run(f);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Error::FieldSet), static_cast<int>(r.error));
    TEST_ASSERT_FALSE(memory.program.ready);
}
void test_binding_shortest_cbor_and_failed_open_cleanup() {
    Fixture f;
    f.sources.binding.bytes.insert(2, 1, static_cast<char>(0x18));
    auto r = run(f);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Error::CanonicalInput), static_cast<int>(r.error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::InputSource::Artifact), static_cast<int>(r.source));
    TEST_ASSERT_EQUAL(f.sources.opened, f.sources.closed);
    Fixture failed;
    failed.sources.reject = failed.sources.nonnull = true;
    r = run(failed);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Error::SourceFailure), static_cast<int>(r.error));
    TEST_ASSERT_EQUAL(1, failed.sources.closed);
}
void test_profile_byte_mismatch_keeps_last_eof_failure_priority_and_reuse() {
    Fixture f;
    const auto original = f.sources.profile.bytes;
    f.sources.profile.bytes = replace(original, "\"station\":\"s\"", "\"station\":\"t\"");
    auto r = run(f);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Error::ProfileMismatch), static_cast<int>(r.error));
    TEST_ASSERT_FALSE(memory.program.ready);
    f.sources.profile.failure = f.sources.profile.bytes.size();
    r = run(f);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Error::SourceFailure), static_cast<int>(r.error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::InputSource::Artifact), static_cast<int>(r.source));
    TEST_ASSERT_EQUAL(f.sources.profile.bytes.size(), r.offset);
    TEST_ASSERT_EQUAL(f.sources.opened, f.sources.closed);
    f.sources.profile.bytes = original;
    f.sources.profile.failure = static_cast<std::size_t>(-1);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Scope::IntegrityCheckedUnauthenticated),
                          static_cast<int>(run(f).scope));
}
void test_virtual_path_failure_precedes_provider_access() {
    Fixture f;
    f.deployment.bytes = replace(f.deployment.bytes, "binding.cbor", "../binding.cbor");
    const auto r = run(f);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Error::Path), static_cast<int>(r.error));
    TEST_ASSERT_EQUAL(0, f.sources.opened);
    TEST_ASSERT_FALSE(memory.program.ready);
}
void test_unused_resource_pin_source_is_actually_read() {
    Fixture f;
    const std::string resource =
        "{\"bytes\":4,\"digest\":\"" + hash + "\",\"id\":\"unused\",\"kind\":\"audio\"}";
    f.content.bytes =
        replace(f.content.bytes, "\"resources\":[]", "\"resources\":[" + resource + "]");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Scope::IntegrityCheckedUnauthenticated),
                          static_cast<int>(run(f).scope));
    TEST_ASSERT_EQUAL(3, f.sources.opened);
    f.sources.pin.failure = 4;
    const auto r = run(f);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::Error::SourceFailure), static_cast<int>(r.error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(xa::InputSource::ContentResource),
                          static_cast<int>(r.source));
    TEST_ASSERT_EQUAL_STRING("unused", xa::name(memory.catalog, r.contentResource));
    TEST_ASSERT_EQUAL(4, r.offset);
    TEST_ASSERT_EQUAL(f.sources.opened, f.sources.closed);
}
void test_shared_metadata_profile_exact_boundary() {
    xc::PackageSourceMetadata meta;
    xc::CanonicalPackageJsonReader reader;
    const std::string base = "{\"stations\":[{\"instances\":[\"\"],\"rules\":[]}]}";
    const std::size_t overhead = base.size() - 2;
    const std::string exact =
        replace(base, "instances\":[\"", "instances\":[\"" + std::string(32768 - overhead, 'x'));
    meta.reset();
    meta.beginSource(false, 0);
    auto canonical = reader.validate(exact.data(), exact.size(), &meta);
    meta.endSource(canonical);
    TEST_ASSERT_TRUE(static_cast<bool>(canonical));
    TEST_ASSERT_EQUAL(32768, meta.metadataBytes());
    TEST_ASSERT_FALSE(meta.exceeded());
    const auto beyond = replace(exact, "instances\":[\"", "instances\":[\"x");
    meta.reset();
    meta.beginSource(false, 0);
    canonical = reader.validate(beyond.data(), beyond.size(), &meta);
    meta.endSource(canonical);
    TEST_ASSERT_EQUAL(32769, meta.metadataBytes());
    TEST_ASSERT_TRUE(meta.exceeded());
}
void test_actual_legacy_grammar_is_not_new_canonical_json() {
    Fixture f;
    auto &profile = memory.example;
    profile.revision = 1;
    TEST_ASSERT_TRUE(profile.name.assign("legacy"));
    profile.initialState = xc::StateId(1);
    profile.errorState = xc::StateId(1);
    xc::State state;
    state.id = xc::StateId(1);
    TEST_ASSERT_TRUE(state.name.assign("idle"));
    TEST_ASSERT_TRUE(profile.states.push(state));
    class Writer : public xc::IProfileWriter {
      public:
        std::string bytes;
        bool write(const char *b, std::size_t n) override {
            bytes.append(b, n);
            return true;
        }
    } writer;
    TEST_ASSERT_TRUE(xc::writeProfile(profile, writer));
    f.sources.legacy.bytes = " \n" + writer.bytes + "\r\n ";
    const std::string old =
        "{\"bytes\":" + std::to_string(f.sources.legacy.bytes.size()) + ",\"digest\":\"" + hash +
        "\",\"format\":\"profile.v1\",\"path\":\"zlegacy.json\",\"station\":\"s\"}";
    f.deployment.bytes = replace(f.deployment.bytes, "\"station\":\"s\"}],\"requires\"",
                                 "\"station\":\"s\"}," + old + "],\"requires\"");
    const auto r = run(f);
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(xa::Scope::IntegrityCheckedUnauthenticated),
                                  static_cast<int>(r.scope), r.path);
    TEST_ASSERT_EQUAL(1, memory.legacy.revision);
    TEST_ASSERT_EQUAL(0, memory.program.rules.count);
    TEST_ASSERT_EQUAL(3, f.sources.opened);
}
static_assert(!std::is_copy_constructible<xa::Validator>::value,
              "self-referencing output ports forbid copying");
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_complete_artifacts_and_full_uint64_generation_seam);
    RUN_TEST(test_early_failure_clears_old_ready_and_hash_outputs);
    RUN_TEST(test_binding_shortest_cbor_and_failed_open_cleanup);
    RUN_TEST(test_profile_byte_mismatch_keeps_last_eof_failure_priority_and_reuse);
    RUN_TEST(test_virtual_path_failure_precedes_provider_access);
    RUN_TEST(test_unused_resource_pin_source_is_actually_read);
    RUN_TEST(test_shared_metadata_profile_exact_boundary);
    RUN_TEST(test_actual_legacy_grammar_is_not_new_canonical_json);
    return UNITY_END();
}
