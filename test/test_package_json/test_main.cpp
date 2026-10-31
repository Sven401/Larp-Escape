#include "GameConfiguration/CanonicalPackageJson.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <unity.h>

using namespace larp::configuration;
namespace {
class Bytes : public IPackageByteSource {
  public:
    explicit Bytes(const std::string &text, std::size_t failAt = static_cast<std::size_t>(-1))
        : text_(text), failAt_(failAt) {}
    PackageReadStatus readByte(std::uint8_t &byte) override {
        ++calls;
        if (position_ == failAt_)
            return PackageReadStatus::Failure;
        if (position_ == text_.size())
            return PackageReadStatus::End;
        byte = static_cast<std::uint8_t>(text_[position_++]);
        return PackageReadStatus::Byte;
    }
    std::size_t calls = 0;

  private:
    const std::string &text_;
    std::size_t failAt_;
    std::size_t position_ = 0;
};
void check(const std::string &text, CanonicalJsonError error, std::size_t offset) {
    CanonicalPackageJsonReader reader;
    const CanonicalJsonResult memory = reader.validate(text.data(), text.size());
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(error), static_cast<unsigned>(memory.error));
    TEST_ASSERT_EQUAL_UINT32(offset, memory.offset);
    TEST_ASSERT_EQUAL(error == CanonicalJsonError::None, static_cast<bool>(memory));
    Bytes source(text);
    const CanonicalJsonResult stream = reader.validate(source);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(error), static_cast<unsigned>(stream.error));
    TEST_ASSERT_EQUAL_UINT32(offset, stream.offset);
}
void good(const std::string &text) {
    check(text, CanonicalJsonError::None, text.size());
}
void test_valid_documents() {
    good("{}");
    good("{\"\":true}");
    good("{\"a\":[],\"b\":{},\"c\":[true,false,0,-1,17,{\"x\":\"text\"}]}");
    good("{\"a\":18446744073709551615,\"b\":18446744073709551616}");
    good("{\"a\":-999999999999999999999999999999999999999999999999999}");
    good("{\"a\":\" \\\" \\\\ / \x7f\"}");
    good("{\"a\":\"\xc2\x80\xe0\xa0\x80\xed\x9f\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf\"}");
    good("{\"a\":\"e\xcc\x81\"}"); // No normalization requirement.
    good("{\"a\":\"\xc3\xa9\"}");
}
void test_root_and_structure() {
    check("", CanonicalJsonError::UnexpectedEnd, 0);
    check("[]", CanonicalJsonError::UnexpectedToken, 0);
    check("true", CanonicalJsonError::UnexpectedToken, 0);
    check("0", CanonicalJsonError::UnexpectedToken, 0);
    check("\xef\xbb\xbf{}", CanonicalJsonError::UnexpectedToken, 0);
    check(" {}", CanonicalJsonError::UnexpectedToken, 0);
    check("{ }", CanonicalJsonError::UnexpectedToken, 1);
    check("{\"a\" :1}", CanonicalJsonError::UnexpectedToken, 4);
    check("{\"a\": 1}", CanonicalJsonError::UnexpectedToken, 5);
    check("{\"a\":1,}", CanonicalJsonError::UnexpectedToken, 7);
    check("{\"a\":[1,]}", CanonicalJsonError::UnexpectedToken, 8);
    check("{\"a\":[,1]}", CanonicalJsonError::UnexpectedToken, 6);
    check("{\"a\":null}", CanonicalJsonError::UnexpectedToken, 5);
    check("{\"a\":True}", CanonicalJsonError::UnexpectedToken, 5);
    check("{\"a\":truex}", CanonicalJsonError::UnexpectedToken, 9);
    check("{\"a\":]}", CanonicalJsonError::UnexpectedToken, 5);
    check("{\"a\"=1}", CanonicalJsonError::UnexpectedToken, 4);
    check("{}{}", CanonicalJsonError::TrailingData, 2);
    check("{}\n", CanonicalJsonError::TrailingData, 2);
    check(std::string("{}\0", 3), CanonicalJsonError::TrailingData, 2);
}
void test_number_offsets() {
    check("{\"a\":-0}", CanonicalJsonError::InvalidNumber, 6);
    check("{\"a\":01}", CanonicalJsonError::InvalidNumber, 6);
    check("{\"a\":+1}", CanonicalJsonError::InvalidNumber, 5);
    check("{\"a\":.1}", CanonicalJsonError::InvalidNumber, 5);
    check("{\"a\":1.0}", CanonicalJsonError::InvalidNumber, 6);
    check("{\"a\":1e3}", CanonicalJsonError::InvalidNumber, 6);
    check("{\"a\":1E3}", CanonicalJsonError::InvalidNumber, 6);
    check("{\"a\":1+2}", CanonicalJsonError::InvalidNumber, 6);
    check("{\"a\":-}", CanonicalJsonError::InvalidNumber, 6);
    check("{\"a\":-", CanonicalJsonError::UnexpectedEnd, 6);
}
void test_decoded_key_order_and_scopes() {
    good("{\"\":0,\"\\\"\":0,\"/\":0,\"\\\\\":0,\"a\":0,\"aa\":0,\"b\":0}");
    good("{\"a\":{\"x\":0},\"b\":{\"\":0,\"x\":1}}");
    good("{\"\x7f\":0}");
    check("{\"/\":0,\"\\\"\":0}", CanonicalJsonError::KeyOrder, 7);
    check("{\"b\":0,\"a\":0}", CanonicalJsonError::KeyOrder, 7);
    check("{\"aa\":0,\"a\":0}", CanonicalJsonError::KeyOrder, 8);
    check("{\"a\":0,\"a\":1}", CanonicalJsonError::DuplicateKey, 7);
    check("{\"\":0,\"\":1}", CanonicalJsonError::DuplicateKey, 6);
    check("{\"\\\"\":0,\"\\\"\":1}", CanonicalJsonError::DuplicateKey, 8);
    check("{\"a\":{\"b\":0,\"a\":0}}", CanonicalJsonError::KeyOrder, 12);
    check("{\"\xc3\xa9\":0}", CanonicalJsonError::InvalidText, 2);
}
void test_text_and_escape_offsets() {
    check("{\"a\":\"\\u0061\"}", CanonicalJsonError::InvalidEscape, 7);
    check("{\"a\":\"\\n\"}", CanonicalJsonError::InvalidEscape, 7);
    check("{\"a\":\"\\/\"}", CanonicalJsonError::InvalidEscape, 7);
    check("{\"a\":\"\\\n\"}", CanonicalJsonError::InvalidEscape, 7);
    check("{\"a\":\"\n\"}", CanonicalJsonError::InvalidText, 6);
    check(std::string("{\"a\":\"\0\"}", 9), CanonicalJsonError::InvalidText, 6);
    check("{\"\\u0061\":0}", CanonicalJsonError::InvalidEscape, 3);
    check("{\"a\":\"\\", CanonicalJsonError::UnexpectedEnd, 7);
}
void test_invalid_utf8_offsets() {
    check("{\"a\":\"\x80\"}", CanonicalJsonError::InvalidUtf8, 6);
    check("{\"a\":\"\xc0\x80\"}", CanonicalJsonError::InvalidUtf8, 6);
    check("{\"a\":\"\xc2\x7f\"}", CanonicalJsonError::InvalidUtf8, 7);
    check("{\"a\":\"\xe0\x9f\xbf\"}", CanonicalJsonError::InvalidUtf8, 7);
    check("{\"a\":\"\xed\xa0\x80\"}", CanonicalJsonError::InvalidUtf8, 7);
    check("{\"a\":\"\xf0\x8f\xbf\xbf\"}", CanonicalJsonError::InvalidUtf8, 7);
    check("{\"a\":\"\xf4\x90\x80\x80\"}", CanonicalJsonError::InvalidUtf8, 7);
    check("{\"a\":\"\xf5\x80\x80\x80\"}", CanonicalJsonError::InvalidUtf8, 6);
    check("{\"a\":\"\xe1\x80\x7f\"}", CanonicalJsonError::InvalidUtf8, 8);
    check("{\"a\":\"\xf1\x80\x80\x7f\"}", CanonicalJsonError::InvalidUtf8, 9);
    check("{\"a\":\"\xe1\x80", CanonicalJsonError::UnexpectedEnd, 8);
    check("{\"a\":\"\xf1\x80\x80", CanonicalJsonError::UnexpectedEnd, 9);
}
void test_all_truncated_prefixes() {
    const std::string text = "{\"a\":[true,false,-12,\"\xc3\xa9\\\"\",{}]}";
    for (std::size_t length = 0; length < text.size(); ++length)
        check(text.substr(0, length), CanonicalJsonError::UnexpectedEnd, length);
    good(text);
}
void test_source_failure_and_reuse() {
    const std::string text = "{\"a\":[true,-12,\"\xc3\xa9\"]}";
    CanonicalPackageJsonReader reader;
    for (std::size_t offset = 0; offset <= text.size(); ++offset) {
        Bytes source(text, offset);
        const CanonicalJsonResult result = reader.validate(source);
        TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(CanonicalJsonError::InputFailure),
                               static_cast<unsigned>(result.error));
        TEST_ASSERT_EQUAL_UINT32(offset, result.offset);
        TEST_ASSERT_TRUE(static_cast<bool>(reader.validate("{}", 2)));
    }
    const CanonicalJsonResult invalid = reader.validate(nullptr, 1);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(CanonicalJsonError::InvalidArgument),
                           static_cast<unsigned>(invalid.error));
    TEST_ASSERT_EQUAL_UINT32(0, invalid.offset);
    TEST_ASSERT_FALSE(static_cast<bool>(reader.validate(nullptr, 0)));
    TEST_ASSERT_TRUE(static_cast<bool>(reader.validate(text.data(), text.size())));
    const std::string bad = "{\"a\":{\"x\":0,\"x\":0}}";
    TEST_ASSERT_FALSE(static_cast<bool>(reader.validate(bad.data(), bad.size())));
    TEST_ASSERT_TRUE(static_cast<bool>(reader.validate(text.data(), text.size())));
}
void test_key_limit() {
    good("{\"" + std::string(48, 'a') + "\":0}");
    check("{\"" + std::string(49, 'a') + "\":0}", CanonicalJsonError::KeyTooLong, 50);
    good("{\"" + std::string(47, 'a') + "\\\"\":0}");
    check("{\"" + std::string(48, 'a') + "\\\"\":0}", CanonicalJsonError::KeyTooLong, 50);
    check("{\"" + std::string(48, 'a') + "\\\\\":0}", CanonicalJsonError::KeyTooLong, 50);
}
std::string nested(std::size_t arrays) {
    return "{\"a\":" + std::string(arrays, '[') + "0" + std::string(arrays, ']') + "}";
}
void test_depth_limit_and_expression_wrappers() {
    good(nested(31)); // Root object plus 31 arrays.
    check(nested(32), CanonicalJsonError::DepthLimit, 36);
    std::string expression = "{\"op\":\"literal\",\"type\":\"bool\",\"value\":true}";
    for (unsigned depth = 1; depth < 8; ++depth)
        expression = "{\"args\":[" + expression + "," + expression + "],\"op\":\"all\"}";
    good("{\"rules\":[{\"guard\":" + expression + "}]}");
}
void test_document_limit() {
    const std::size_t overhead = std::strlen("{\"a\":\"\"}");
    const std::string exact =
        "{\"a\":\"" + std::string(CanonicalPackageJsonReader::DocumentBytes - overhead, 'x') +
        "\"}";
    good(exact);
    check(exact + " ", CanonicalJsonError::DocumentLimit,
          CanonicalPackageJsonReader::DocumentBytes);
    check("{\"a\":\"" + std::string(CanonicalPackageJsonReader::DocumentBytes, 'x') + "\"}",
          CanonicalJsonError::DocumentLimit, CanonicalPackageJsonReader::DocumentBytes);
    CanonicalPackageJsonReader reader;
    Bytes failure(exact, exact.size());
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(CanonicalJsonError::InputFailure),
                           static_cast<unsigned>(reader.validate(failure).error));
    TEST_ASSERT_TRUE(static_cast<bool>(reader.validate("{}", 2)));
}
void test_validation_has_no_schema_claim() {
    good("{\"unknown\":999999999999999999999999999999999999999999}");
    good("{\"\":[]}");
    static_assert(sizeof(CanonicalPackageJsonReader) <= 4096,
                  "Reader storage exceeds layout target");
    std::printf("CanonicalPackageJsonReader bytes: %u\n",
                static_cast<unsigned>(sizeof(CanonicalPackageJsonReader)));
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_documents);
    RUN_TEST(test_root_and_structure);
    RUN_TEST(test_number_offsets);
    RUN_TEST(test_decoded_key_order_and_scopes);
    RUN_TEST(test_text_and_escape_offsets);
    RUN_TEST(test_invalid_utf8_offsets);
    RUN_TEST(test_all_truncated_prefixes);
    RUN_TEST(test_source_failure_and_reuse);
    RUN_TEST(test_key_limit);
    RUN_TEST(test_depth_limit_and_expression_wrappers);
    RUN_TEST(test_document_limit);
    RUN_TEST(test_validation_has_no_schema_claim);
    return UNITY_END();
}
