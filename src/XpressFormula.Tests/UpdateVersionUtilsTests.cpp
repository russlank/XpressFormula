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

} // namespace XpressFormulaTests
