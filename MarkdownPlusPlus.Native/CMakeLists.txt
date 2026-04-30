if(NOT WIN32)
    add_custom_target(MarkdownPlusPlus
        COMMAND ${CMAKE_COMMAND} -E echo "Markdown++ native plugin is Windows-only. Configure with Visual Studio 18 2026 on Windows to build it.")
    return()
endif()

enable_language(RC)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Markdown++ native is currently x64-only. Configure CMake with -A x64.")
endif()

set(MARKDOWNPP_WEBVIEW2_VERSION "1.0.3912.50" CACHE STRING "Microsoft.Web.WebView2 native SDK version")
set(MARKDOWNPP_WEBVIEW2_ROOT
    "${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2.${MARKDOWNPP_WEBVIEW2_VERSION}/build/native"
    CACHE PATH "Path to the Microsoft.Web.WebView2 native build folder")

if(NOT EXISTS "${MARKDOWNPP_WEBVIEW2_ROOT}/include/WebView2.h")
    message(FATAL_ERROR
        "WebView2 SDK not found at ${MARKDOWNPP_WEBVIEW2_ROOT}. "
        "Run: python tools/install-native-deps.py")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/cmark-gfm/CMakeLists.txt")
    message(FATAL_ERROR
        "cmark-gfm source not found under third_party/cmark-gfm. "
        "Run: python tools/install-native-deps.py")
endif()

set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_STATIC ON CACHE BOOL "" FORCE)
set(CMARK_LIB_FUZZER OFF CACHE BOOL "" FORCE)
set(CMARK_FUZZ_QUADRATIC OFF CACHE BOOL "" FORCE)

add_subdirectory(
    "${CMAKE_SOURCE_DIR}/third_party/cmark-gfm"
    "${CMAKE_BINARY_DIR}/third_party/cmark-gfm"
    EXCLUDE_FROM_ALL)

add_library(MarkdownPlusPlus MODULE
    src/ClipboardUtil.cpp
    src/ClipboardUtil.h
    src/HtmlUtil.cpp
    src/HtmlUtil.h
    src/MarkdownPlusPlus.rc
    src/MarkdownRenderer.cpp
    src/MarkdownRenderer.h
    src/NppApi.h
    src/PluginOptions.cpp
    src/PluginOptions.h
    src/PluginController.cpp
    src/PluginController.h
    src/PluginExports.cpp
    src/PreviewWindow.cpp
    src/PreviewWindow.h
    src/WebViewHost.cpp
    src/WebViewHost.h
    src/WinUtil.cpp
    src/WinUtil.h
    src/resource.h)

target_compile_features(MarkdownPlusPlus PRIVATE cxx_std_20)

target_compile_definitions(MarkdownPlusPlus PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    CMARK_GFM_STATIC_DEFINE
    CMARK_GFM_EXTENSIONS_STATIC_DEFINE)

target_include_directories(MarkdownPlusPlus PRIVATE
    "${MARKDOWNPP_WEBVIEW2_ROOT}/include"
    "${CMAKE_SOURCE_DIR}/third_party/cmark-gfm/src"
    "${CMAKE_BINARY_DIR}/third_party/cmark-gfm/src"
    "${CMAKE_SOURCE_DIR}/third_party/cmark-gfm/extensions"
    "${CMAKE_BINARY_DIR}/third_party/cmark-gfm/extensions")

target_link_libraries(MarkdownPlusPlus PRIVATE
    "${MARKDOWNPP_WEBVIEW2_ROOT}/x64/WebView2LoaderStatic.lib"
    libcmark-gfm-extensions_static
    libcmark-gfm_static
    comctl32
    comdlg32
    gdi32
    ole32
    shell32
    shlwapi
    user32)

if(MSVC)
    target_compile_options(MarkdownPlusPlus PRIVATE /W4 /permissive-)
endif()

set_target_properties(MarkdownPlusPlus PROPERTIES
    OUTPUT_NAME "MarkdownPlusPlus"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/$<CONFIG>")

add_custom_command(TARGET MarkdownPlusPlus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:MarkdownPlusPlus>/assets"
    VERBATIM)

set(MARKDOWNPP_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package/$<CONFIG>/MarkdownPlusPlus")

add_custom_target(markdownplusplus_package
    DEPENDS MarkdownPlusPlus
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${MARKDOWNPP_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MARKDOWNPP_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:MarkdownPlusPlus>" "${MARKDOWNPP_PACKAGE_DIR}/MarkdownPlusPlus.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${MARKDOWNPP_PACKAGE_DIR}/assets"
    COMMENT "Packaging Markdown++ native plugin folder"
    VERBATIM)

install(TARGETS MarkdownPlusPlus
    RUNTIME DESTINATION MarkdownPlusPlus
    LIBRARY DESTINATION MarkdownPlusPlus)

install(DIRECTORY assets DESTINATION MarkdownPlusPlus)
