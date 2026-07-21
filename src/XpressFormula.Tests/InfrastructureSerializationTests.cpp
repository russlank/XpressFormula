// InfrastructureSerializationTests.cpp - Tests for shared JSON, UTF, and atomic file helpers.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/InputLimits.h"
#include "../XpressFormula/Infrastructure/FileSystem/AtomicFileWriter.h"
#include "../XpressFormula/Infrastructure/Serialization/JsonParser.h"
#include "../XpressFormula/Infrastructure/Serialization/JsonWriter.h"
#include "../XpressFormula/Platform/Windows/Utf.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFAtomic = XpressFormula::Infrastructure::FileSystem;
namespace XFInputLimits = XpressFormula::Core::InputLimits;
namespace XFJson = XpressFormula::Infrastructure::Serialization;
namespace XFWUtf = XpressFormula::Platform::Windows;

namespace XpressFormulaTests {

namespace {

XFJson::JsonValue parseJsonOrFail(std::string_view json) {
    XFJson::JsonValue value;
    XFJson::JsonParser parser(json);
    std::string error;
    Assert::IsTrue(parser.parse(value, error));
    return value;
}

std::filesystem::path uniqueTestDirectory() {
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        (L"XpressFormulaTests_" + std::to_wstring(static_cast<long long>(ticks)));
    std::filesystem::create_directories(path);
    return path;
}

std::string readWholeFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::string text;
    in.seekg(0, std::ios::end);
    text.resize(static_cast<std::size_t>(in.tellg()));
    in.seekg(0, std::ios::beg);
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    return text;
}

bool parseJsonFails(std::string_view json, std::string& error) {
    XFJson::JsonValue value;
    XFJson::JsonParser parser(json);
    return !parser.parse(value, error);
}

} // namespace

TEST_CASE(JsonParser_ParsesPrimitivesObjectsAndArrays) {
    const XFJson::JsonValue root =
        parseJsonOrFail(R"({"text":"hello","flag":true,"nothing":null,"items":[1,-2.5e3,false]})");

    Assert::AreEqual(XFJson::JsonValue::Type::Object, root.type);
    Assert::AreEqual(std::string("hello"), root.find("text")->text);
    Assert::IsTrue(root.find("flag")->boolean);
    Assert::AreEqual(XFJson::JsonValue::Type::Null, root.find("nothing")->type);
    Assert::AreEqual(XFJson::JsonValue::Type::Array, root.find("items")->type);
    Assert::AreEqual(3ULL, static_cast<unsigned long long>(root.find("items")->array.size()));
}

TEST_CASE(JsonParser_DecodesUnicodeEscapesAndSurrogatePairs) {
    const XFJson::JsonValue root =
        parseJsonOrFail(R"({"latin":"\u00E9","emoji":"\uD83D\uDE00"})");

    Assert::AreEqual(std::string("\xC3""\xA9"), root.find("latin")->text);
    Assert::AreEqual(std::string("\xF0""\x9F""\x98""\x80"), root.find("emoji")->text);
}

TEST_CASE(JsonParser_RejectsMalformedStringsAndNumbers) {
    const char* invalidJson[] = {
        R"({"bad":"\uD83D"})",
        R"({"bad":"\uDE00"})",
        "{ \"bad\": \"line\nbreak\" }",
        R"({"bad":01})",
        R"({"bad":1.})",
        R"({"bad":1e999})"
    };

    for (const char* json : invalidJson) {
        XFJson::JsonValue value;
        XFJson::JsonParser parser(json);
        std::string error;
        Assert::IsFalse(parser.parse(value, error));
    }
}

TEST_CASE(JsonParser_RejectsDuplicateKeysMalformedUtf8AndLimitOverruns) {
    std::string error;
    Assert::IsTrue(parseJsonFails(R"({"a":1,"a":2})", error));
    Assert::IsTrue(error.find("Duplicate") != std::string::npos);

    error.clear();
    const std::string malformedUtf8 = std::string("{\"text\":\"") + "\xC3""(\"}";
    Assert::IsTrue(parseJsonFails(malformedUtf8, error));
    Assert::IsTrue(error.find("UTF-8") != std::string::npos);

    std::string deepJson;
    deepJson.reserve((XFInputLimits::kMaxJsonDepth + 1) * 2 + 1);
    for (std::size_t i = 0; i <= XFInputLimits::kMaxJsonDepth; ++i) {
        deepJson.push_back('[');
    }
    deepJson.push_back('0');
    for (std::size_t i = 0; i <= XFInputLimits::kMaxJsonDepth; ++i) {
        deepJson.push_back(']');
    }
    error.clear();
    Assert::IsTrue(parseJsonFails(deepJson, error));
    Assert::IsTrue(error.find("depth") != std::string::npos);

    std::ostringstream values;
    values << '[';
    for (std::size_t i = 0; i < XFInputLimits::kMaxJsonValues; ++i) {
        if (i != 0) {
            values << ',';
        }
        values << '0';
    }
    values << ']';
    error.clear();
    Assert::IsTrue(parseJsonFails(values.str(), error));
    Assert::IsTrue(error.find("value count") != std::string::npos);
}

TEST_CASE(JsonWriter_RoundTripsEscapedStringsAndNumbers) {
    XFJson::JsonWriter writer;
    writer.beginObject();
    writer.key("message");
    writer.value("line\n\"quoted\"\\path\t");
    writer.key("precise");
    writer.value(1.0 / 3.0);
    writer.key("finiteFallback");
    writer.value(std::numeric_limits<double>::infinity());
    writer.endObject();

    Assert::IsTrue(writer.valid());
    const XFJson::JsonValue root = parseJsonOrFail(writer.str());
    Assert::AreEqual(std::string("line\n\"quoted\"\\path\t"), root.find("message")->text);
    Assert::IsTrue(root.find("precise")->number > 0.333333333333);
    Assert::AreEqual(0.0, root.find("finiteFallback")->number);
}

TEST_CASE(AtomicFileWriter_ReplacesExistingTextAndCleansTemp) {
    const std::filesystem::path dir = uniqueTestDirectory();
    const std::filesystem::path target = dir / L"state.json";
    {
        std::ofstream out(target, std::ios::binary);
        out << "old";
    }

    const XFAtomic::AtomicWriteResult result =
        XFAtomic::writeTextAtomically(target, "{\"value\":42}");

    Assert::IsTrue(result.success);
    Assert::AreEqual(std::string("{\"value\":42}"), readWholeFile(target));
    Assert::IsFalse(std::filesystem::exists(XFAtomic::atomicTempPathFor(target)));

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(AtomicFileWriter_LeavesTargetDirectoryOnReplaceFailure) {
    const std::filesystem::path dir = uniqueTestDirectory();
    const std::filesystem::path target = dir / L"target";
    std::filesystem::create_directories(target);

    const XFAtomic::AtomicWriteResult result =
        XFAtomic::writeTextAtomically(target, "new text");

    Assert::IsFalse(result.success);
    Assert::IsTrue(std::filesystem::is_directory(target));
    Assert::IsFalse(std::filesystem::exists(XFAtomic::atomicTempPathFor(target)));

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(AtomicFileWriter_StaleLegacyTempPathDoesNotBlockWrite) {
    const std::filesystem::path dir = uniqueTestDirectory();
    const std::filesystem::path target = dir / L"state.json";
    const std::filesystem::path staleTemp = XFAtomic::atomicTempPathFor(target);
    {
        std::ofstream out(target, std::ios::binary);
        out << "old";
    }
    {
        std::ofstream out(staleTemp, std::ios::binary);
        out << "stale";
    }

    const XFAtomic::AtomicWriteResult result =
        XFAtomic::writeTextAtomically(target, "new");

    Assert::IsTrue(result.success);
    Assert::AreEqual(std::string("new"), readWholeFile(target));
    Assert::AreEqual(std::string("stale"), readWholeFile(staleTemp));

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(Utf_RoundTripsUtf8AndRejectsMalformedInput) {
    const std::string utf8 = std::string("Cafe ") + "\xC3""\xA9" + " " +
        "\xF0""\x9F""\x98""\x80";

    const XFWUtf::Utf8ToUtf16Result wide = XFWUtf::utf8ToUtf16(utf8);
    Assert::IsTrue(wide.success);

    const XFWUtf::Utf16ToUtf8Result roundTrip = XFWUtf::utf16ToUtf8(wide.text);
    Assert::IsTrue(roundTrip.success);
    Assert::AreEqual(utf8, roundTrip.text);

    const XFWUtf::Utf8ToUtf16Result badUtf8 =
        XFWUtf::utf8ToUtf16(std::string("\xC3""(", 2));
    Assert::IsFalse(badUtf8.success);

    std::wstring badUtf16;
    badUtf16.push_back(static_cast<wchar_t>(0xD800));
    const XFWUtf::Utf16ToUtf8Result badWide = XFWUtf::utf16ToUtf8(badUtf16);
    Assert::IsFalse(badWide.success);
}

} // namespace XpressFormulaTests
