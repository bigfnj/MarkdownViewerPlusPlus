#pragma once

#include "NppApi.h"
#include "MarkdownRenderer.h"
#include "PluginOptions.h"
#include "PreviewWindow.h"

#include <array>
#include <string>

namespace markdownplusplus {

class PluginController {
public:
    static PluginController& Instance();

    void SetModule(HINSTANCE module);
    void Initialize(npp::NppData data);
    npp::FuncItem* GetFunctions(int* count);
    const wchar_t* Name() const;
    LRESULT MessageProc(UINT message, WPARAM wParam, LPARAM lParam);
    void OnNotification(npp::SCNotification* notification);
    void Shutdown();

    void TogglePreview();
    void RefreshPreview();
    void CopyRenderedHtml();
    void ExportHtml();
    void ExportPdf();
    void PrintPreview();
    void ShowAbout();
    void ToggleAutoOpenMarkdown();
    void ToggleMermaid();
    void ToggleScrollSync();

private:
    enum CommandIndex {
        ToggleCommand = 0,
        SeparatorOne,
        RefreshCommand,
        SeparatorTwo,
        CopyHtmlCommand,
        ExportHtmlCommand,
        ExportPdfCommand,
        PrintCommand,
        SeparatorThree,
        ToggleAutoOpenMarkdownCommand,
        ToggleScrollSyncCommand,
        ToggleMermaidCommand,
        SeparatorFour,
        AboutCommand,
        CommandCount
    };

    PluginController() = default;

    static LRESULT CALLBACK ScintillaSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
    static void CALLBACK RenderDebounceTimerProc(HWND hwnd, UINT message, UINT_PTR timerId, DWORD time);

    void RegisterCommands();
    void SetCommand(CommandIndex index, const wchar_t* name, npp::PluginCommand command, npp::ShortcutKey* shortcut = nullptr, bool checkOnInit = false);
    void EnsureDebounceSubmenu();
    bool AllocateDebounceCommandIds();
    HMENU FindPluginMenu() const;
    bool MenuContainsCommand(HMENU menu, int commandId) const;
    void SetDebounceDelay(int delayMs);
    void UpdateDebounceMenuChecks();
    bool HandleDebounceCommand(int commandId);
    void InstallScintillaSubclass();
    void RemoveScintillaSubclass();
    void SetPreviewVisible(bool visible);
    HWND CurrentScintilla() const;
    std::string ReadCurrentBufferUtf8() const;
    std::wstring GetNotepadString(UINT message) const;
    PreviewDocumentRequest BuildCurrentDocumentRequest() const;
    std::wstring ResolveConfigPath() const;
    bool CurrentDocumentIsMarkdown() const;
    std::wstring ChooseExportPath(const wchar_t* title, const wchar_t* filter, const wchar_t* defaultExtension) const;
    void AutoOpenOrCloseForCurrentDocument();
    std::wstring ResolvePreviewLinkPath(const std::wstring& href) const;
    void OpenPreviewLink(const std::wstring& href);
    void LoadOptions();
    void SaveOptions();
    void UpdateMenuChecks();
    void UpdateOptionMenuCheck(CommandIndex index, bool checked);
    double PreviewScrollFallbackRatio() const;
    void RenderCurrentBuffer();
    void RenderCurrentBufferNow(bool preferCaretSourceLine);
    void ScheduleRenderCurrentBuffer();
    void CancelScheduledRender();
    void OnRenderDebounceTimer(UINT_PTR timerId);
    int CurrentEditorSourceLine() const;
    int CurrentEditorCaretSourceLine() const;
    double CurrentEditorScrollRatio() const;
    double CurrentEditorCaretViewportRatio() const;
    void ScrollEditorToSourceLine(int sourceLine);
    void ScrollEditorToRatio(double ratio);
    void SyncPreviewScrollFromEditor(bool preferCaretSourceLine);
    void HandlePreviewScroll(int sourceLine, double ratio, bool force);
    void OnEditorViewportMaybeChanged(HWND hwnd, bool preferCaretSourceLine);
    bool EditorToPreviewSuppressed() const;
    bool PreviewToEditorSuppressed() const;
    void MarkPreviewDirtyIfNeeded(const npp::SCNotification& notification);

    HINSTANCE module_ = nullptr;
    npp::NppData nppData_{};
    npp::ShortcutKey toggleShortcut_{true, false, true, 'M'};
    std::array<npp::FuncItem, CommandCount> commands_{};
    PluginOptions options_{};
    PluginOptionsStore optionsStore_;
    PreviewWindow previewWindow_;
    HMENU debounceSubmenu_ = nullptr;
    int debounceCommandBase_ = 0;
    bool commandsRegistered_ = false;
    bool debounceSubmenuInstalled_ = false;
    bool previewVisible_ = false;
    bool previewDirty_ = false;
    bool pendingRenderUseCaretAnchor_ = false;
    bool syncingEditorFromPreview_ = false;
    bool scintillaSubclassed_ = false;
    UINT_PTR renderDebounceTimer_ = 0;
    ULONGLONG suppressEditorToPreviewUntil_ = 0;
    ULONGLONG suppressPreviewToEditorUntil_ = 0;
    ULONGLONG lastEditorScrollSyncAt_ = 0;
    ULONGLONG lastPreviewScrollSyncAt_ = 0;
    int lastEditorScrollSourceLine_ = -1;
    int lastPreviewScrollSourceLine_ = -1;
    int lastEditorViewportSourceLine_ = -1;
    int lastEditorCaretSourceLine_ = -1;
    double lastEditorScrollRatio_ = -1.0;
    double lastPreviewScrollRatio_ = -1.0;
    double lastEditorViewportRatio_ = -1.0;
    double lastEditorScrollAnchorRatio_ = -1.0;
};

}  // namespace markdownplusplus
