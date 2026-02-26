// UpdateVersionUtilsTests.cpp - Tests for semantic-version parsing/comparison and JSON extraction.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/UpdateVersionUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::Core::UpdateVersionUtils;

namespace XpressFormulaTests {

TEST_CASE(UpdateVersion_ParsePlainVersion) {
    SemanticVersion v{};
    Assert::IsTrue(tryParseSemanticVersion("1.3.0", v));
    Assert::AreEqual(1, v.major);
    Assert::AreEqual(3, v.minor);
    Assert::AreEqual(0, v.patch);
}

TEST_CASE(UpdateVersion_ParseVPrefixedTagWithSuffix) {
    SemanticVersion v{};
    Assert::IsTrue(tryParseSemanticVersion(" v1.4.2-beta.1 ", v));
    Assert::AreEqual(1, v.major);
    Assert::AreEqual(4, v.minor);
    Assert::AreEqual(2, v.patch);
}

TEST_CASE(UpdateVersion_RejectFourPartVersion) {
    SemanticVersion v{};
    Assert::IsFalse(tryParseSemanticVersion("1.2.3.4", v));
}

TEST_CASE(UpdateVersion_CompareSemanticVersion) {
    SemanticVersion a{ 1, 3, 0 };
    SemanticVersion b{ 1, 4, 0 };
    SemanticVersion c{ 1, 4, 0 };
    Assert::IsTrue(compareSemanticVersion(a, b) < 0);
    Assert::IsTrue(compareSemanticVersion(b, a) > 0);
    Assert::AreEqual(0, compareSemanticVersion(b, c));
}

TEST_CASE(UpdateVersion_IsRemoteVersionNewer) {
    Assert::IsTrue(isRemoteVersionNewer("1.3.0", "v1.3.1"));
    Assert::IsTrue(isRemoteVersionNewer("1.3.0", "1.4.0"));
    Assert::IsFalse(isRemoteVersionNewer("1.3.0", "v1.3.0"));
    Assert::IsFalse(isRemoteVersionNewer("1.3.0", "v1.2.9"));
    Assert::IsFalse(isRemoteVersionNewer("unknown", "v1.4.0"));
}

TEST_CASE(UpdateVersion_ExtractJsonStringField) {
    const std::string json =
        R"({"tag_name":"v1.4.0","html_url":"https:\/\/github.com\/russlank\/XpressFormula\/releases\/tag\/v1.4.0"})";
    Assert::AreEqual(std::string("v1.4.0"), extractJsonStringField(json, "tag_name"));
    Assert::AreEqual(std::string("https://github.com/russlank/XpressFormula/releases/tag/v1.4.0"),
                     extractJsonStringField(json, "html_url"));
    Assert::AreEqual(std::string(), extractJsonStringField(json, "missing"));
}

// ----- Edge-case tests -----

TEST_CASE(UpdateVersion_EmptyStringParse) {
    SemanticVersion v{};
    Assert::IsFalse(tryParseSemanticVersion("", v));
}

TEST_CASE(UpdateVersion_WhitespaceOnlyParse) {
    SemanticVersion v{};
    Assert::IsFalse(tryParseSemanticVersion("   ", v));
}

TEST_CASE(UpdateVersion_OnePartVersion) {
    SemanticVersion v{};
    Assert::IsFalse(tryParseSemanticVersion("1", v));
}

TEST_CASE(UpdateVersion_TwoPartVersion) {
    SemanticVersion v{};
    Assert::IsFalse(tryParseSemanticVersion("1.2", v));
}

TEST_CASE(UpdateVersion_ZeroVersion) {
    SemanticVersion v{};
    Assert::IsTrue(tryParseSemanticVersion("0.0.0", v));
    Assert::AreEqual(0, v.major);
    Assert::AreEqual(0, v.minor);
    Assert::AreEqual(0, v.patch);
}

TEST_CASE(UpdateVersion_UppercaseV) {
    SemanticVersion v{};
    Assert::IsTrue(tryParseSemanticVersion("V2.0.1", v));
    Assert::AreEqual(2, v.major);
    Assert::AreEqual(0, v.minor);
    Assert::AreEqual(1, v.patch);
}

TEST_CASE(UpdateVersion_SuffixDash) {
    SemanticVersion v{};
    Assert::IsTrue(tryParseSemanticVersion("1.2.3-alpha", v));
    Assert::AreEqual(1, v.major);
    Assert::AreEqual(2, v.minor);
    Assert::AreEqual(3, v.patch);
}

TEST_CASE(UpdateVersion_SuffixPlus) {
    SemanticVersion v{};
    Assert::IsTrue(tryParseSemanticVersion("1.2.3+build.42", v));
    Assert::AreEqual(1, v.major);
    Assert::AreEqual(2, v.minor);
    Assert::AreEqual(3, v.patch);
}

TEST_CASE(UpdateVersion_LargeNumbers) {
    SemanticVersion v{};
    Assert::IsTrue(tryParseSemanticVersion("99.88.77", v));
    Assert::AreEqual(99, v.major);
    Assert::AreEqual(88, v.minor);
    Assert::AreEqual(77, v.patch);
}

TEST_CASE(UpdateVersion_NonNumericAfterDot) {
    SemanticVersion v{};
    Assert::IsFalse(tryParseSemanticVersion("1.x.3", v));
}

TEST_CASE(UpdateVersion_LeadingTrailingWhitespace) {
    SemanticVersion v{};
    Assert::IsTrue(tryParseSemanticVersion("  1.2.3  ", v));
    Assert::AreEqual(1, v.major);
    Assert::AreEqual(2, v.minor);
    Assert::AreEqual(3, v.patch);
}

TEST_CASE(UpdateVersion_ComparePatchLevel) {
    SemanticVersion a{ 1, 3, 0 };
    SemanticVersion b{ 1, 3, 1 };
    Assert::IsTrue(compareSemanticVersion(a, b) < 0);
    Assert::IsTrue(compareSemanticVersion(b, a) > 0);
}

TEST_CASE(UpdateVersion_CompareEqual) {
    SemanticVersion a{ 2, 5, 3 };
    SemanticVersion b{ 2, 5, 3 };
    Assert::AreEqual(0, compareSemanticVersion(a, b));
}

TEST_CASE(UpdateVersion_CompareMajorDominates) {
    SemanticVersion a{ 2, 0, 0 };
    SemanticVersion b{ 1, 99, 99 };
    Assert::IsTrue(compareSemanticVersion(a, b) > 0);
}

TEST_CASE(UpdateVersion_IsRemoteNewer_SameVersion) {
    Assert::IsFalse(isRemoteVersionNewer("1.3.0", "1.3.0"));
}

TEST_CASE(UpdateVersion_IsRemoteNewer_BothInvalid) {
    Assert::IsFalse(isRemoteVersionNewer("garbage", "trash"));
}

TEST_CASE(UpdateVersion_IsRemoteNewer_CurrentInvalid) {
    Assert::IsFalse(isRemoteVersionNewer("unknown", "1.4.0"));
}

TEST_CASE(UpdateVersion_IsRemoteNewer_RemoteInvalid) {
    Assert::IsFalse(isRemoteVersionNewer("1.3.0", "not-a-version"));
}

TEST_CASE(UpdateVersion_ExtractJson_EmptyJson) {
    Assert::AreEqual(std::string(), extractJsonStringField("{}", "key"));
}

TEST_CASE(UpdateVersion_ExtractJson_EmptyString) {
    Assert::AreEqual(std::string(), extractJsonStringField("", "key"));
}

TEST_CASE(UpdateVersion_ExtractJson_EmptyValue) {
    const std::string json = R"({"key":""})";
    Assert::AreEqual(std::string(""), extractJsonStringField(json, "key"));
}

TEST_CASE(UpdateVersion_ExtractJson_EscapedQuote) {
    const std::string json = R"({"key":"hello \"world\""})";
    Assert::AreEqual(std::string("hello \"world\""), extractJsonStringField(json, "key"));
}

TEST_CASE(UpdateVersion_ExtractJson_EscapedBackslash) {
    const std::string json = R"({"key":"a\\b"})";
    Assert::AreEqual(std::string("a\\b"), extractJsonStringField(json, "key"));
}

TEST_CASE(UpdateVersion_ExtractJson_EscapedNewline) {
    const std::string json = R"({"key":"line1\nline2"})";
    Assert::AreEqual(std::string("line1\nline2"), extractJsonStringField(json, "key"));
}

TEST_CASE(UpdateVersion_ExtractJson_EscapedTab) {
    const std::string json = R"({"key":"col1\tcol2"})";
    Assert::AreEqual(std::string("col1\tcol2"), extractJsonStringField(json, "key"));
}

TEST_CASE(UpdateVersion_ExtractJson_NonStringValue) {
    // Value is a number, not a string — should return empty
    const std::string json = R"({"count":42})";
    Assert::AreEqual(std::string(), extractJsonStringField(json, "count"));
}

TEST_CASE(UpdateVersion_ExtractJson_MultipleFields) {
    const std::string json = R"({"first":"a","second":"b","third":"c"})";
    Assert::AreEqual(std::string("a"), extractJsonStringField(json, "first"));
    Assert::AreEqual(std::string("b"), extractJsonStringField(json, "second"));
    Assert::AreEqual(std::string("c"), extractJsonStringField(json, "third"));
}

TEST_CASE(UpdateVersion_ExtractJson_WhitespaceAroundColon) {
    const std::string json = R"({"key" : "value"})";
    Assert::AreEqual(std::string("value"), extractJsonStringField(json, "key"));
}

} // namespace XpressFormulaTests
