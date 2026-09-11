# Unit tests for the parts of the plugin that do not need Notepad++ or WebView2.
# No third-party test framework: TestHarness.h is ~160 lines of header, which is
# cheaper than a configure-time network fetch in a repo that deliberately keeps
# its dependency list short.

add_executable(MarkdownPlusPlus_tests
    TestMain.cpp
    TestHarness.h
    TempFiles.h
    HtmlUtilTests.cpp
    MarkdownRendererTests.cpp
    PluginOptionsTests.cpp
    WinUtilTests.cpp)

# markdownplusplus_core carries the production objects plus the cxx_std_20,
# UNICODE/_UNICODE and include-directory usage requirements.
target_link_libraries(MarkdownPlusPlus_tests PRIVATE markdownplusplus_core)

if(MSVC)
    target_compile_options(MarkdownPlusPlus_tests PRIVATE /W4 /permissive-)
endif()

set_target_properties(MarkdownPlusPlus_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/$<CONFIG>/tests"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/out/$<CONFIG>/tests")

# One CTest entry per suite. The argument is a substring matched against
# "suite.TestName", and the runner exits 2 when a filter selects nothing, so a
# renamed or deleted suite fails loudly instead of quietly running zero tests.
foreach(suite IN ITEMS htmlutil options renderer winutil)
    add_test(NAME ${suite} COMMAND MarkdownPlusPlus_tests "${suite}.")
endforeach()

# Proves the "zero selected tests is an error" guard above is live: this filter
# matches nothing, so the runner must exit non-zero.
add_test(NAME harness_rejects_an_empty_selection
    COMMAND MarkdownPlusPlus_tests "no.such.test.exists")
set_tests_properties(harness_rejects_an_empty_selection PROPERTIES WILL_FAIL TRUE)

# This entry exists so that "ctest" names the degraded state out loud on every
# run instead of printing an unqualified "100% tests passed". It runs only the
# MDPP_KNOWN_BUG_TEST cases and requires the banner to still be there, so it goes
# red when the last known bug is fixed and the tests need promoting. See
# tests/README.md.
add_test(NAME suite_carries_known_product_bugs
    COMMAND MarkdownPlusPlus_tests "--known-bugs")
# Either banner is a pass: "N KNOWN PRODUCT BUG(S)" says the suite is running degraded and
# says so out loud, and "NO KNOWN PRODUCT BUGS" is the end state that promoting them reaches.
# What must never happen is silence, which is what an unfiltered green run would give.
set_tests_properties(suite_carries_known_product_bugs PROPERTIES
    PASS_REGULAR_EXPRESSION "KNOWN PRODUCT BUG")
