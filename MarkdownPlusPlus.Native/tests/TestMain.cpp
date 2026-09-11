#include "TestHarness.h"

// Usage: MarkdownPlusPlus_tests [filter]
//   filter is a substring matched against "suite.TestName". CTest registers one
//   entry per suite by passing "renderer.", "options.", and so on.
int main(int argc, char** argv) {
    return mdpptest::RunAll(argc, argv);
}
