#pragma once

#include <cstddef>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Microsoft::VisualStudio::CppUnitTestFramework {

class TestFailure : public std::runtime_error {
public:
    explicit TestFailure(std::string message)
        : std::runtime_error(std::move(message)) {
    }
};

inline std::string narrowMessage(const wchar_t* message) {
    if (message == nullptr) {
        return {};
    }

    std::string result;
    while (*message != L'\0') {
        const wchar_t ch = *message++;
        result.push_back(ch >= 0 && ch <= 127 ? static_cast<char>(ch) : '?');
    }

    return result;
}

inline std::string buildMessage(const char* prefix, const wchar_t* message) {
    std::string result(prefix);
    const std::string extra = narrowMessage(message);
    if (!extra.empty()) {
        result += " ";
        result += extra;
    }

    return result;
}

class TestRegistry {
public:
    struct TestCase {
        std::string name;
        std::function<void()> run;
    };

    static TestRegistry& instance() {
        static TestRegistry registry;
        return registry;
    }

    void add(std::string name, std::function<void()> run) {
        tests_.push_back({ std::move(name), std::move(run) });
    }

    int runAll() {
        std::size_t passed = 0;

        for (const TestCase& test : tests_) {
            try {
                test.run();
                ++passed;
                std::cout << "[PASS] " << test.name << "\n";
            } catch (const TestFailure& ex) {
                std::cout << "[FAIL] " << test.name << ": " << ex.what() << "\n";
            } catch (const std::exception& ex) {
                std::cout << "[FAIL] " << test.name << ": unexpected exception: " << ex.what() << "\n";
            } catch (...) {
                std::cout << "[FAIL] " << test.name << ": unknown exception\n";
            }
        }

        std::cout << passed << "/" << tests_.size() << " tests passed.\n";
        return passed == tests_.size() ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

class Assert {
public:
    static void IsTrue(bool condition, const wchar_t* message = L"") {
        if (!condition) {
            throw TestFailure(buildMessage("Assert::IsTrue failed.", message));
        }
    }

    static void IsFalse(bool condition, const wchar_t* message = L"") {
        if (condition) {
            throw TestFailure(buildMessage("Assert::IsFalse failed.", message));
        }
    }

    template <typename TExpected, typename TActual>
    static void AreEqual(const TExpected& expected, const TActual& actual,
                         const wchar_t* message = L"") {
        if (!(expected == actual)) {
            throw TestFailure(buildMessage("Assert::AreEqual failed.", message));
        }
    }
};

} // namespace Microsoft::VisualStudio::CppUnitTestFramework

#define XF_CONCAT_IMPL(a, b) a##b
#define XF_CONCAT(a, b) XF_CONCAT_IMPL(a, b)

#define TEST_CASE(testName) \
    static void testName(); \
    namespace { \
    const bool XF_CONCAT(testName##_registered_, __LINE__) = []() { \
        ::Microsoft::VisualStudio::CppUnitTestFramework::TestRegistry::instance().add( \
            std::string(__FILE__) + "::" #testName, &testName); \
        return true; \
    }(); \
    } \
    static void testName()
