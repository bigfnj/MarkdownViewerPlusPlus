#include "PreviewWindow.h"

#include "WinUtil.h"

#include <utility>

namespace markdownplusplus {
namespace {

constexpr wchar_t kPreviewWindowClass[] = L"MarkdownPlusPlusNativePreview";
constexpr wchar_t kPluginName[] = L"Markdown++";
constexpr wchar_t kModuleName[] = L"MarkdownPlusPlus";
constexpr UINT DMN_CLOSE = 1051;

}  // namespace

PreviewWindow::~PreviewWindow() {
    Destroy();
}

bool PreviewWindow::Create(HINSTANCE module, HWND notepadHandle, int commandId, CloseCallback onClose) {
    if (hwnd_) {
        return true;
    }

    module_ = module;
    notepadHandle_ = notepadHandle;
    onClose_ = std::move(onClose);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = module_;
    wc.lpfnWndProc = PreviewWindow::WindowProc;
    wc.lpszClassName = kPreviewWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        0,
        kPreviewWindowClass,
        kPluginName,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0,
        0,
        400,
        600,
        notepadHandle_,
        nullptr,
        module_,
        this);

    if (!hwnd_) {
        return false;
    }

    RegisterDockableWindow(commandId);
    webView_.Create(hwnd_, CombinePath(GetModuleDirectory(module_), L"assets"));
    return true;
}

void PreviewWindow::Destroy() {
    webView_.Destroy();

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void PreviewWindow::Show() {
    if (notepadHandle_ && hwnd_) {
        SendMessageW(notepadHandle_, npp::NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(hwnd_));
    }
}

void PreviewWindow::Hide() {
    if (notepadHandle_ && hwnd_) {
        SendMessageW(notepadHandle_, npp::NPPM_DMMHIDE, 0, reinterpret_cast<LPARAM>(hwnd_));
    }
}

void PreviewWindow::Resize() {
    webView_.Resize();
}

void PreviewWindow::SetDocument(
    const std::wstring& document,
    const std::wstring& articleHtml,
    const std::wstring& title,
    const std::wstring& documentRoot,
    int initialSourceLine,
    double initialScrollRatio,
    double initialAnchorRatio,
    bool mermaidEnabled) {
    webView_.NavigateToString(document, articleHtml, title, documentRoot, initialSourceLine, initialScrollRatio, initialAnchorRatio, mermaidEnabled);
}

void PreviewWindow::SetScrollCallback(ScrollCallback callback) {
    webView_.SetScrollCallback(std::move(callback));
}

void PreviewWindow::SetLinkCallback(LinkCallback callback) {
    webView_.SetLinkCallback(std::move(callback));
}

void PreviewWindow::SetScrollRatio(double ratio) {
    webView_.SetScrollRatio(ratio);
}

void PreviewWindow::SetScrollSourceLine(int sourceLine, double fallbackRatio, double anchorRatio) {
    webView_.SetScrollSourceLine(sourceLine, fallbackRatio, anchorRatio);
}

bool PreviewWindow::ShowPrintUi() {
    return webView_.ShowPrintUi();
}

bool PreviewWindow::PrintToPdf(const std::wstring& path) {
    return webView_.PrintToPdf(path);
}

LRESULT CALLBACK PreviewWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PreviewWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<PreviewWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<PreviewWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT PreviewWindow::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE:
            Resize();
            return 0;

        case WM_NOTIFY: {
            const auto* header = reinterpret_cast<NMHDR*>(lParam);
            if (header && header->code == DMN_CLOSE && onClose_) {
                onClose_();
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            webView_.Destroy();
            hwnd_ = nullptr;
            return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void PreviewWindow::RegisterDockableWindow(int commandId) {
    npp::NppTbData data{};
    data.hClient = hwnd_;
    data.pszName = kPluginName;
    data.dlgID = commandId;
    data.uMask = npp::DWS_DF_CONT_RIGHT;
    data.pszModuleName = kModuleName;
    SendMessageW(notepadHandle_, npp::NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&data));
}

}  // namespace markdownplusplus
