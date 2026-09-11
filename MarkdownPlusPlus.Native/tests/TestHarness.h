// Minimal, dependency-free test harness for the Markdown++ native plugin.
//
// Deliberately tiny: this repo keeps its dependency list short, and fetching
// Catch2/gtest at configure time would add a network step to the build.
#pragma once

#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

namespace mdpptest {

using TestFn = void (*)();

struct TestCase {
    const char* suite;
    const char* name;
    TestFn fn;
    const char* knownBug;  // non-null => the checks are expected to fail today
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline int& CurrentTestFailures() { static int n = 0; return n; }
inline int& TotalAssertions() { static int n = 0; return n; }
inline int& TotalFailedAssertions() { static int n = 0; return n; }
inline bool& SuppressFailureOutput() { static bool quiet = false; return quiet; }

struct Registrar {
    Registrar(const char* suite, const char* name, TestFn fn, const char* knownBug = nullptr) {
        Registry().push_back(TestCase{suite, name, fn, knownBug});
    }
};

// ---------------------------------------------------------------- formatting

inline void AppendEscaped(std::string& out, unsigned long code) {
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "\\u%04lX", code);
    out += buffer;
}

inline std::string Show(bool value) { return value ? "true" : "false"; }
inline std::string Show(int value) { return std::to_string(value); }
inline std::string Show(unsigned int value) { return std::to_string(value); }
inline std::string Show(long long value) { return std::to_string(value); }
inline std::string Show(unsigned long long value) { return std::to_string(value); }

inline std::string Show(const std::string& value) {
    std::string out = "\"";
    size_t shown = 0;
    for (unsigned char ch : value) {
        if (shown >= 400) { out += "\"... (truncated, " + std::to_string(value.size()) + " bytes)"; return out; }
        if (ch >= 0x20 && ch < 0x7F) { out.push_back(static_cast<char>(ch)); }
        else { AppendEscaped(out, ch); }
        ++shown;
    }
    out += "\" (" + std::to_string(value.size()) + " bytes)";
    return out;
}

inline std::string Show(const std::wstring& value) {
    std::string out = "\"";
    size_t shown = 0;
    for (wchar_t ch : value) {
        if (shown >= 400) { out += "\"... (truncated, " + std::to_string(value.size()) + " chars)"; return out; }
        if (ch >= 0x20 && ch < 0x7F) { out.push_back(static_cast<char>(ch)); }
        else { AppendEscaped(out, static_cast<unsigned long>(static_cast<unsigned short>(ch))); }
        ++shown;
    }
    out += "\" (" + std::to_string(value.size()) + " chars)";
    return out;
}

inline std::string Show(const char* value) { return value ? Show(std::string(value)) : std::string("(null)"); }
inline std::string Show(const wchar_t* value) { return value ? Show(std::wstring(value)) : std::string("(null)"); }

// ---------------------------------------------------------------- assertions

inline const char*& ActiveTestName() { static const char* name = "<none>"; return name; }

inline void Fail(const char* file, int line, const char* expression, const std::string& detail) {
    ++CurrentTestFailures();
    if (SuppressFailureOutput()) {
        return;  // an expected failure; the runner prints one XFAIL line instead
    }
    ++TotalFailedAssertions();
    std::printf("    FAIL  %s\n          at %s:%d\n          check: %s\n", ActiveTestName(), file, line, expression);
    if (!detail.empty()) {
        std::printf("%s", detail.c_str());
    }
    std::fflush(stdout);
}

#define MDPP_CHECK(expr)                                                                      \
    do {                                                                                      \
        ++::mdpptest::TotalAssertions();                                                      \
        if (!(expr)) { ::mdpptest::Fail(__FILE__, __LINE__, #expr, std::string()); }          \
    } while (false)

#define MDPP_CHECK_EQ(actual, expected)                                                       \
    do {                                                                                      \
        ++::mdpptest::TotalAssertions();                                                      \
        const auto& mdpp_actual = (actual);                                                   \
        const auto& mdpp_expected = (expected);                                               \
        if (!(mdpp_actual == mdpp_expected)) {                                                \
            ::mdpptest::Fail(__FILE__, __LINE__, #actual " == " #expected,                    \
                             "          actual:   " + ::mdpptest::Show(mdpp_actual) + "\n" +  \
                             "          expected: " + ::mdpptest::Show(mdpp_expected) + "\n");\
        }                                                                                     \
    } while (false)

#define MDPP_CHECK_CONTAINS(haystack, needle)                                                 \
    do {                                                                                      \
        ++::mdpptest::TotalAssertions();                                                      \
        const auto& mdpp_hay = (haystack);                                                    \
        const auto& mdpp_needle = (needle);                                                   \
        if (mdpp_hay.find(mdpp_needle) == std::decay_t<decltype(mdpp_hay)>::npos) {           \
            ::mdpptest::Fail(__FILE__, __LINE__, #haystack " contains " #needle,              \
                             "          missing:  " + ::mdpptest::Show(mdpp_needle) + "\n" +  \
                             "          haystack: " + ::mdpptest::Show(mdpp_hay) + "\n");     \
        }                                                                                     \
    } while (false)

#define MDPP_CHECK_NOT_CONTAINS(haystack, needle)                                             \
    do {                                                                                      \
        ++::mdpptest::TotalAssertions();                                                      \
        const auto& mdpp_hay = (haystack);                                                    \
        const auto& mdpp_needle = (needle);                                                   \
        if (mdpp_hay.find(mdpp_needle) != std::decay_t<decltype(mdpp_hay)>::npos) {           \
            ::mdpptest::Fail(__FILE__, __LINE__, #haystack " does not contain " #needle,      \
                             "          forbidden: " + ::mdpptest::Show(mdpp_needle) + "\n" + \
                             "          haystack:  " + ::mdpptest::Show(mdpp_hay) + "\n");    \
        }                                                                                     \
    } while (false)

#define MDPP_TEST(suite, name)                                                                \
    static void mdpp_body_##suite##_##name();                                                 \
    static const ::mdpptest::Registrar mdpp_reg_##suite##_##name(                             \
        #suite, #name, &mdpp_body_##suite##_##name);                                          \
    static void mdpp_body_##suite##_##name()

// A test that asserts the CORRECT behaviour of something that is broken in the
// product today. Its checks are expected to fail; the run stays green but prints
// an XFAIL line every time, and the moment the bug is fixed the test turns red so
// nobody can quietly leave it parked here.
#define MDPP_KNOWN_BUG_TEST(suite, name, reason)                                              \
    static void mdpp_body_##suite##_##name();                                                 \
    static const ::mdpptest::Registrar mdpp_reg_##suite##_##name(                             \
        #suite, #name, &mdpp_body_##suite##_##name, reason);                                  \
    static void mdpp_body_##suite##_##name()

// ------------------------------------------------------------------- running

// "--known-bugs" selects exactly the MDPP_KNOWN_BUG_TEST cases; anything else is
// a plain substring match against "suite.TestName".
inline bool Selected(const TestCase& test, const char* filter) {
    if (!filter || !*filter) { return true; }
    if (std::string(filter) == "--known-bugs") { return test.knownBug != nullptr; }
    const std::string full = std::string(test.suite) + "." + test.name;
    return full.find(filter) != std::string::npos;
}

inline int RunAll(int argc, char** argv) {
    const char* filter = (argc > 1) ? argv[1] : nullptr;
    int selected = 0;
    int failedTests = 0;
    std::vector<std::string> knownBugs;

    for (const TestCase& test : Registry()) {
        if (!Selected(test, filter)) { continue; }
        ++selected;
        const std::string full = std::string(test.suite) + "." + test.name;
        ActiveTestName() = full.c_str();
        CurrentTestFailures() = 0;
        SuppressFailureOutput() = (test.knownBug != nullptr);
        test.fn();
        SuppressFailureOutput() = false;

        if (test.knownBug != nullptr) {
            if (CurrentTestFailures() > 0) {
                knownBugs.push_back(full + ": " + test.knownBug);
                std::printf("  [XFAIL] %s (%d check(s) still failing) - %s\n",
                            full.c_str(), CurrentTestFailures(), test.knownBug);
            } else {
                ++failedTests;
                std::printf("  [FAILED] %s - this test is marked MDPP_KNOWN_BUG_TEST but every check now\n"
                            "           passes. The bug looks fixed; promote it to MDPP_TEST.\n"
                            "           Recorded reason: %s\n", full.c_str(), test.knownBug);
            }
        } else if (CurrentTestFailures() > 0) {
            ++failedTests;
            std::printf("  [FAILED] %s (%d failed check(s))\n", full.c_str(), CurrentTestFailures());
        } else {
            std::printf("  [ok] %s\n", full.c_str());
        }
        ActiveTestName() = "<none>";
    }

    // A filter that matches nothing must be an error, not a silent pass: that is
    // exactly how a renamed test disappears from CI without anyone noticing.
    if (selected == 0) {
        // ...with one exception. "--known-bugs" selecting nothing means every known bug has
        // been fixed and its test promoted, which is the outcome this whole mechanism exists
        // to drive. Treating that as an error would leave the suite permanently red for
        // succeeding. Still prints a definite line so the run never looks ambiguous.
        if (filter && std::string(filter) == "--known-bugs") {
            std::printf("NO KNOWN PRODUCT BUGS recorded in this suite.\n");
            return 0;
        }
        std::printf("ERROR: filter '%s' selected 0 of %d registered tests.\n",
                    filter ? filter : "", static_cast<int>(Registry().size()));
        return 2;
    }

    // Say it on every run, not just when something is red: a suite carrying known
    // failures is running degraded and must not look identical to a clean one.
    if (!knownBugs.empty()) {
        std::printf("\n  *** %d KNOWN PRODUCT BUG(S) - these checks assert correct behaviour and are\n"
                    "      failing right now. This run is NOT a clean bill of health. ***\n",
                    static_cast<int>(knownBugs.size()));
        for (const std::string& entry : knownBugs) {
            std::printf("      - %s\n", entry.c_str());
        }
        std::printf("\n");
    }

    std::printf("%d test(s), %d assertion(s), %d failed assertion(s), %d failed test(s), %d known bug(s)\n",
                selected, TotalAssertions(), TotalFailedAssertions(), failedTests,
                static_cast<int>(knownBugs.size()));
    return failedTests == 0 ? 0 : 1;
}

}  // namespace mdpptest
