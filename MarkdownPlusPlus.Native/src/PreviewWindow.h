#pragma once

#include "NppApi.h"
#include "WebViewHost.h"

#include <functional>
#include <string>

namespace markdownplusplus {

class PreviewWindow {
public:
    using CloseCallback = std::function<void()>;
    using ScrollCallback = WebViewHost::ScrollCallback;
    using LinkCallback = WebViewHost::LinkCallback;

    PreviewWindow() = default;
    ~PreviewWindow();

    PreviewWindow(const PreviewWindow&) = delete;
    PreviewWindow& operator=(const PreviewWindow&) = delete;

    bool Create(HINSTANCE module, HWND notepadHandle, int commandId, CloseCallback onClose);
    void Destroy();
    void Show();
    void Hide();
    void Resize();
    void SetDocument(
        const std::wstring& document,
        const std::wstring& articleHtml,
        const std::wstring& title,
        const std::wstring& documentRoot,
        int initialSourceLine,
        double initialScrollRatio,
        double initialAnchorRatio,
        bool mermaidEnabled);
    void SetScrollCallback(ScrollCallback callback);
    void SetLinkCallback(LinkCallback callback);
    void SetScrollRatio(double ratio);
    void SetScrollSourceLine(int sourceLine, double fallbackRatio, double anchorRatio);
    bool ShowPrintUi();
    bool PrintToPdf(const std::wstring& path);
    bool Ready() const { return webView_.Ready(); }

    HWND Handle() const { return hwnd_; }
    bool Created() const { return hwnd_ != nullptr; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void RegisterDockableWindow(int commandId);

    HINSTANCE module_ = nullptr;
    HWND notepadHandle_ = nullptr;
    HWND hwnd_ = nullptr;
    WebViewHost webView_;
    CloseCallback onClose_;
};

}  // namespace markdownplusplus
