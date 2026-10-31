#include "GameConfiguration/PackageModules.h"
#include <cstring>
#include <string>
#include <unity.h>
#ifdef LARP_REAL_PACKAGE_PSA
#include "PsaPackageDigest.h"
#endif
using namespace larp::configuration;
using namespace larp::configuration::module_ir;
namespace {
// Fault-seam tests only: this fake is deliberately not a cryptographic proof.
// Real SHA256 acceptance runs the independent fixed-vector CLI with public PSA.
class SeamDigest : public IPackageDigest {
  public:
    int failure = 0;
    unsigned aborts = 0;
    bool begin() override {
        return failure != 1;
    }
    bool update(const std::uint8_t *, std::size_t) override {
        return failure != 2;
    }
    bool finish(std::uint8_t out[32], std::size_t &written) override {
        std::memset(out, 0x42, 32);
        written = failure == 4 ? 31 : 32;
        return failure != 3;
    }
    bool abort() override {
        ++aborts;
        return failure != 5;
    }
};
class Bytes : public IPackageByteSource {
  public:
    std::string bytes;
    std::size_t position = 0, failure = static_cast<std::size_t>(-1);
    PackageReadStatus readByte(std::uint8_t &byte) override {
        if (position == failure)
            return PackageReadStatus::Failure;
        if (position == bytes.size())
            return PackageReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes[position++]);
        return PackageReadStatus::Byte;
    }
};
class Provider : public IModuleSourceProvider {
  public:
    Bytes source;
    unsigned opens = 0, closes = 0;
    bool reject = false, nonnullFailure = false;
    bool open(const ModulePin &, IPackageByteSource *&out) override {
        ++opens;
        source.position = 0;
        out = reject && !nonnullFailure ? nullptr : &source;
        return !reject;
    }
    void close(IPackageByteSource &) override {
        ++closes;
    }
};
class Sink : public IExpandedPackageSink {
  public:
    std::string staged, published;
    unsigned aborts = 0, calls = 0;
    int failure = 0;
    bool operation() {
        return static_cast<int>(++calls) != failure;
    }
    bool begin() override {
        staged.clear();
        return operation();
    }
    bool beginStation(const char *) override {
        return operation();
    }
    bool write(const char *bytes, std::size_t count) override {
        staged.append(bytes, count);
        return operation();
    }
    bool endStation() override {
        return operation();
    }
    bool commit() override {
        if (!operation())
            return false;
        published = staged;
        return true;
    }
    void abort() override {
        ++aborts;
        staged.clear();
    }
};
template <typename T, std::size_t N> void pool(Pool<T> &p, T (&data)[N]) {
    p.data = data;
    p.capacity = N;
    p.count = 0;
}
struct Storage {
    char strings[16384];
    std::uint32_t offsets[2048];
    Node nodes[2048], expandedNodes[512];
    Symbol symbols[512];
    Action actions[1024], expandedActions[512];
    Rule rules[256];
    Definition definitions[24];
    Instance instances[128];
    Binding bindings[512];
    Constraint constraints[128];
    Id triggers[4096], expandedTriggers[4096];
    ExpandedRule expandedRules[64];
    ExpandedSymbol values[32], timers[16];
    ModuleCompileWorkspace work;
    ExpandedStationWorkspace expanded;
    Storage() {
        pool(work.strings, strings);
        pool(work.stringOffsets, offsets);
        pool(work.nodes, nodes);
        pool(work.symbols, symbols);
        pool(work.actions, actions);
        pool(work.rules, rules);
        pool(work.definitions, definitions);
        pool(work.instances, instances);
        pool(work.bindings, bindings);
        pool(work.constraints, constraints);
        pool(work.triggers, triggers);
        pool(expanded.nodes, expandedNodes);
        pool(expanded.actions, expandedActions);
        pool(expanded.rules, expandedRules);
        pool(expanded.values, values);
        pool(expanded.timers, timers);
        pool(expanded.triggers, expandedTriggers);
    }
};
Storage storage;
PackageModuleCompiler compiler;
std::string digestText() {
    std::string text;
    for (unsigned i = 0; i < 32; ++i)
        text += "42";
    return text;
}
const std::string module =
    "{\"constraints\":[],\"format\":\"larp.module.v1\",\"inputs\":[],\"instances\":[],\"label\":"
    "\"module\",\"moduleId\":\"m\",\"outputs\":[],\"parameters\":[{\"default\":2,\"description\":"
    "\"count\",\"id\":\"limit\",\"label\":\"limit\",\"max\":9,\"min\":1,\"type\":\"int32\","
    "\"unit\":\"count\"}],\"resources\":[],\"rules\":[],\"timers\":[],\"values\":[{\"id\":"
    "\"count\",\"initial\":{\"parameter\":\"limit\"},\"type\":\"int32\"}],\"version\":\"1.0.0\"}";
std::string content(bool includeModule = true) {
    const std::string digest = digestText();
    const std::string pin =
        "{\"digest\":\"" + digest + "\",\"moduleId\":\"m\",\"version\":\"1.0.0\"}";
    const std::string instance =
        "{\"digest\":\"" + digest +
        "\",\"id\":\"instance\",\"inputs\":{},\"moduleId\":\"m\",\"outputs\":{},\"parameters\":{"
        "\"limit\":{\"op\":\"literal\",\"type\":\"int32\",\"value\":3}},\"resources\":{},"
        "\"version\":\"1.0.0\"}";
    return "{\"format\":\"larp.package.v1\",\"label\":\"package\",\"modules\":[" +
           (includeModule ? pin : "") +
           "],\"packageId\":\"p\",\"requires\":[],\"resources\":[],\"revision\":1,\"stations\":[{"
           "\"id\":\"station\",\"instances\":[" +
           (includeModule ? instance : "") +
           "],\"label\":\"station\",\"ports\":[],\"rules\":[],\"timers\":[],\"values\":[]}],"
           "\"subjects\":[]}";
}
std::string replace(std::string bytes, const std::string &from, const std::string &to) {
    const auto p = bytes.find(from);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, p);
    bytes.replace(p, from.size(), to);
    return bytes;
}
ModuleCompileResult run(const std::string &bytes, Provider &provider, SeamDigest &digest,
                        Sink &sink, std::size_t failure = static_cast<std::size_t>(-1)) {
    Bytes source;
    source.bytes = bytes;
    source.failure = failure;
    return compiler.compile(source, provider, digest, sink, storage.work, storage.expanded);
}
void test_binding_and_exact_expansion_seam() {
    Provider provider;
    provider.source.bytes = module;
    SeamDigest digest;
    Sink sink;
    const auto result = run(content(), provider, digest, sink);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleScope::ExpandedUnauthenticated),
                          static_cast<int>(result.scope));
    TEST_ASSERT_EQUAL_STRING(
        "{\"format\":\"larp.rules.v1\",\"ports\":[],\"rules\":[],\"station\":\"station\","
        "\"timers\":[],\"values\":[{\"id\":\"/instance/count\",\"initial\":3,\"type\":\"int32\"}]}",
        sink.published.c_str());
    TEST_ASSERT_EQUAL(1, provider.opens);
    TEST_ASSERT_EQUAL(1, provider.closes);
    TEST_ASSERT_EQUAL(1, result.instances);
}
void test_parameter_and_schema_failures_seam() {
    Provider provider;
    provider.source.bytes = module;
    SeamDigest digest;
    Sink sink;
    auto result = run(replace(content(), "\"value\":3", "\"value\":10"), provider, digest, sink);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::Range), static_cast<int>(result.error));
    TEST_ASSERT_EQUAL(0, result.source);
    TEST_ASSERT_EQUAL_STRING("/stations/0/instances/0", result.path);
    TEST_ASSERT_TRUE(sink.published.empty());
    provider.source.bytes = replace(module, "\"unit\":\"count\"", "\"unit\":\"days\"");
    result = run(content(), provider, digest, sink);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::Value), static_cast<int>(result.error));
    provider.source.bytes = replace(module, "\"moduleId\":\"m\"", "\"moduleId\":\"other\"");
    result = run(content(), provider, digest, sink);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::ModuleConflict),
                          static_cast<int>(result.error));
}
void test_crypto_faults_fail_closed_and_reuse() {
    for (int fault = 1; fault <= 5; ++fault) {
        Provider provider;
        provider.source.bytes = module;
        SeamDigest digest;
        digest.failure = fault;
        Sink sink;
        const auto r = run(content(), provider, digest, sink);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::CryptoFailure),
                              static_cast<int>(r.error));
        TEST_ASSERT_EQUAL(0, sink.calls);
        TEST_ASSERT_TRUE(digest.aborts > 0);
        for (auto byte : r.contentDigest)
            TEST_ASSERT_EQUAL(0, byte);
        digest.failure = 0;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleScope::ExpandedUnauthenticated),
                              static_cast<int>(run(content(), provider, digest, sink).scope));
    }
}
void test_every_sink_operation_aborts_and_no_publication() {
    Provider provider;
    provider.source.bytes = module;
    SeamDigest digest;
    Sink good;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleScope::ExpandedUnauthenticated),
                          static_cast<int>(run(content(), provider, digest, good).scope));
    for (unsigned call = 1; call <= good.calls; ++call) {
        Sink sink;
        sink.failure = static_cast<int>(call);
        const auto r = run(content(), provider, digest, sink);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::SinkFailure),
                              static_cast<int>(r.error));
        TEST_ASSERT_EQUAL(1, sink.aborts);
        TEST_ASSERT_TRUE(sink.published.empty());
        TEST_ASSERT_TRUE(sink.staged.empty());
    }
}
void test_sources_beat_semantics_and_digest_failures() {
    const std::string bytes = replace(content(), "\"value\":3", "\"value\":10");
    for (std::size_t pos = 0; pos <= bytes.size(); ++pos) {
        Provider provider;
        provider.source.bytes = module;
        SeamDigest digest;
        digest.failure = 2;
        Sink sink;
        const auto r = run(bytes, provider, digest, sink, pos);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::CanonicalInput),
                              static_cast<int>(r.error));
        TEST_ASSERT_EQUAL(pos, r.offset);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CanonicalJsonError::InputFailure),
                              static_cast<int>(r.canonical.error));
        TEST_ASSERT_EQUAL(0, sink.calls);
    }
    Provider provider;
    provider.reject = true;
    provider.nonnullFailure = true;
    SeamDigest digest;
    Sink sink;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::MissingModule),
                          static_cast<int>(run(content(), provider, digest, sink).error));
    TEST_ASSERT_EQUAL(1, provider.closes);
}
void test_configurable_pool_exhaustion_and_reuse() {
    Provider provider;
    provider.source.bytes = module;
    SeamDigest digest;
    Sink sink;
    const std::size_t capacity = storage.work.strings.capacity;
    storage.work.strings.capacity = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::OperatingProfileLimit),
                          static_cast<int>(run(content(), provider, digest, sink).error));
    storage.work.strings.capacity = capacity;
    const auto r = run(content(), provider, digest, sink);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleScope::ExpandedUnauthenticated),
                          static_cast<int>(r.scope));
}
void test_unused_cycles_reject_with_negative_only_digest_seam() {
    // Circular real SHA256 pins would require a fixed point. The deliberately
    // noncryptographic seam is used only to prove rejection by the DAG layer.
    class DualProvider : public Provider {
      public:
        Bytes second;
        bool open(const ModulePin &pin, IPackageByteSource *&out) override {
            ++opens;
            Bytes &selected = std::strcmp(pin.id, "n") == 0 ? second : source;
            selected.position = 0;
            out = &selected;
            return true;
        }
    } provider;
    const std::string pin =
        "{\"digest\":\"" + digestText() + "\",\"moduleId\":\"m\",\"version\":\"1.0.0\"}";
    const std::string instance =
        "{\"digest\":\"" + digestText() +
        "\",\"id\":\"child\",\"inputs\":{},\"moduleId\":\"m\",\"outputs\":{},"
        "\"parameters\":{\"limit\":{\"op\":\"literal\",\"type\":\"int32\",\"value\":3}},"
        "\"resources\":{},\"version\":\"1.0.0\"}";
    const std::string unused =
        replace(content(false), "\"modules\":[]", "\"modules\":[" + pin + "]");
    provider.source.bytes = replace(module, "\"instances\":[]", "\"instances\":[" + instance + "]");
    SeamDigest digest;
    Sink sink;
    auto result = run(unused, provider, digest, sink);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::Cycle), static_cast<int>(result.error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleScope::Rejected), static_cast<int>(result.scope));
    TEST_ASSERT_EQUAL(0, sink.calls);
    const std::string pinN = replace(pin, "\"moduleId\":\"m\"", "\"moduleId\":\"n\"");
    const std::string mutual =
        replace(content(false), "\"modules\":[]", "\"modules\":[" + pin + "," + pinN + "]");
    provider.source.bytes = replace(
        module, "\"instances\":[]",
        "\"instances\":[" + replace(instance, "\"moduleId\":\"m\"", "\"moduleId\":\"n\"") + "]");
    provider.second.bytes = replace(replace(module, "\"moduleId\":\"m\"", "\"moduleId\":\"n\""),
                                    "\"instances\":[]", "\"instances\":[" + instance + "]");
    result = run(mutual, provider, digest, sink);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleError::Cycle), static_cast<int>(result.error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ModuleScope::Rejected), static_cast<int>(result.scope));
    TEST_ASSERT_EQUAL(0, sink.calls);
}
#ifdef LARP_REAL_PACKAGE_PSA
void test_real_psa_known_vectors_and_reset() {
    PsaPackageDigest digest;
    std::uint8_t out[32];
    std::size_t count = 0;
    const std::uint8_t expected[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
                                       0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                                       0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                                       0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    TEST_ASSERT_TRUE(digest.begin());
    TEST_ASSERT_TRUE(digest.update(reinterpret_cast<const std::uint8_t *>("a"), 1));
    TEST_ASSERT_TRUE(digest.update(reinterpret_cast<const std::uint8_t *>("bc"), 2));
    TEST_ASSERT_TRUE(digest.finish(out, count));
    TEST_ASSERT_EQUAL(32, count);
    TEST_ASSERT_EQUAL_MEMORY(expected, out, 32);
    TEST_ASSERT_TRUE(digest.abort());
    TEST_ASSERT_TRUE(digest.begin());
    TEST_ASSERT_FALSE(digest.update(nullptr, 1));
    std::memset(out, 0xff, 32);
    count = 99;
    TEST_ASSERT_FALSE(digest.finish(out, count));
    TEST_ASSERT_EQUAL(0, count);
    for (auto byte : out)
        TEST_ASSERT_EQUAL(0, byte);
    TEST_ASSERT_TRUE(digest.begin());
    TEST_ASSERT_TRUE(digest.abort());
    const char *expectedEmpty = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    TEST_ASSERT_TRUE(digest.begin());
    TEST_ASSERT_TRUE(digest.finish(out, count));
    TEST_ASSERT_EQUAL(32, count);
    const char hex[] = "0123456789abcdef";
    std::string rendered;
    for (auto byte : out) {
        rendered += hex[byte >> 4];
        rendered += hex[byte & 15];
    }
    TEST_ASSERT_EQUAL_STRING(expectedEmpty, rendered.c_str());
    const char *message = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    TEST_ASSERT_TRUE(digest.begin());
    TEST_ASSERT_TRUE(digest.update(reinterpret_cast<const std::uint8_t *>(message), 17));
    TEST_ASSERT_TRUE(digest.update(reinterpret_cast<const std::uint8_t *>(message + 17),
                                   std::strlen(message) - 17));
    TEST_ASSERT_TRUE(digest.finish(out, count));
    rendered.clear();
    for (auto byte : out) {
        rendered += hex[byte >> 4];
        rendered += hex[byte & 15];
    }
    TEST_ASSERT_EQUAL_STRING("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                             rendered.c_str());
}
#endif
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_binding_and_exact_expansion_seam);
    RUN_TEST(test_parameter_and_schema_failures_seam);
    RUN_TEST(test_crypto_faults_fail_closed_and_reuse);
    RUN_TEST(test_every_sink_operation_aborts_and_no_publication);
    RUN_TEST(test_sources_beat_semantics_and_digest_failures);
    RUN_TEST(test_configurable_pool_exhaustion_and_reuse);
    RUN_TEST(test_unused_cycles_reject_with_negative_only_digest_seam);
#ifdef LARP_REAL_PACKAGE_PSA
    RUN_TEST(test_real_psa_known_vectors_and_reset);
#endif
    return UNITY_END();
}
