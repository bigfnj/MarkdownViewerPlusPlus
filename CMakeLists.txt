cmake_minimum_required(VERSION 3.24)

project(MarkdownPlusPlus
    VERSION 1.3.0
    DESCRIPTION "Native Markdown++ Notepad++ plugin"
    LANGUAGES C CXX)

option(MARKDOWNPP_BUILD_NATIVE "Build the native Markdown++ Notepad++ plugin" ON)
option(MARKDOWNPP_BUILD_TESTS "Build the Markdown++ unit tests (ctest)" ON)

enable_testing()

if(MARKDOWNPP_BUILD_NATIVE)
    add_subdirectory(MarkdownPlusPlus.Native)
endif()
