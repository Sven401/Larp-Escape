#include "GameConfiguration/PackageDeclarations.h"
#include <cstring>
#include <string>
#include <unity.h>
using namespace larp::configuration;
namespace {
const std::string hex(64, 'a');
std::string root(const std::string &stations = "[]", const std::string &modules = "[]",
                 const std::string &requirements = "[]", const std::string &resources = "[]",
                 const std::string &subjects = "[]", const std::string &label = "x") {
    return "{\"format\":\"larp.package.v1\",\"label\":\"" + label + "\",\"modules\":" + modules +
           ",\"packageId\":\"pkg\",\"requires\":" + requirements + ",\"resources\":" + resources +
           ",\"revision\":1,\"stations\":" + stations + ",\"subjects\":" + subjects + "}";
}
std::string station(const std::string &values = "[]", const std::string &timers = "[]",
                    const std::string &ports = "[]", const std::string &rules = "[]") {
    return "[{\"id\":\"s\",\"instances\":[],\"label\":\"s\",\"ports\":" + ports +
           ",\"rules\":" + rules + ",\"timers\":" + timers + ",\"values\":" + values + "}]";
}
DeclarationResult check(const std::string &bytes) {
    PackageDeclarationReader reader;
    return reader.validate(bytes.data(), bytes.size());
}
std::string replace(std::string bytes, const std::string &from, const std::string &to) {
    const std::size_t index = bytes.find(from);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, index);
    bytes.replace(index, from.size(), to);
    return bytes;
}
void accepted(const std::string &bytes) {
    DeclarationResult result = check(bytes);
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(DeclarationError::None),
                                  static_cast<int>(result.error), result.path);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DeclarationScope::DeclarationsOnly),
                          static_cast<int>(result.scope));
}
void rejected(const std::string &bytes, DeclarationError error) {
    DeclarationResult result = check(bytes);
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(error), static_cast<int>(result.error),
                                  result.path);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DeclarationScope::Rejected),
                          static_cast<int>(result.scope));
}
class Stream : public IPackageByteSource {
  public:
    Stream(const std::string &text, std::size_t failure = static_cast<std::size_t>(-1))
        : text_(text), failure_(failure) {}
    PackageReadStatus readByte(std::uint8_t &byte) override {
        if (index_ == failure_)
            return PackageReadStatus::Failure;
        if (index_ == text_.size())
            return PackageReadStatus::End;
        byte = static_cast<std::uint8_t>(text_[index_++]);
        return PackageReadStatus::Byte;
    }

  private:
    const std::string &text_;
    std::size_t index_ = 0;
    std::size_t failure_;
};
void test_root_and_field_sets() {
    accepted(root());
    rejected("{}", DeclarationError::FieldSet);
    rejected(replace(root(), "\"format\":\"larp.package.v1\",", ""), DeclarationError::FieldSet);
    rejected(replace(root(), "\"label\":\"x\",", "\"label\":\"x\",\"legacy\":1,"),
             DeclarationError::FieldSet);
    rejected(replace(root(), "larp.package.v1", "larp.module.v1"), DeclarationError::Value);
    rejected(replace(root(), "\"revision\":1", "\"revision\":true"), DeclarationError::Type);
    rejected(replace(root(), "\"stations\":[]", "\"stations\":{}"), DeclarationError::Type);
    rejected(replace(root(), "\"stations\":[]", "\"stations\":[1]"), DeclarationError::Type);
    DeclarationResult missing = check("{}");
    TEST_ASSERT_EQUAL_STRING("/format", missing.path);
}
void test_number_bounds_and_types() {
    const std::string values = "[{\"id\":\"v\",\"initial\":-2147483648,\"type\":\"int32\"}]";
    accepted(root(station(values)));
    accepted(root(station(replace(values, "-2147483648", "2147483647"))));
    for (const char *value : {"-2147483649", "2147483648", "12345678901234567890123456789"})
        rejected(root(station(replace(values, "-2147483648", value))), DeclarationError::Range);
    rejected(root(station(replace(values, "-2147483648", "true"))), DeclarationError::Type);
    accepted(root(station(replace(replace(values, "-2147483648", "false"), "int32", "bool"))));
    for (const char *value : {"0", "-1", "4294967296"})
        rejected(replace(root(), "\"revision\":1", std::string("\"revision\":") + value),
                 DeclarationError::Range);
    accepted(replace(root(), "\"revision\":1", "\"revision\":4294967295"));
    const std::string timer = "[{\"durationMs\":1,\"id\":\"t\"}]";
    accepted(root(station("[]", timer)));
    accepted(root(station("[]", replace(timer, "Ms\":1", "Ms\":86400000"))));
    for (const char *value : {"0", "86400001"})
        rejected(root(station("[]", replace(timer, "Ms\":1", std::string("Ms\":") + value))),
                 DeclarationError::Range);
}
void test_text_and_ids() {
    accepted(root("[]", "[]", "[]", "[]", "[]", std::string(96, 'x')));
    rejected(root("[]", "[]", "[]", "[]", "[]", ""), DeclarationError::Range);
    rejected(root("[]", "[]", "[]", "[]", "[]", std::string(97, 'x')), DeclarationError::Range);
    accepted(replace(root(), "\"pkg\"", "\"" + std::string(48, 'x') + "\""));
    rejected(replace(root(), "\"pkg\"", "\"" + std::string(49, 'x') + "\""),
             DeclarationError::Value);
    rejected(replace(root(), "\"pkg\"", "\"\""), DeclarationError::Value);
    rejected(replace(root(), "\"pkg\"", "\"\xc3\xa4\""), DeclarationError::Value);
    accepted(replace(root(), "\"pkg\"", "\" \\\"\\\\\x7f\""));
    const std::string port =
        "[{\"capability\":\"\",\"direction\":\"input\",\"id\":\"p\",\"type\":\"boolean_sample\"}]";
    accepted(root(station("[]", "[]", port)));
    accepted(root(station(
        "[]", "[]", replace(port, "capability\":\"", "capability\":\"" + std::string(300, 'x')))));
}
void test_modules_and_requirements() {
    const std::string module =
        "{\"digest\":\"" + hex + "\",\"moduleId\":\"m\",\"version\":\"65535.0.65535\"}";
    accepted(root("[]", "[" + module + "]"));
    for (const char *value : {"65536.0.0", "01.0.0", "1.2", "1.2.3.4", ".1.2", "1..2"})
        rejected(root("[]", "[" + replace(module, "65535.0.65535", value) + "]"),
                 DeclarationError::Value);
    for (const std::string &value :
         {std::string(63, 'a'), std::string(65, 'a'), std::string(64, 'A'), std::string(64, 'g')})
        rejected(root("[]", "[" + replace(module, hex, value) + "]"), DeclarationError::Value);
    accepted(root("[]", "[" + module + "," + replace(module, "65535.0.65535", "0.0.0") + "]"));
    rejected(root("[]", "[" + module + "," + replace(module, hex, std::string(64, 'b')) + "]"),
             DeclarationError::ModuleConflict);
    std::string repeated = "[";
    for (unsigned i = 0; i < 20; ++i) {
        if (i)
            repeated += ',';
        repeated += module;
    }
    accepted(root("[]", repeated + "]"));
    rejected(root("[]", repeated + "," + replace(module, hex, std::string(64, 'b')) + "]"),
             DeclarationError::ModuleConflict);
    std::string different = "[";
    for (unsigned i = 0; i < 17; ++i) {
        if (i)
            different += ',';
        if (i == 16)
            accepted(root("[]", different.substr(0, different.size() - 1) + "]"));
        different += replace(module, "65535.0.65535", std::to_string(i) + ".0.0");
    }
    rejected(root("[]", different + "]"), DeclarationError::Capacity);
    accepted(root("[]", "[]", "[{\"name\":\"x\",\"version\":123456789012345678901234567890}]"));
    rejected(root("[]", "[]", "[{\"name\":\"x\",\"version\":0}]"), DeclarationError::Range);
}
void test_lists_and_resources() {
    const std::string a = "{\"id\":\"a\",\"originStation\":\"s\"}";
    const std::string b = replace(a, "\"a\"", "\"b\"");
    accepted(root("[]", "[]", "[]", "[]", "[" + a + "," + b + "]"));
    rejected(root("[]", "[]", "[]", "[]", "[" + a + "," + a + "]"), DeclarationError::ListOrder);
    rejected(root("[]", "[]", "[]", "[]", "[" + b + "," + a + "]"), DeclarationError::ListOrder);
    const std::string resource =
        "[{\"bytes\":4294967295,\"digest\":\"" + hex + "\",\"id\":\"r\",\"kind\":\"audio\"}]";
    accepted(root("[]", "[]", "[]", resource));
    rejected(root("[]", "[]", "[]", replace(resource, "4294967295", "0")), DeclarationError::Range);
    rejected(root("[]", "[]", "[]", replace(resource, "audio", "video")), DeclarationError::Value);
    std::string values = "[";
    for (unsigned i = 0; i < 33; ++i) {
        if (i)
            values += ',';
        values += "{\"id\":\"v" + std::to_string(100 + i) + "\",\"initial\":0,\"type\":\"int32\"}";
    }
    values += ']';
    rejected(root(station(values)), DeclarationError::Capacity);
}
void test_stream_errors_and_reuse() {
    PackageDeclarationReader reader;
    const std::string bytes = root(station("[{\"id\":\"v\",\"initial\":true,\"type\":\"bool\"}]"));
    for (std::size_t i = 0; i <= bytes.size(); ++i) {
        Stream stream(bytes, i);
        const DeclarationResult result = reader.validate(stream);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(DeclarationError::CanonicalInput),
                              static_cast<int>(result.error));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CanonicalJsonError::InputFailure),
                              static_cast<int>(result.canonical.error));
        TEST_ASSERT_EQUAL(i, result.offset);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(DeclarationScope::Rejected),
                              static_cast<int>(result.scope));
        if (i != bytes.size()) {
            const DeclarationResult truncated = reader.validate(bytes.data(), i);
            TEST_ASSERT_EQUAL_INT(static_cast<int>(DeclarationScope::Rejected),
                                  static_cast<int>(truncated.scope));
        }
    }
    Stream healthy(bytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DeclarationScope::DeclarationsOnly),
                          static_cast<int>(reader.validate(healthy).scope));
    const std::string invalid = "{}";
    Stream invalidFailure(invalid, invalid.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CanonicalJsonError::InputFailure),
                          static_cast<int>(reader.validate(invalidFailure).canonical.error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DeclarationScope::DeclarationsOnly),
                          static_cast<int>(reader.validate(bytes.data(), bytes.size()).scope));
}
void test_skipped_semantics_and_metadata() {
    // Canonical but intentionally invalid rules/instances do not become accepted package semantics.
    accepted(root(station("[]", "[]", "[]", "[{\"not_a_rule\":true}]")));
    const std::string skipped =
        root(station("[]", "[]", "[]", "[\"" + std::string(40000, 'x') + "\"]"));
    accepted(skipped);
    const std::string port =
        "[{\"capability\":\"\",\"direction\":\"input\",\"id\":\"p\",\"type\":\"audio\"}]";
    const std::string small = root(station("[]", "[]", port));
    const std::size_t overhead = small.size() - 4; // two [] skipped subtrees
    const std::string atLimit = replace(
        small, "capability\":\"",
        "capability\":\"" + std::string(PackageDeclarationReader::MetadataBytes - overhead, 'x'));
    accepted(atLimit);
    rejected(replace(atLimit, "capability\":\"", "capability\":\"x"),
             DeclarationError::MetadataLimit);
    static_assert(sizeof(PackageDeclarationReader) < 12288, "bounded reader layout");
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_root_and_field_sets);
    RUN_TEST(test_number_bounds_and_types);
    RUN_TEST(test_text_and_ids);
    RUN_TEST(test_modules_and_requirements);
    RUN_TEST(test_lists_and_resources);
    RUN_TEST(test_stream_errors_and_reuse);
    RUN_TEST(test_skipped_semantics_and_metadata);
    return UNITY_END();
}
