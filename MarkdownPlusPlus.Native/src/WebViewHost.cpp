#include "WebViewHost.h"

#include "HtmlUtil.h"
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

using Microsoft::WRL::Callback;

namespace markdownplusplus {
namespace {

constexpr wchar_t kMessageWindowClass[] = L"MarkdownPlusPlusRuntimeMessage";
constexpr wchar_t kWebView2DownloadUrl[] = L"https://developer.microsoft.com/en-us/microsoft-edge/webview2/";
constexpr wchar_t kDocumentHostName[] = L"markdownplusplus.document";
constexpr wchar_t kLocalHostUrl[] = L"https://markdownplusplus.local";

struct RuntimeProbe {
    HRESULT result = E_FAIL;
    std::wstring version;

    bool Available() const {
        return SUCCEEDED(result) && !version.empty();
    }
};

std::wstring HResultToHex(HRESULT result) {
    wchar_t buffer[16] = {};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned int>(result));
    return buffer;
}

double ClampScrollRatio(double ratio) {
    if (!(ratio >= 0.0) && !(ratio <= 1.0)) {
        return 0.0;
    }

    return std::clamp(ratio, 0.0, 1.0);
}

bool TryParseJsonDouble(const std::wstring& message, const std::wstring& key, double& value) {
    std::wstring marker = L"\"";
    marker += key;
    marker += L"\":";
    size_t position = message.find(marker);
    if (position == std::wstring::npos) {
        return false;
    }

    const wchar_t* start = message.c_str() + position + marker.size();
    wchar_t* end = nullptr;
    double parsed = wcstod(start, &end);
    if (end == start) {
        return false;
    }

    value = parsed;
    return true;
}

bool TryParseJsonInt(const std::wstring& message, const std::wstring& key, int& value) {
    std::wstring marker = L"\"";
    marker += key;
    marker += L"\":";
    size_t position = message.find(marker);
    if (position == std::wstring::npos) {
        return false;
    }

    const wchar_t* start = message.c_str() + position + marker.size();
    wchar_t* end = nullptr;
    long parsed = wcstol(start, &end, 10);
    if (end == start) {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool TryParseJsonString(const std::wstring& message, const std::wstring& key, std::wstring& value) {
    std::wstring marker = L"\"";
    marker += key;
    marker += L"\":\"";
    size_t position = message.find(marker);
    if (position == std::wstring::npos) {
        return false;
    }

    position += marker.size();
    std::wstring parsed;
    parsed.reserve(message.size() - position);

    for (size_t index = position; index < message.size(); ++index) {
        wchar_t ch = message[index];
        if (ch == L'"') {
            value = std::move(parsed);
            return true;
        }

        if (ch != L'\\') {
            parsed.push_back(ch);
            continue;
        }

        if (++index >= message.size()) {
            return false;
        }

        wchar_t escaped = message[index];
        switch (escaped) {
            case L'"':
            case L'\\':
            case L'/':
                parsed.push_back(escaped);
                break;
            case L'b':
                parsed.push_back(L'\b');
                break;
            case L'f':
                parsed.push_back(L'\f');
                break;
            case L'n':
                parsed.push_back(L'\n');
                break;
            case L'r':
                parsed.push_back(L'\r');
                break;
            case L't':
                parsed.push_back(L'\t');
                break;
            case L'u': {
                if (index + 4 >= message.size()) {
                    return false;
                }

                wchar_t* end = nullptr;
                wchar_t hex[5] = {
                    message[index + 1],
                    message[index + 2],
                    message[index + 3],
                    message[index + 4],
                    L'\0'};
                unsigned long codepoint = wcstoul(hex, &end, 16);
                if (end != hex + 4) {
                    return false;
                }

                parsed.push_back(static_cast<wchar_t>(codepoint));
                index += 4;
                break;
            }
            default:
                return false;
        }
    }

    return false;
}

bool TryParseJsonBool(const std::wstring& message, const std::wstring& key, bool& value) {
    std::wstring marker = L"\"";
    marker += key;
    marker += L"\":";
    size_t position = message.find(marker);
    if (position == std::wstring::npos) {
        return false;
    }

    const wchar_t* start = message.c_str() + position + marker.size();
    if (wcsncmp(start, L"true", 4) == 0) {
        value = true;
        return true;
    }

    if (wcsncmp(start, L"false", 5) == 0) {
        value = false;
        return true;
    }

    return false;
}

bool TryParsePreviewScrollMessage(const std::wstring& message, int& sourceLine, double& ratio, bool& force) {
    if (message.find(L"\"type\":\"previewScroll\"") == std::wstring::npos) {
        return false;
    }

    sourceLine = 0;
    ratio = 0.0;
    force = false;
    TryParseJsonInt(message, L"line", sourceLine);
    TryParseJsonBool(message, L"force", force);
    double parsedRatio = 0.0;
    if (TryParseJsonDouble(message, L"ratio", parsedRatio)) {
        ratio = ClampScrollRatio(parsedRatio);
    }
    return true;
}

bool TryParsePreviewLinkMessage(const std::wstring& message, std::wstring& href) {
    if (message.find(L"\"type\":\"linkClick\"") == std::wstring::npos) {
        return false;
    }

    return TryParseJsonString(message, L"href", href) && !href.empty();
}

bool StartsWithIgnoreCase(const std::wstring& value, const wchar_t* prefix) {
    const size_t prefixLength = wcslen(prefix);
    return value.size() >= prefixLength && _wcsnicmp(value.c_str(), prefix, prefixLength) == 0;
}

bool HasUrlHost(const std::wstring& value, const wchar_t* schemeAndHost) {
    const size_t length = wcslen(schemeAndHost);
    if (!StartsWithIgnoreCase(value, schemeAndHost)) {
        return false;
    }

    return value.size() == length || value[length] == L'/' || value[length] == L'?' || value[length] == L'#';
}

bool IsRoutablePreviewNavigation(const std::wstring& uri) {
    if (uri.empty() || HasUrlHost(uri, kLocalHostUrl)) {
        return false;
    }

    return StartsWithIgnoreCase(uri, L"http://") ||
           StartsWithIgnoreCase(uri, L"https://") ||
           StartsWithIgnoreCase(uri, L"file:");
}

bool DirectoryExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool EnsureDirectory(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }

    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }

    return GetLastError() == ERROR_ALREADY_EXISTS && DirectoryExists(path);
}

std::wstring AppendPathSegment(const std::wstring& root, const wchar_t* segment) {
    std::wstring path = root;
    if (!path.empty() && path.back() != L'\\' && path.back() != L'/') {
        path.push_back(L'\\');
    }

    path += segment;
    return path;
}

std::wstring PrepareWebViewUserDataFolder(const std::wstring& root) {
    std::wstring appFolder = AppendPathSegment(root, L"MarkdownPlusPlus");
    std::wstring webViewFolder = AppendPathSegment(appFolder, L"WebView2");

    if (!EnsureDirectory(appFolder) || !EnsureDirectory(webViewFolder)) {
        return {};
    }

    return webViewFolder;
}

std::wstring BuildWebViewUserDataFolder() {
    PWSTR localAppData = nullptr;
    HRESULT folderResult = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData);
    if (SUCCEEDED(folderResult) && localAppData != nullptr) {
        std::wstring folder = PrepareWebViewUserDataFolder(localAppData);
        CoTaskMemFree(localAppData);

        if (!folder.empty()) {
            return folder;
        }
    } else {
        CoTaskMemFree(localAppData);
    }

    std::vector<wchar_t> tempPath(MAX_PATH);
    DWORD tempLength = GetTempPathW(static_cast<DWORD>(tempPath.size()), tempPath.data());
    if (tempLength == 0) {
        return {};
    }

    if (tempLength >= tempPath.size()) {
        tempPath.resize(static_cast<size_t>(tempLength) + 1);
        tempLength = GetTempPathW(static_cast<DWORD>(tempPath.size()), tempPath.data());
        if (tempLength == 0 || tempLength >= tempPath.size()) {
            return {};
        }
    }

    return PrepareWebViewUserDataFolder(tempPath.data());
}

RuntimeProbe ProbeWebView2Runtime() {
    RuntimeProbe probe;
    LPWSTR versionInfo = nullptr;
    probe.result = GetAvailableCoreWebView2BrowserVersionString(nullptr, &versionInfo);
    if (SUCCEEDED(probe.result) && versionInfo != nullptr) {
        probe.version = versionInfo;
    }

    CoTaskMemFree(versionInfo);
    return probe;
}

std::wstring UserDataFolderLine(const std::wstring& userDataFolder) {
    if (userDataFolder.empty()) {
        return L"WebView2 user data folder: unavailable\r\n";
    }

    return L"WebView2 user data folder: " + userDataFolder + L"\r\n";
}

std::wstring WebViewRuntimeMessage(HRESULT startupResult, const RuntimeProbe& probe, const std::wstring& userDataFolder) {
    return L"Markdown++ requires the Microsoft Edge WebView2 Evergreen Runtime.\r\n\r\n"
           L"Install the runtime from Microsoft, then restart Notepad++.\r\n\r\n"
           L"Click this message to open the WebView2 download page:\r\n" +
           std::wstring(kWebView2DownloadUrl) + L"\r\n\r\n"
           + UserDataFolderLine(userDataFolder) +
           L"WebView2 startup result: " +
           HResultToHex(startupResult) + L"\r\n"
           L"WebView2 version query result: " +
           HResultToHex(probe.result);
}

std::wstring WebViewStartupFailureMessage(HRESULT startupResult, const RuntimeProbe& probe, const std::wstring& userDataFolder) {
    std::wstring message =
        L"Markdown++ found Microsoft Edge WebView2 Runtime";

    if (!probe.version.empty()) {
        message += L" version " + probe.version;
    }

    message +=
        L", but could not start the preview surface.\r\n\r\n"
        L"Restart Notepad++. If this keeps happening, repair or reinstall WebView2 from Microsoft:\r\n" +
        std::wstring(kWebView2DownloadUrl) + L"\r\n\r\n"
        + UserDataFolderLine(userDataFolder) +
        L"WebView2 startup result: " +
        HResultToHex(startupResult) + L"\r\n"
        L"WebView2 version query result: " +
        HResultToHex(probe.result);

    return message;
}

std::wstring WebViewStartupMessage(HRESULT startupResult, const RuntimeProbe& probe, const std::wstring& userDataFolder) {
    if (probe.Available()) {
        return WebViewStartupFailureMessage(startupResult, probe, userDataFolder);
    }

    return WebViewRuntimeMessage(startupResult, probe, userDataFolder);
}

std::wstring WebViewFailureMessage(const wchar_t* message, HRESULT result) {
    return std::wstring(message) + L"\r\n\r\nWebView2 result: " + HResultToHex(result);
}

LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;

        case WM_LBUTTONUP:
            ShellExecuteW(hwnd, L"open", kWebView2DownloadUrl, nullptr, nullptr, SW_SHOWNORMAL);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT client{};
            GetClientRect(hwnd, &client);

            HBRUSH background = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &client, background);
            DeleteObject(background);

            int length = GetWindowTextLengthW(hwnd);
            std::vector<wchar_t> text(static_cast<size_t>(length) + 1, L'\0');
            if (length > 0) {
                GetWindowTextW(hwnd, text.data(), static_cast<int>(text.size()));
            }

            RECT textBounds = client;
            InflateRect(&textBounds, -18, -16);

            HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
            if (!font) {
                font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            }

            HGDIOBJ oldFont = SelectObject(hdc, font);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(238, 238, 238));
            DrawTextW(hdc, text.data(), -1, &textBounds, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

            if (oldFont) {
                SelectObject(hdc, oldFont);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void RegisterMessageWindowClass(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = MessageWindowProc;
    wc.lpszClassName = kMessageWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    atom = RegisterClassExW(&wc);
}

}  // namespace

WebViewHost::~WebViewHost() {
    Destroy();
}

HRESULT WebViewHost::Create(HWND parent, const std::wstring& assetRoot) {
    parent_ = parent;
    assetRoot_ = assetRoot;

    HRESULT coResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(coResult)) {
        coInitialized_ = true;
    } else if (coResult != RPC_E_CHANGED_MODE) {
        ShowMessage(WebViewFailureMessage(L"Markdown++ could not initialize COM for WebView2.", coResult));
        return coResult;
    }

    RuntimeProbe runtime = ProbeWebView2Runtime();
    webViewUserDataFolder_ = BuildWebViewUserDataFolder();

    HRESULT result = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        webViewUserDataFolder_.empty() ? nullptr : webViewUserDataFolder_.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, runtime, userDataFolder = webViewUserDataFolder_](HRESULT environmentResult, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(environmentResult) || environment == nullptr) {
                    HRESULT effectiveResult = FAILED(environmentResult) ? environmentResult : E_POINTER;
                    ShowMessage(WebViewStartupMessage(effectiveResult, runtime, userDataFolder));
                    return S_OK;
                }

                environment_ = environment;
                environment_->CreateCoreWebView2Controller(
                    parent_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || controller == nullptr) {
                                HRESULT effectiveResult = FAILED(controllerResult) ? controllerResult : E_POINTER;
                                ShowMessage(WebViewFailureMessage(L"Markdown++ could not create the WebView2 preview surface.", effectiveResult));
                                return S_OK;
                            }

                            controller_ = controller;
                            controller_->get_CoreWebView2(&webView_);

                            if (webView_) {
                                Microsoft::WRL::ComPtr<ICoreWebView2_3> webView3;
                                if (SUCCEEDED(webView_.As(&webView3)) && !assetRoot_.empty()) {
                                    webView3->SetVirtualHostNameToFolderMapping(
                                        L"markdownplusplus.local",
                                        assetRoot_.c_str(),
                                        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
                                }

                                webView_->add_NavigationStarting(
                                    Callback<ICoreWebView2NavigationStartingEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                            BOOL userInitiated = FALSE;
                                            args->get_IsUserInitiated(&userInitiated);
                                            if (userInitiated) {
                                                args->put_Cancel(TRUE);

                                                LPWSTR uri = nullptr;
                                                if (linkCallback_ && SUCCEEDED(args->get_Uri(&uri)) && uri != nullptr) {
                                                    std::wstring href(uri);
                                                    CoTaskMemFree(uri);
                                                    if (IsRoutablePreviewNavigation(href)) {
                                                        linkCallback_(href);
                                                    }
                                                } else {
                                                    CoTaskMemFree(uri);
                                                }
                                            }
                                            return S_OK;
                                        })
                                        .Get(),
                                    &navigationStartingToken_);

                                webView_->add_NewWindowRequested(
                                    Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                                            args->put_Handled(TRUE);

                                            LPWSTR uri = nullptr;
                                            if (linkCallback_ && SUCCEEDED(args->get_Uri(&uri)) && uri != nullptr) {
                                                std::wstring href(uri);
                                                CoTaskMemFree(uri);
                                                if (IsRoutablePreviewNavigation(href)) {
                                                    linkCallback_(href);
                                                }
                                            } else {
                                                CoTaskMemFree(uri);
                                            }

                                            return S_OK;
                                        })
                                        .Get(),
                                    &newWindowRequestedToken_);

                                webView_->add_NavigationCompleted(
                                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                            documentLoaded_ = true;
                                            ApplyPendingScrollRatio();
                                            return S_OK;
                                        })
                                        .Get(),
                                    &navigationCompletedToken_);

                                webView_->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            HandleWebMessage(args);
                                            return S_OK;
                                        })
                                        .Get(),
                                    &webMessageReceivedToken_);
                            }

                            ready_ = true;
                            Resize();
                            ApplyPendingDocument();
                            return S_OK;
                        })
                        .Get());

                return S_OK;
            })
            .Get());

    if (FAILED(result)) {
        ShowMessage(WebViewStartupMessage(result, runtime, webViewUserDataFolder_));
    }

    return result;
}

void WebViewHost::Destroy() {
    ready_ = false;
    pendingDocument_.clear();
    pendingArticleHtml_.clear();
    pendingTitle_.clear();
    pendingDocumentRoot_.clear();
    mappedDocumentRoot_.clear();
    hasPendingScrollRatio_ = false;
    documentLoaded_ = false;
    pendingDocumentRevision_ = 0;

    if (controller_) {
        controller_->Close();
    }

    webView_.Reset();
    controller_.Reset();
    environment_.Reset();

    if (messageWindow_) {
        DestroyWindow(messageWindow_);
        messageWindow_ = nullptr;
    }

    if (coInitialized_) {
        CoUninitialize();
        coInitialized_ = false;
    }
}

void WebViewHost::Resize() {
    if (messageWindow_) {
        RECT rc{};
        GetClientRect(parent_, &rc);
        MoveWindow(messageWindow_, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    }

    if (!controller_) {
        return;
    }

    RECT bounds{};
    GetClientRect(parent_, &bounds);
    controller_->put_Bounds(bounds);
}

void WebViewHost::NavigateToString(
    const std::wstring& document,
    const std::wstring& articleHtml,
    const std::wstring& title,
    const std::wstring& documentRoot,
    int initialSourceLine,
    double initialScrollRatio,
    double initialAnchorRatio,
    bool mermaidEnabled) {
    pendingDocument_ = document;
    pendingArticleHtml_ = articleHtml;
    pendingTitle_ = title;
    pendingDocumentRoot_ = documentRoot;
    pendingSourceLine_ = std::max(0, initialSourceLine);
    pendingScrollRatio_ = ClampScrollRatio(initialScrollRatio);
    pendingAnchorRatio_ = ClampScrollRatio(initialAnchorRatio);
    pendingMermaidEnabled_ = mermaidEnabled;
    hasPendingScrollRatio_ = true;
    ++pendingDocumentRevision_;
    ApplyPendingDocument();
}

void WebViewHost::SetScrollCallback(ScrollCallback callback) {
    scrollCallback_ = std::move(callback);
}

void WebViewHost::SetLinkCallback(LinkCallback callback) {
    linkCallback_ = std::move(callback);
}

void WebViewHost::SetScrollRatio(double ratio) {
    pendingScrollRatio_ = ClampScrollRatio(ratio);
    pendingAnchorRatio_ = 0.0;
    pendingSourceLine_ = 0;
    hasPendingScrollRatio_ = true;
    ApplyPendingScrollRatio();
}

void WebViewHost::SetScrollSourceLine(int sourceLine, double fallbackRatio, double anchorRatio) {
    pendingSourceLine_ = std::max(0, sourceLine);
    pendingScrollRatio_ = ClampScrollRatio(fallbackRatio);
    pendingAnchorRatio_ = ClampScrollRatio(anchorRatio);
    hasPendingScrollRatio_ = true;
    ApplyPendingScrollRatio();
}

bool WebViewHost::ShowPrintUi() {
    if (!ready_ || !webView_ || !documentLoaded_) {
        ShowMessage(L"Markdown++ print is available after the preview finishes loading.");
        return false;
    }

    Microsoft::WRL::ComPtr<ICoreWebView2_16> printableWebView;
    if (FAILED(webView_.As(&printableWebView)) || !printableWebView) {
        ShowMessage(L"Markdown++ print requires a newer Microsoft Edge WebView2 Runtime.");
        return false;
    }

    HRESULT result = printableWebView->ShowPrintUI(COREWEBVIEW2_PRINT_DIALOG_KIND_SYSTEM);
    if (FAILED(result)) {
        ShowMessage(WebViewFailureMessage(L"Markdown++ could not open the print dialog.", result));
        return false;
    }

    return true;
}

bool WebViewHost::PrintToPdf(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }

    if (!ready_ || !webView_ || !documentLoaded_) {
        ShowMessage(L"Markdown++ PDF export is available after the preview finishes loading.");
        return false;
    }

    Microsoft::WRL::ComPtr<ICoreWebView2_7> pdfWebView;
    if (FAILED(webView_.As(&pdfWebView)) || !pdfWebView) {
        ShowMessage(L"Markdown++ PDF export requires a newer Microsoft Edge WebView2 Runtime.");
        return false;
    }

    webView_->ExecuteScript(
        L"window.MarkdownPlusPlusPreview&&window.MarkdownPlusPlusPreview.preparePdfExport&&window.MarkdownPlusPlusPreview.preparePdfExport();",
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [this, path, pdfWebView](HRESULT, LPCWSTR) -> HRESULT {
                StartPrintToPdf(path, pdfWebView);
                return S_OK;
            })
            .Get());

    return true;
}

bool WebViewHost::StartPrintToPdf(const std::wstring& path, Microsoft::WRL::ComPtr<ICoreWebView2_7> pdfWebView) {
    if (!pdfWebView) {
        RestorePdfExportLinks();
        return false;
    }

    const HWND owner = parent_;
    const HRESULT result = pdfWebView->PrintToPdf(
        path.c_str(),
        nullptr,
        Callback<ICoreWebView2PrintToPdfCompletedHandler>(
            [this, owner, path](HRESULT errorCode, BOOL succeeded) -> HRESULT {
                RestorePdfExportLinks();
                if (FAILED(errorCode) || !succeeded) {
                    MessageBoxW(
                        owner,
                        (L"Markdown++ could not export the PDF.\r\n\r\nWebView2 result: " + HResultToHex(errorCode)).c_str(),
                        L"Markdown++",
                        MB_OK | MB_ICONERROR);
                    return S_OK;
                }

                MessageBoxW(owner, (L"Markdown++ exported PDF:\r\n" + path).c_str(), L"Markdown++", MB_OK | MB_ICONINFORMATION);
                return S_OK;
            })
            .Get());

    if (FAILED(result)) {
        RestorePdfExportLinks();
        ShowMessage(WebViewFailureMessage(L"Markdown++ could not start PDF export.", result));
        return false;
    }

    return true;
}

void WebViewHost::RestorePdfExportLinks() {
    if (!ready_ || !webView_) {
        return;
    }

    webView_->ExecuteScript(
        L"window.MarkdownPlusPlusPreview&&window.MarkdownPlusPlusPreview.restorePdfExport&&window.MarkdownPlusPlusPreview.restorePdfExport();",
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [](HRESULT, LPCWSTR) -> HRESULT {
                return S_OK;
            })
            .Get());
}

void WebViewHost::ApplyPendingDocument() {
    if (!ready_ || !webView_ || pendingDocument_.empty()) {
        return;
    }

    UpdateDocumentHostMapping(pendingDocumentRoot_);
    if (documentLoaded_ && ApplyPendingContentUpdate()) {
        return;
    }

    documentLoaded_ = false;
    webView_->NavigateToString(pendingDocument_.c_str());
}

bool WebViewHost::ApplyPendingContentUpdate() {
    if (!ready_ || !webView_) {
        return false;
    }

    wchar_t ratio[64] = {};
    wchar_t anchor[64] = {};
    swprintf_s(ratio, L"%.8f", pendingScrollRatio_);
    swprintf_s(anchor, L"%.8f", pendingAnchorRatio_);

    const unsigned long long revision = pendingDocumentRevision_;
    std::wstring script;
    script.reserve(pendingArticleHtml_.size() + pendingTitle_.size() + 512);
    script += L"(function(){window.MarkdownPlusPlusOptions={mermaidEnabled:";
    script += pendingMermaidEnabled_ ? L"true" : L"false";
    script += L"};return !!(window.MarkdownPlusPlusPreview&&window.MarkdownPlusPlusPreview.replaceContent('";
    script += EscapeScriptString(pendingArticleHtml_);
    script += L"','";
    script += EscapeScriptString(pendingTitle_);
    script += L"',";
    script += std::to_wstring(pendingSourceLine_);
    script += L",";
    script += ratio;
    script += L",";
    script += anchor;
    script += L",";
    script += std::to_wstring(revision);
    script += L"));})();";

    webView_->ExecuteScript(
        script.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [this, revision](HRESULT result, LPCWSTR value) -> HRESULT {
                if ((FAILED(result) || value == nullptr || wcscmp(value, L"true") != 0) && revision == pendingDocumentRevision_) {
                    documentLoaded_ = false;
                    ApplyPendingDocument();
                }
                return S_OK;
            })
            .Get());

    return true;
}

void WebViewHost::ApplyPendingScrollRatio() {
    if (!ready_ || !webView_ || !hasPendingScrollRatio_) {
        return;
    }

    wchar_t script[256] = {};
    swprintf_s(
        script,
        L"window.MarkdownPlusPlusPreview&&window.MarkdownPlusPlusPreview.scrollToSourceLine(%d,%.8f,%.8f);",
        pendingSourceLine_,
        pendingScrollRatio_,
        pendingAnchorRatio_);

    webView_->ExecuteScript(
        script,
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [](HRESULT, LPCWSTR) -> HRESULT {
                return S_OK;
            })
            .Get());
}

void WebViewHost::HandleWebMessage(ICoreWebView2WebMessageReceivedEventArgs* args) {
    if (!args) {
        return;
    }

    LPWSTR json = nullptr;
    if (FAILED(args->get_WebMessageAsJson(&json)) || json == nullptr) {
        CoTaskMemFree(json);
        return;
    }

    std::wstring messageJson(json);
    int sourceLine = 0;
    double ratio = 0.0;
    bool force = false;
    std::wstring href;
    const bool parsedScroll = scrollCallback_ && TryParsePreviewScrollMessage(messageJson, sourceLine, ratio, force);
    const bool parsedLink = linkCallback_ && TryParsePreviewLinkMessage(messageJson, href);
    CoTaskMemFree(json);

    if (parsedScroll) {
        pendingSourceLine_ = sourceLine;
        pendingScrollRatio_ = ratio;
        scrollCallback_(sourceLine, ratio, force);
    } else if (parsedLink) {
        linkCallback_(href);
    }
}

void WebViewHost::UpdateDocumentHostMapping(const std::wstring& documentRoot) {
    if (!webView_ || mappedDocumentRoot_ == documentRoot) {
        return;
    }

    Microsoft::WRL::ComPtr<ICoreWebView2_3> webView3;
    if (FAILED(webView_.As(&webView3))) {
        return;
    }

    webView3->ClearVirtualHostNameToFolderMapping(kDocumentHostName);
    mappedDocumentRoot_.clear();

    if (documentRoot.empty()) {
        return;
    }

    HRESULT result = webView3->SetVirtualHostNameToFolderMapping(
        kDocumentHostName,
        documentRoot.c_str(),
        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);

    if (SUCCEEDED(result)) {
        mappedDocumentRoot_ = documentRoot;
    }
}

void WebViewHost::ShowMessage(const std::wstring& message) {
    if (!parent_) {
        return;
    }

    if (!messageWindow_) {
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent_, GWLP_HINSTANCE));
        RegisterMessageWindowClass(instance);

        messageWindow_ = CreateWindowExW(
            0,
            kMessageWindowClass,
            L"",
            WS_CHILD | WS_VISIBLE,
            0,
            0,
            0,
            0,
            parent_,
            nullptr,
            instance,
            nullptr);

        if (messageWindow_) {
            SendMessageW(messageWindow_, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
    }

    if (!messageWindow_) {
        return;
    }

    SetWindowTextW(messageWindow_, message.c_str());
    InvalidateRect(messageWindow_, nullptr, TRUE);
    Resize();
}

}  // namespace markdownplusplus
