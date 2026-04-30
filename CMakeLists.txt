cmake_minimum_required(VERSION 3.24)

project(MarkdownPlusPlus
    VERSION 0.1.0
    DESCRIPTION "Native Markdown++ Notepad++ plugin"
    LANGUAGES C CXX)

option(MARKDOWNPP_BUILD_NATIVE "Build the native Markdown++ Notepad++ plugin" ON)

if(MARKDOWNPP_BUILD_NATIVE)
    add_subdirectory(MarkdownPlusPlus.Native)
endif()
