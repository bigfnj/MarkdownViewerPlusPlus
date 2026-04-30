#pragma once

#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

#include <functional>
#include <string>

namespace markdownplusplus {

class WebViewHost {
public:
    using ScrollCallback = std::function<void(int, double, bool)>;
    using LinkCallback = std::function<void(const std::wstring&)>;

    WebViewHost() = default;
    ~WebViewHost();

    WebViewHost(const WebViewHost&) = delete;
    WebViewHost& operator=(const WebViewHost&) = delete;

    HRESULT Create(HWND parent, const std::wstring& assetRoot);
    void Destroy();
    void Resize();
    void NavigateToString(
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
    bool Ready() const { return ready_ && webView_ != nullptr && documentLoaded_; }

private:
    void ShowMessage(const std::wstring& message);
    void ApplyPendingDocument();
    bool ApplyPendingContentUpdate();
    void ApplyPendingScrollRatio();
    void HandleWebMessage(ICoreWebView2WebMessageReceivedEventArgs* args);
    void UpdateDocumentHostMapping(const std::wstring& documentRoot);
    bool StartPrintToPdf(const std::wstring& path, Microsoft::WRL::ComPtr<ICoreWebView2_7> pdfWebView);
    void RestorePdfExportLinks();

    HWND parent_ = nullptr;
    HWND messageWindow_ = nullptr;
    std::wstring assetRoot_;
    std::wstring webViewUserDataFolder_;
    bool coInitialized_ = false;
    bool ready_ = false;
    EventRegistrationToken navigationStartingToken_{};
    EventRegistrationToken newWindowRequestedToken_{};
    EventRegistrationToken navigationCompletedToken_{};
    EventRegistrationToken webMessageReceivedToken_{};
    std::wstring pendingDocument_;
    std::wstring pendingArticleHtml_;
    std::wstring pendingTitle_;
    std::wstring pendingDocumentRoot_;
    std::wstring mappedDocumentRoot_;
    int pendingSourceLine_ = 1;
    double pendingScrollRatio_ = 0.0;
    double pendingAnchorRatio_ = 0.0;
    bool pendingMermaidEnabled_ = true;
    bool hasPendingScrollRatio_ = false;
    bool documentLoaded_ = false;
    unsigned long long pendingDocumentRevision_ = 0;
    ScrollCallback scrollCallback_;
    LinkCallback linkCallback_;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;
};

}  // namespace markdownplusplus
