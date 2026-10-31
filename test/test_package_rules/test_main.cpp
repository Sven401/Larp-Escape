#include "GameConfiguration/PackageRules.h"
#include <cstring>
#include <string>
#include <unity.h>
using namespace larp::configuration;
namespace {
const std::string boolean = "{\"op\":\"literal\",\"type\":\"bool\",\"value\":true}";
const std::string number = "{\"op\":\"literal\",\"type\":\"int32\",\"value\":1}";
const std::string local = "{\"kind\":\"local\",\"ports\":[\"p\"]}";
const std::string ports = "[{\"capability\":\"sample\",\"direction\":\"input\",\"id\":\"p\","
                          "\"type\":\"boolean_sample\"}]";
const std::string values = "[{\"id\":\"v\",\"initial\":false,\"type\":\"bool\"}]";
std::string rule(const std::string &guard = boolean, const std::string &actions = "[]",
                 const std::string &trigger = local, const std::string &id = "r") {
    return "{\"actions\":" + actions + ",\"generation\":1,\"guard\":" + guard + ",\"id\":\"" + id +
           "\",\"mode\":\"once\",\"offline\":{\"policy\":\"local_continue\"},\"priority\":0,"
           "\"trigger\":" +
           trigger + "}";
}
std::string station(const std::string &rules, const std::string &portList = ports,
                    const std::string &valueList = values, const std::string &timers = "[]",
                    const std::string &instances = "[]", const std::string &id = "s") {
    return "{\"id\":\"" + id + "\",\"instances\":" + instances +
           ",\"label\":\"station\",\"ports\":" + portList + ",\"rules\":[" + rules +
           "],\"timers\":" + timers + ",\"values\":" + valueList + "}";
}
std::string package(const std::string &stations, const std::string &subjects = "[]",
                    const std::string &resources = "[]", const std::string &modules = "[]") {
    return "{\"format\":\"larp.package.v1\",\"label\":\"package\",\"modules\":" + modules +
           ",\"packageId\":\"pkg\",\"requires\":[],\"resources\":" + resources +
           ",\"revision\":1,\"stations\":[" + stations + "],\"subjects\":" + subjects + "}";
}
std::string replace(std::string bytes, const std::string &from, const std::string &to) {
    const std::size_t at = bytes.find(from);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, at);
    bytes.replace(at, from.size(), to);
    return bytes;
}
// Caller-owned static storage, not a claim about device task stacks.
PackageRuleReader reader;
RuleResult check(const std::string &bytes) {
    return reader.validate(bytes.data(), bytes.size());
}
void accepted(const std::string &bytes) {
    const RuleResult r = check(bytes);
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(RuleError::None), static_cast<int>(r.error),
                                  r.path);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RuleScope::DirectRulesOnly), static_cast<int>(r.scope));
}
void rejected(const std::string &bytes, RuleError error) {
    const RuleResult r = check(bytes);
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(error), static_cast<int>(r.error), r.path);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RuleScope::Rejected), static_cast<int>(r.scope));
}
void test_exact_schemas() {
    const std::string good = package(station(rule()));
    accepted(good);
    rejected(replace(good, "\"generation\":1,", ""), RuleError::FieldSet);
    rejected(replace(good, "\"generation\":1", "\"generation\":0"), RuleError::Range);
    rejected(replace(good, "\"priority\":0", "\"priority\":65536"), RuleError::Range);
    rejected(replace(good, "\"mode\":\"once\"", "\"mode\":\"always\""), RuleError::Value);
    rejected(replace(good, "\"trigger\":", "\"surplus\":false,\"trigger\":"), RuleError::FieldSet);
    rejected(replace(good, "local_continue", "wait"), RuleError::Value);
}
void test_expressions_and_forward_references() {
    accepted(package(station(rule("{\"field\":\"value\",\"op\":\"input\"}"))));
    accepted(package(station(rule("{\"id\":\"v\",\"op\":\"value\"}"))));
    rejected(package(station(rule("{\"id\":\"missing\",\"op\":\"value\"}"))), RuleError::Reference);
    for (const char *op : {"eq", "lt", "le", "gt", "ge"}) {
        const std::string expr =
            "{\"left\":" + number + ",\"op\":\"" + op + "\",\"right\":" + number + "}";
        accepted(package(station(rule(expr))));
        rejected(package(station(rule(replace(expr, number, boolean)))), RuleError::Type);
    }
    rejected(package(station(rule(number))), RuleError::Type);
    rejected(package(station(rule("{\"op\":\"literal\",\"type\":\"int32\",\"value\":true}"))),
             RuleError::Type);
    rejected(package(station(rule("{\"op\":\"literal\",\"type\":\"int32\",\"value\":2147483648}"))),
             RuleError::Range);
    rejected(package(station(rule("{\"args\":[" + boolean + "],\"op\":\"all\"}"))),
             RuleError::Range);
    accepted(package(station(rule("{\"args\":[" + boolean + "," + boolean + "],\"op\":\"any\"}"))));
    rejected(package(station(rule("{\"arg\":" + boolean + ",\"op\":\"not\",\"unused\":false}"))),
             RuleError::FieldSet);
}
void test_actions_and_timers() {
    const std::string set = "[{\"expr\":" + boolean + ",\"id\":\"v\",\"op\":\"set\"}]";
    accepted(package(station(rule(boolean, set))));
    rejected(package(station(rule(boolean, replace(set, "\"set\"", "\"add\"")))), RuleError::Type);
    rejected(package(station(rule(boolean, replace(set, boolean, number)))), RuleError::Type);
    const std::string trigger = "{\"kind\":\"timer\",\"timer\":\"t\"}";
    const std::string timers = "[{\"durationMs\":1,\"id\":\"t\"}]";
    accepted(package(station(rule(boolean, "[{\"op\":\"cancel\",\"timer\":\"t\"}]", trigger), ports,
                             values, timers)));
    rejected(package(station(rule(boolean, "[]", trigger))), RuleError::Reference);
    rejected(package(station(rule("{\"field\":\"value\",\"op\":\"input\"}", "[]", trigger), ports,
                             values, timers)),
             RuleError::Type);
    accepted(package(station(rule("{\"id\":\"p\",\"op\":\"port_is\"}"))));
}
void test_subject_binding_and_fact_rules() {
    const std::string subjects =
        "[{\"id\":\"f\",\"originStation\":\"s\"},{\"id\":\"out\",\"originStation\":\"s\"}]";
    const std::string fact = "{\"kind\":\"fact\",\"subjects\":[\"f\"]}";
    const std::string guard = "{\"op\":\"fact\",\"subject\":\"f\"}";
    const std::string emit = "[{\"op\":\"emit\",\"subject\":\"out\"}]";
    const std::string fr = rule(guard, emit, fact);
    accepted(package(station(fr), subjects));
    rejected(package(station(fr)), RuleError::Reference);
    rejected(package(station(replace(fr, "\"once\"", "\"each\"")), subjects), RuleError::Type);
    rejected(package(station(rule(boolean, emit, fact)), subjects), RuleError::Type);
    rejected(package(station(fr + "," + rule(boolean, emit, local, "z")), subjects),
             RuleError::Conflict);
    rejected(package(station(rule(boolean, emit) + "," + rule(guard, emit, fact, "z")), subjects),
             RuleError::Conflict);
    rejected(package(station(fr),
                     replace(subjects, "\"originStation\":\"s\"", "\"originStation\":\"absent\"")),
             RuleError::Reference);
    const std::string foreign =
        "[{\"id\":\"f\",\"originStation\":\"z\"},{\"id\":\"out\",\"originStation\":\"z\"}]";
    rejected(package(station(fr) + "," + station("", "[]", "[]", "[]", "[]", "z"), foreign),
             RuleError::Reference);
}
void test_rule_and_expression_depth_budgets() {
    std::string expr = boolean;
    for (unsigned i = 1; i < 8; ++i)
        expr = "{\"arg\":" + expr + ",\"op\":\"not\"}";
    accepted(package(station(rule(expr))));
    rejected(package(station(rule("{\"arg\":" + expr + ",\"op\":\"not\"}"))), RuleError::Budget);
    std::string rules;
    for (unsigned i = 0; i < 65; ++i) {
        if (i == 64)
            accepted(package(station(rules)));
        if (i)
            rules += ',';
        rules += rule(boolean, "[]", local, "r" + std::to_string(100 + i));
    }
    rejected(package(station(rules)), RuleError::Budget);
    rejected(package(station(rule() + "," + rule())), RuleError::ListOrder);
}
std::string tree(unsigned count) {
    if (count == 1)
        return boolean;
    if (count == 2)
        return "{\"arg\":" + boolean + ",\"op\":\"not\"}";
    const unsigned children = count - 1 < 8 ? count - 1 : 8;
    unsigned remaining = count - 1;
    std::string args;
    for (unsigned i = 0; i < children; ++i) {
        const unsigned part = remaining / (children - i);
        if (i)
            args += ',';
        args += tree(part);
        remaining -= part;
    }
    return "{\"args\":[" + args + "],\"op\":\"all\"}";
}
void test_node_and_action_budgets() {
    accepted(package(station(rule(tree(512)))));
    rejected(package(station(rule(tree(513)))), RuleError::Budget);
    accepted(package(station(rule(tree(512))) + "," +
                     station(rule(tree(512)), ports, values, "[]", "[]", "z")));
    std::string actions, timers;
    for (unsigned i = 0; i < 9; ++i) {
        if (i == 8)
            accepted(package(
                station(rule(boolean, "[" + actions + "]"), ports, values, "[" + timers + "]")));
        if (i) {
            actions += ',';
            timers += ',';
        }
        const std::string id = "t" + std::to_string(100 + i);
        actions += "{\"op\":\"cancel\",\"timer\":\"" + id + "\"}";
        timers += "{\"durationMs\":1,\"id\":\"" + id + "\"}";
    }
    rejected(
        package(station(rule(boolean, "[" + actions + "]"), ports, values, "[" + timers + "]")),
        RuleError::Budget);
}
void test_effect_resources_and_profile_conflicts() {
    const std::string output =
        ports.substr(0, ports.size() - 1) +
        ",{\"capability\":\"light\",\"direction\":\"output\",\"id\":\"q\",\"type\":\"light\"}]";
    const std::string resource = "[{\"bytes\":1,\"digest\":\"" + std::string(64, 'a') +
                                 "\",\"id\":\"res\",\"kind\":\"light_preset\"}]";
    const std::string effect = "[{\"op\":\"effect\",\"port\":\"q\",\"resource\":\"res\"}]";
    accepted(package(station(rule(boolean, effect), output), "[]", resource));
    rejected(package(station(rule(boolean, effect), output)), RuleError::Reference);
    rejected(package(station(rule(boolean, effect), output), "[]",
                     replace(resource, "light_preset", "audio")),
             RuleError::Type);
    rejected(
        package(station(rule(boolean, effect) + "," + rule(boolean, effect, local, "z"), output),
                "[]", resource),
        RuleError::OperatingProfileLimit);
}
class Stream : public IPackageByteSource {
  public:
    Stream(const std::string &bytes, std::size_t failure) : bytes_(bytes), failure_(failure) {}
    PackageReadStatus readByte(std::uint8_t &byte) override {
        if (position_ == failure_)
            return PackageReadStatus::Failure;
        if (position_ == bytes_.size())
            return PackageReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes_[position_++]);
        return PackageReadStatus::Byte;
    }

  private:
    const std::string &bytes_;
    std::size_t position_ = 0, failure_;
};
void test_source_errors_and_reuse() {
    const std::string bytes = package(station(rule()));
    for (std::size_t i = 0; i <= bytes.size(); ++i) {
        Stream source(bytes, i);
        const RuleResult r = reader.validate(source);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(RuleError::Declarations), static_cast<int>(r.error));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CanonicalJsonError::InputFailure),
                              static_cast<int>(r.declarations.canonical.error));
        TEST_ASSERT_EQUAL(i, r.offset);
        if (i < bytes.size())
            TEST_ASSERT_EQUAL_INT(static_cast<int>(RuleScope::Rejected),
                                  static_cast<int>(reader.validate(bytes.data(), i).scope));
    }
    accepted(bytes);
    const std::string invalid = package(station(rule(number)));
    Stream failed(invalid, invalid.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CanonicalJsonError::InputFailure),
                          static_cast<int>(reader.validate(failed).declarations.canonical.error));
    accepted(bytes);
    rejected("{}", RuleError::Declarations);
    accepted(bytes);
}
void test_unsupported_and_error_priority() {
    const std::string instance = package(station(rule(), ports, values, "[]", "[{}]"));
    rejected(instance, RuleError::UnsupportedScope);
    const std::string fallback =
        replace(package(station(rule())), "{\"policy\":\"local_continue\"}",
                "{\"policy\":\"local_fallback\",\"rule\":\"fallback\"}");
    rejected(fallback, RuleError::UnsupportedScope);
    rejected(replace(fallback, ",\"rule\":\"fallback\"", ""), RuleError::FieldSet);
    Stream failed(instance, instance.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CanonicalJsonError::InputFailure),
                          static_cast<int>(reader.validate(failed).declarations.canonical.error));
    rejected(instance + " ", RuleError::Declarations);
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_exact_schemas);
    RUN_TEST(test_expressions_and_forward_references);
    RUN_TEST(test_actions_and_timers);
    RUN_TEST(test_subject_binding_and_fact_rules);
    RUN_TEST(test_rule_and_expression_depth_budgets);
    RUN_TEST(test_node_and_action_budgets);
    RUN_TEST(test_effect_resources_and_profile_conflicts);
    RUN_TEST(test_source_errors_and_reuse);
    RUN_TEST(test_unsupported_and_error_priority);
    return UNITY_END();
}
