#include "PluginController.h"

#include "ClipboardUtil.h"
#include "MarkdownRenderer.h"
#include "WinUtil.h"

#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <cstdlib>
#include <iterator>
#include <vector>

namespace markdownplusplus {
namespace {

constexpr wchar_t kPluginName[] = L"Markdown++";
constexpr wchar_t kConfigFileName[] = L"MarkdownPlusPlus.ini";
constexpr int kMaxPathBuffer = 32768;
constexpr UINT_PTR kMainScintillaSubclassId = 0x4D505031;
constexpr UINT_PTR kSecondScintillaSubclassId = 0x4D505032;
constexpr ULONGLONG kScrollSyncThrottleMs = 50;
constexpr ULONGLONG kCrossScrollSuppressMs = 260;
constexpr double kScrollRatioEpsilon = 0.01;
constexpr double kScrollAnchorEpsilon = 0.02;
constexpr double kPreviewTopAnchorRatio = 0.03;
constexpr int kDebounceDelaysMs[] = {0, 80, 140, 250, 500};
constexpr const wchar_t* kMarkdownExtensions[] = {L".md", L".markdown", L".mdown", L".mkd", L".mkdn"};

void TogglePreviewCommand() {
    PluginController::Instance().TogglePreview();
}

void RefreshPreviewCommand() {
    PluginController::Instance().RefreshPreview();
}

void RunCopyHtmlCommand() {
    PluginController::Instance().CopyRenderedHtml();
}

void RunExportHtmlCommand() {
    PluginController::Instance().ExportHtml();
}

void RunExportPdfCommand() {
    PluginController::Instance().ExportPdf();
}

void RunPrintPreviewCommand() {
    PluginController::Instance().PrintPreview();
}

void RunAboutCommand() {
    PluginController::Instance().ShowAbout();
}

void RunToggleAutoOpenCommand() {
    PluginController::Instance().ToggleAutoOpenMarkdown();
}

void RunToggleMermaidCommand() {
    PluginController::Instance().ToggleMermaid();
}

void RunToggleScrollSyncCommand() {
    PluginController::Instance().ToggleScrollSync();
}

bool IsEditorViewportMessage(UINT message) {
    return message == WM_VSCROLL ||
           message == WM_MOUSEWHEEL ||
           message == WM_LBUTTONUP ||
           message == WM_KEYUP ||
           message == WM_SIZE;
}

bool IsEditorCaretNavigationMessage(UINT message) {
    return message == WM_LBUTTONUP ||
           message == WM_KEYUP;
}

bool SameEditorPreviewTarget(int leftLine, double leftRatio, double leftAnchor, int rightLine, double rightRatio, double rightAnchor) {
    return leftLine == rightLine &&
           std::abs(leftRatio - rightRatio) < kScrollRatioEpsilon &&
           std::abs(leftAnchor - rightAnchor) < kScrollAnchorEpsilon;
}

bool SamePreviewScrollPosition(int leftLine, double leftRatio, int rightLine, double rightRatio) {
    return leftLine == rightLine && std::abs(leftRatio - rightRatio) < kScrollRatioEpsilon;
}

bool IsTextChangeNotification(const npp::SCNotification& notification) {
    return (notification.modificationType & npp::SC_MOD_INSERTTEXT) != 0 ||
           (notification.modificationType & npp::SC_MOD_DELETETEXT) != 0;
}

bool IsSelectionUpdate(const npp::SCNotification& notification) {
    return (notification.updated & npp::SC_UPDATE_SELECTION) != 0;
}

bool IsScrollUpdate(const npp::SCNotification& notification) {
    return (notification.updated & (npp::SC_UPDATE_V_SCROLL | npp::SC_UPDATE_H_SCROLL)) != 0;
}

std::wstring ToLowerAscii(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
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

bool LooksLikeUrl(const std::wstring& value) {
    const size_t colon = value.find(L':');
    if (colon == std::wstring::npos) {
        return false;
    }

    const size_t slash = value.find_first_of(L"\\/");
    return slash == std::wstring::npos || colon < slash;
}

std::wstring StripLinkFragmentAndQuery(std::wstring value) {
    const size_t marker = value.find_first_of(L"?#");
    if (marker != std::wstring::npos) {
        value.erase(marker);
    }
    return value;
}

std::wstring UrlDecode(std::wstring value) {
    DWORD length = static_cast<DWORD>(value.size() + 1);
    if (UrlUnescapeW(value.data(), nullptr, &length, URL_UNESCAPE_INPLACE) == S_OK) {
        value.resize(wcslen(value.c_str()));
    }
    return value;
}

std::wstring CanonicalizeWindowsPath(const std::wstring& path) {
    if (path.empty()) {
        return {};
    }

    wchar_t buffer[MAX_PATH] = {};
    if (!PathCanonicalizeW(buffer, path.c_str())) {
        return path;
    }

    return buffer;
}

bool IsMarkdownFileName(const std::wstring& fileName) {
    if (fileName.empty()) {
        return false;
    }

    const size_t slash = fileName.find_last_of(L"\\/");
    const size_t dot = fileName.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        return false;
    }

    const std::wstring extension = ToLowerAscii(fileName.substr(dot));
    return std::any_of(std::begin(kMarkdownExtensions), std::end(kMarkdownExtensions), [&extension](const wchar_t* markdownExtension) {
        return extension == markdownExtension;
    });
}

bool IsExistingFile(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IsExternalPreviewUrl(const std::wstring& href) {
    if (HasUrlHost(href, L"https://markdownplusplus.document") ||
        HasUrlHost(href, L"https://markdownplusplus.local")) {
        return false;
    }

    return StartsWithIgnoreCase(href, L"https://") || StartsWithIgnoreCase(href, L"http://");
}

}  // namespace

PluginController& PluginController::Instance() {
    static PluginController controller;
    return controller;
}

void PluginController::SetModule(HINSTANCE module) {
    module_ = module;
}

void PluginController::Initialize(npp::NppData data) {
    nppData_ = data;
    LoadOptions();
    RegisterCommands();
    InstallScintillaSubclass();
}

npp::FuncItem* PluginController::GetFunctions(int* count) {
    RegisterCommands();
    if (count) {
        *count = static_cast<int>(commands_.size());
    }

    return commands_.data();
}

const wchar_t* PluginController::Name() const {
    return kPluginName;
}

LRESULT PluginController::MessageProc(UINT message, WPARAM wParam, LPARAM) {
    if (message == WM_COMMAND && HandleDebounceCommand(LOWORD(wParam))) {
        return TRUE;
    }

    return TRUE;
}

void PluginController::OnNotification(npp::SCNotification* notification) {
    if (!notification) {
        return;
    }

    switch (notification->nmhdr.code) {
        case npp::NPPN_SHUTDOWN:
            Shutdown();
            return;

        case npp::NPPN_READY:
            EnsureDebounceSubmenu();
            AutoOpenOrCloseForCurrentDocument();
            return;

        case npp::NPPN_TBMODIFICATION:
            EnsureDebounceSubmenu();
            UpdateMenuChecks();
            return;

        case npp::NPPN_BUFFERACTIVATED:
            AutoOpenOrCloseForCurrentDocument();
            if (previewVisible_) {
                RenderCurrentBuffer();
            }
            return;

        case npp::SCN_MODIFIED:
            if (previewVisible_ && IsTextChangeNotification(*notification)) {
                ScheduleRenderCurrentBuffer();
            } else {
                MarkPreviewDirtyIfNeeded(*notification);
            }
            return;

        case npp::SCN_UPDATEUI:
            if (options_.scrollSyncEnabled && previewVisible_ && !previewDirty_ && !syncingEditorFromPreview_ && !EditorToPreviewSuppressed()) {
                const bool selectionUpdate = IsSelectionUpdate(*notification);
                if (selectionUpdate || IsScrollUpdate(*notification)) {
                    SyncPreviewScrollFromEditor(selectionUpdate);
                }
            }
            return;

        default:
            return;
    }
}

void PluginController::Shutdown() {
    CancelScheduledRender();
    RemoveScintillaSubclass();
    previewWindow_.Destroy();
    previewVisible_ = false;
    previewDirty_ = false;
}

void PluginController::TogglePreview() {
    SetPreviewVisible(!previewVisible_);
}

void PluginController::RefreshPreview() {
    if (!previewVisible_) {
        SetPreviewVisible(true);
        return;
    }

    RenderCurrentBuffer();
}

void PluginController::CopyRenderedHtml() {
    PreviewDocumentRequest request = BuildCurrentDocumentRequest();
    PreviewRenderResult rendered = MarkdownRenderer::BuildPreview(request);

    if (!CopyHtmlToClipboard(nppData_._nppHandle, rendered.articleHtml, rendered.articleHtml)) {
        MessageBoxW(nppData_._nppHandle, L"Markdown++ could not copy the rendered HTML to the clipboard.", kPluginName, MB_OK | MB_ICONERROR);
    }
}

void PluginController::ExportHtml() {
    const std::wstring path = ChooseExportPath(
        L"Export Markdown++ HTML",
        L"HTML Files (*.html)\0*.html\0All Files (*.*)\0*.*\0",
        L"html");
    if (path.empty()) {
        return;
    }

    PreviewDocumentRequest request = BuildCurrentDocumentRequest();
    const std::wstring document = MarkdownRenderer::BuildStandaloneDocument(request);
    if (!WriteUtf8File(path, WideToUtf8(document))) {
        MessageBoxW(nppData_._nppHandle, L"Markdown++ could not write the HTML export file.", kPluginName, MB_OK | MB_ICONERROR);
    }
}

void PluginController::ExportPdf() {
    if (previewVisible_ && previewDirty_) {
        RenderCurrentBuffer();
        MessageBoxW(
            nppData_._nppHandle,
            L"Markdown++ refreshed the preview. Run Export PDF again after the preview finishes loading.",
            kPluginName,
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (!previewVisible_ || !previewWindow_.Created() || !previewWindow_.Ready()) {
        SetPreviewVisible(true);
        MessageBoxW(
            nppData_._nppHandle,
            L"Markdown++ opened the preview. Run Export PDF again after the preview finishes loading.",
            kPluginName,
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    const std::wstring path = ChooseExportPath(
        L"Export Markdown++ PDF",
        L"PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0",
        L"pdf");
    if (!path.empty()) {
        previewWindow_.PrintToPdf(path);
    }
}

void PluginController::PrintPreview() {
    if (previewVisible_ && previewDirty_) {
        RenderCurrentBuffer();
        MessageBoxW(
            nppData_._nppHandle,
            L"Markdown++ refreshed the preview. Run Print again after the preview finishes loading.",
            kPluginName,
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (!previewVisible_ || !previewWindow_.Created() || !previewWindow_.Ready()) {
        SetPreviewVisible(true);
        MessageBoxW(
            nppData_._nppHandle,
            L"Markdown++ opened the preview. Run Print again after the preview finishes loading.",
            kPluginName,
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    previewWindow_.ShowPrintUi();
}

void PluginController::ShowAbout() {
    MessageBoxW(
        nppData_._nppHandle,
        L"Markdown++ native preview\n\nRenderer: WebView2\nMarkdown engine: cmark-gfm\nMermaid: bundled/offline\nExports: HTML, PDF, clipboard, print",
        kPluginName,
        MB_OK | MB_ICONINFORMATION);
}

void PluginController::ToggleAutoOpenMarkdown() {
    options_.autoOpenMarkdown = !options_.autoOpenMarkdown;
    SaveOptions();
    UpdateMenuChecks();

    if (options_.autoOpenMarkdown) {
        AutoOpenOrCloseForCurrentDocument();
    }
}

void PluginController::ToggleMermaid() {
    options_.mermaidEnabled = !options_.mermaidEnabled;
    SaveOptions();
    UpdateMenuChecks();

    if (previewVisible_) {
        RenderCurrentBuffer();
    }
}

void PluginController::ToggleScrollSync() {
    options_.scrollSyncEnabled = !options_.scrollSyncEnabled;
    SaveOptions();
    UpdateMenuChecks();

    if (options_.scrollSyncEnabled && previewVisible_) {
        SyncPreviewScrollFromEditor(true);
    }
}

void PluginController::RegisterCommands() {
    if (commandsRegistered_) {
        return;
    }

    SetCommand(ToggleCommand, L"Markdown++", TogglePreviewCommand, &toggleShortcut_);
    SetCommand(SeparatorOne, L"---", nullptr);
    SetCommand(RefreshCommand, L"Refresh preview", RefreshPreviewCommand);
    SetCommand(SeparatorTwo, L"---", nullptr);
    SetCommand(CopyHtmlCommand, L"Copy HTML to clipboard", RunCopyHtmlCommand);
    SetCommand(ExportHtmlCommand, L"Export HTML...", RunExportHtmlCommand);
    SetCommand(ExportPdfCommand, L"Export PDF...", RunExportPdfCommand);
    SetCommand(PrintCommand, L"Print...", RunPrintPreviewCommand);
    SetCommand(SeparatorThree, L"---", nullptr);
    SetCommand(ToggleAutoOpenMarkdownCommand, L"Open automatically for Markdown files", RunToggleAutoOpenCommand, nullptr, options_.autoOpenMarkdown);
    SetCommand(ToggleScrollSyncCommand, L"Synchronize scrolling", RunToggleScrollSyncCommand, nullptr, options_.scrollSyncEnabled);
    SetCommand(ToggleMermaidCommand, L"Render Mermaid diagrams", RunToggleMermaidCommand, nullptr, options_.mermaidEnabled);
    SetCommand(SeparatorFour, L"---", nullptr);
    SetCommand(AboutCommand, L"About", RunAboutCommand);
    commandsRegistered_ = true;
}

void PluginController::SetCommand(CommandIndex index, const wchar_t* name, npp::PluginCommand command, npp::ShortcutKey* shortcut, bool checkOnInit) {
    auto& item = commands_[index];
    wcsncpy_s(item._itemName, name, _TRUNCATE);
    item._pFunc = command;
    item._cmdID = 0;
    item._init2Check = checkOnInit;
    item._pShKey = shortcut;
}

void PluginController::EnsureDebounceSubmenu() {
    if (debounceSubmenuInstalled_ || !nppData_._nppHandle) {
        return;
    }

    if (!AllocateDebounceCommandIds()) {
        return;
    }

    HMENU pluginMenu = FindPluginMenu();
    if (!pluginMenu) {
        return;
    }

    debounceSubmenu_ = CreatePopupMenu();
    if (!debounceSubmenu_) {
        return;
    }

    for (int index = 0; index < static_cast<int>(std::size(kDebounceDelaysMs)); ++index) {
        std::wstring label = std::to_wstring(kDebounceDelaysMs[index]);
        label += L" ms";
        if (kDebounceDelaysMs[index] == 0) {
            label += L" (immediate)";
        }

        AppendMenuW(
            debounceSubmenu_,
            MF_STRING,
            static_cast<UINT_PTR>(debounceCommandBase_ + index),
            label.c_str());
    }

    int insertPosition = GetMenuItemCount(pluginMenu);
    for (int position = 0; position < GetMenuItemCount(pluginMenu); ++position) {
        if (GetMenuItemID(pluginMenu, position) == static_cast<UINT>(commands_[AboutCommand]._cmdID)) {
            insertPosition = position;
            if (position > 0) {
                MENUITEMINFOW previous{};
                previous.cbSize = sizeof(previous);
                previous.fMask = MIIM_FTYPE;
                if (GetMenuItemInfoW(pluginMenu, position - 1, TRUE, &previous) && (previous.fType & MFT_SEPARATOR) != 0) {
                    insertPosition = position - 1;
                }
            }
            break;
        }
    }

    if (!InsertMenuW(pluginMenu, insertPosition, MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(debounceSubmenu_), L"Preview update delay")) {
        DestroyMenu(debounceSubmenu_);
        debounceSubmenu_ = nullptr;
        return;
    }

    debounceSubmenuInstalled_ = true;
    UpdateDebounceMenuChecks();
    DrawMenuBar(nppData_._nppHandle);
}

bool PluginController::AllocateDebounceCommandIds() {
    if (debounceCommandBase_ != 0) {
        return true;
    }

    int commandBase = 0;
    const BOOL allocated = static_cast<BOOL>(SendMessageW(
        nppData_._nppHandle,
        npp::NPPM_ALLOCATECMDID,
        static_cast<WPARAM>(std::size(kDebounceDelaysMs)),
        reinterpret_cast<LPARAM>(&commandBase)));
    if (!allocated || commandBase == 0) {
        return false;
    }

    debounceCommandBase_ = commandBase;
    return true;
}

HMENU PluginController::FindPluginMenu() const {
    HMENU mainMenu = GetMenu(nppData_._nppHandle);
    if (!mainMenu) {
        return nullptr;
    }

    const int mainCount = GetMenuItemCount(mainMenu);
    for (int mainIndex = 0; mainIndex < mainCount; ++mainIndex) {
        HMENU childMenu = GetSubMenu(mainMenu, mainIndex);
        if (!childMenu) {
            continue;
        }

        const int childCount = GetMenuItemCount(childMenu);
        for (int childIndex = 0; childIndex < childCount; ++childIndex) {
            HMENU pluginMenu = GetSubMenu(childMenu, childIndex);
            if (pluginMenu && MenuContainsCommand(pluginMenu, commands_[ToggleCommand]._cmdID)) {
                return pluginMenu;
            }
        }
    }

    return nullptr;
}

bool PluginController::MenuContainsCommand(HMENU menu, int commandId) const {
    const int count = GetMenuItemCount(menu);
    for (int index = 0; index < count; ++index) {
        if (GetMenuItemID(menu, index) == static_cast<UINT>(commandId)) {
            return true;
        }
    }

    return false;
}

void PluginController::SetDebounceDelay(int delayMs) {
    options_.renderDebounceMs = ClampRenderDebounceMs(delayMs);
    SaveOptions();
    UpdateDebounceMenuChecks();
}

void PluginController::UpdateDebounceMenuChecks() {
    if (!debounceSubmenu_ || debounceCommandBase_ == 0) {
        return;
    }

    int checkedCommand = -1;
    for (int index = 0; index < static_cast<int>(std::size(kDebounceDelaysMs)); ++index) {
        if (options_.renderDebounceMs == kDebounceDelaysMs[index]) {
            checkedCommand = debounceCommandBase_ + index;
            break;
        }
    }

    for (int index = 0; index < static_cast<int>(std::size(kDebounceDelaysMs)); ++index) {
        CheckMenuItem(
            debounceSubmenu_,
            static_cast<UINT>(debounceCommandBase_ + index),
            MF_BYCOMMAND | ((debounceCommandBase_ + index) == checkedCommand ? MF_CHECKED : MF_UNCHECKED));
    }
}

bool PluginController::HandleDebounceCommand(int commandId) {
    if (debounceCommandBase_ == 0) {
        return false;
    }

    const int index = commandId - debounceCommandBase_;
    if (index < 0 || index >= static_cast<int>(std::size(kDebounceDelaysMs))) {
        return false;
    }

    SetDebounceDelay(kDebounceDelaysMs[index]);
    return true;
}

LRESULT CALLBACK PluginController::ScintillaSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);

    if (IsEditorViewportMessage(message)) {
        auto* self = reinterpret_cast<PluginController*>(refData);
        if (self) {
            self->OnEditorViewportMaybeChanged(hwnd, IsEditorCaretNavigationMessage(message));
        }
    }

    return result;
}

void CALLBACK PluginController::RenderDebounceTimerProc(HWND, UINT, UINT_PTR timerId, DWORD) {
    PluginController::Instance().OnRenderDebounceTimer(timerId);
}

void PluginController::InstallScintillaSubclass() {
    if (scintillaSubclassed_) {
        return;
    }

    if (nppData_._scintillaMainHandle) {
        SetWindowSubclass(
            nppData_._scintillaMainHandle,
            PluginController::ScintillaSubclassProc,
            kMainScintillaSubclassId,
            reinterpret_cast<DWORD_PTR>(this));
    }

    if (nppData_._scintillaSecondHandle) {
        SetWindowSubclass(
            nppData_._scintillaSecondHandle,
            PluginController::ScintillaSubclassProc,
            kSecondScintillaSubclassId,
            reinterpret_cast<DWORD_PTR>(this));
    }

    scintillaSubclassed_ = true;
}

void PluginController::RemoveScintillaSubclass() {
    if (!scintillaSubclassed_) {
        return;
    }

    if (nppData_._scintillaMainHandle) {
        RemoveWindowSubclass(nppData_._scintillaMainHandle, PluginController::ScintillaSubclassProc, kMainScintillaSubclassId);
    }

    if (nppData_._scintillaSecondHandle) {
        RemoveWindowSubclass(nppData_._scintillaSecondHandle, PluginController::ScintillaSubclassProc, kSecondScintillaSubclassId);
    }

    scintillaSubclassed_ = false;
}

void PluginController::SetPreviewVisible(bool visible) {
    previewVisible_ = visible;

    if (previewVisible_) {
        if (!previewWindow_.Created()) {
            if (!previewWindow_.Create(module_, nppData_._nppHandle, ToggleCommand, [this]() {
                SetPreviewVisible(false);
            })) {
                previewVisible_ = false;
                MessageBoxW(nppData_._nppHandle, L"Markdown++ could not create the native preview window.", kPluginName, MB_OK | MB_ICONERROR);
                return;
            }

            previewWindow_.SetScrollCallback([this](int sourceLine, double ratio, bool force) {
                HandlePreviewScroll(sourceLine, ratio, force);
            });

            previewWindow_.SetLinkCallback([this](const std::wstring& href) {
                OpenPreviewLink(href);
            });
        }

        previewWindow_.Show();
        UpdateMenuChecks();
        RenderCurrentBuffer();
    } else {
        CancelScheduledRender();
        previewWindow_.Hide();
        UpdateMenuChecks();
    }
}

HWND PluginController::CurrentScintilla() const {
    int activeView = 0;
    SendMessageW(nppData_._nppHandle, npp::NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&activeView));
    return activeView == 0 ? nppData_._scintillaMainHandle : nppData_._scintillaSecondHandle;
}

std::string PluginController::ReadCurrentBufferUtf8() const {
    HWND scintilla = CurrentScintilla();
    if (!scintilla) {
        return {};
    }

    LRESULT length = SendMessageW(scintilla, npp::SCI_GETLENGTH, 0, 0);
    if (length <= 0) {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(length) + 1, '\0');
    SendMessageW(scintilla, npp::SCI_GETTEXT, static_cast<WPARAM>(buffer.size()), reinterpret_cast<LPARAM>(buffer.data()));
    return std::string(buffer.data(), static_cast<size_t>(length));
}

std::wstring PluginController::GetNotepadString(UINT message) const {
    std::vector<wchar_t> buffer(kMaxPathBuffer, L'\0');
    SendMessageW(nppData_._nppHandle, message, static_cast<WPARAM>(buffer.size()), reinterpret_cast<LPARAM>(buffer.data()));
    return buffer.data();
}

PreviewDocumentRequest PluginController::BuildCurrentDocumentRequest() const {
    PreviewDocumentRequest request;
    request.markdownUtf8 = ReadCurrentBufferUtf8();
    request.title = GetNotepadString(npp::NPPM_GETFILENAME);
    request.assetRoot = GetModuleDirectory(module_);
    request.baseDirectory = GetNotepadString(npp::NPPM_GETCURRENTDIRECTORY);
    request.mermaidEnabled = options_.mermaidEnabled;
    return request;
}

std::wstring PluginController::ResolveConfigPath() const {
    std::wstring configDirectory = GetNotepadString(npp::NPPM_GETPLUGINSCONFIGDIR);
    if (configDirectory.empty()) {
        configDirectory = GetModuleDirectory(module_);
    }

    if (!configDirectory.empty()) {
        CreateDirectoryW(configDirectory.c_str(), nullptr);
    }

    return CombinePath(configDirectory, kConfigFileName);
}

std::wstring PluginController::ChooseExportPath(const wchar_t* title, const wchar_t* filter, const wchar_t* defaultExtension) const {
    std::wstring fileName = GetNotepadString(npp::NPPM_GETFILENAME);
    if (fileName.empty()) {
        fileName = L"MarkdownPlusPlus";
    }

    const size_t slash = fileName.find_last_of(L"\\/");
    const size_t dot = fileName.find_last_of(L'.');
    if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)) {
        fileName.erase(dot);
    }

    fileName += L".";
    fileName += defaultExtension;

    std::vector<wchar_t> path(kMaxPathBuffer, L'\0');
    wcsncpy_s(path.data(), path.size(), fileName.c_str(), _TRUNCATE);

    const std::wstring initialDirectory = GetNotepadString(npp::NPPM_GETCURRENTDIRECTORY);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nppData_._nppHandle;
    dialog.lpstrTitle = title;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = defaultExtension;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (!initialDirectory.empty()) {
        dialog.lpstrInitialDir = initialDirectory.c_str();
    }

    if (!GetSaveFileNameW(&dialog)) {
        return {};
    }

    return path.data();
}

bool PluginController::CurrentDocumentIsMarkdown() const {
    const std::wstring fullPath = GetNotepadString(npp::NPPM_GETFULLCURRENTPATH);
    const std::wstring fileName = fullPath.empty() ? GetNotepadString(npp::NPPM_GETFILENAME) : fullPath;
    return IsMarkdownFileName(fileName);
}

void PluginController::AutoOpenOrCloseForCurrentDocument() {
    if (!options_.autoOpenMarkdown) {
        return;
    }

    SetPreviewVisible(CurrentDocumentIsMarkdown());
}

std::wstring PluginController::ResolvePreviewLinkPath(const std::wstring& href) const {
    std::wstring link = UrlDecode(StripLinkFragmentAndQuery(href));
    if (link.empty()) {
        return {};
    }

    constexpr wchar_t kDocumentUrlPrefix[] = L"https://markdownplusplus.document/";
    constexpr wchar_t kFileUrlPrefix[] = L"file:";

    if (StartsWithIgnoreCase(link, kDocumentUrlPrefix)) {
        link = link.substr(wcslen(kDocumentUrlPrefix));
    } else if (StartsWithIgnoreCase(link, kFileUrlPrefix)) {
        DWORD pathLength = 0;
        if (PathCreateFromUrlW(link.c_str(), nullptr, &pathLength, 0) != E_POINTER || pathLength == 0) {
            return {};
        }

        std::wstring filePath(pathLength, L'\0');
        if (FAILED(PathCreateFromUrlW(link.c_str(), filePath.data(), &pathLength, 0))) {
            return {};
        }

        filePath.assign(filePath.c_str());
        return CanonicalizeWindowsPath(filePath);
    } else if (LooksLikeUrl(link)) {
        return {};
    }

    std::replace(link.begin(), link.end(), L'/', L'\\');
    while (!link.empty() && (link.front() == L'\\' || link.front() == L'/')) {
        link.erase(link.begin());
    }

    if (PathIsRelativeW(link.c_str())) {
        link = CombinePath(GetNotepadString(npp::NPPM_GETCURRENTDIRECTORY), link);
    }

    return CanonicalizeWindowsPath(link);
}

void PluginController::OpenPreviewLink(const std::wstring& href) {
    if (IsExternalPreviewUrl(href)) {
        ShellExecuteW(nppData_._nppHandle, L"open", href.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }

    const std::wstring path = ResolvePreviewLinkPath(href);
    if (path.empty() || !IsMarkdownFileName(path) || !IsExistingFile(path)) {
        return;
    }

    SendMessageW(nppData_._nppHandle, npp::NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(path.c_str()));
}

void PluginController::LoadOptions() {
    optionsStore_ = PluginOptionsStore(ResolveConfigPath());
    options_ = optionsStore_.Load();
}

void PluginController::SaveOptions() {
    options_.renderDebounceMs = ClampRenderDebounceMs(options_.renderDebounceMs);
    optionsStore_.Save(options_);
}

void PluginController::UpdateMenuChecks() {
    if (!nppData_._nppHandle) {
        return;
    }

    UpdateOptionMenuCheck(ToggleCommand, previewVisible_);
    UpdateOptionMenuCheck(ToggleAutoOpenMarkdownCommand, options_.autoOpenMarkdown);
    UpdateOptionMenuCheck(ToggleScrollSyncCommand, options_.scrollSyncEnabled);
    UpdateOptionMenuCheck(ToggleMermaidCommand, options_.mermaidEnabled);
    UpdateDebounceMenuChecks();
}

void PluginController::UpdateOptionMenuCheck(CommandIndex index, bool checked) {
    SendMessageW(nppData_._nppHandle, npp::NPPM_SETMENUITEMCHECK, commands_[index]._cmdID, checked ? TRUE : FALSE);
}

double PluginController::PreviewScrollFallbackRatio() const {
    if (std::isfinite(lastPreviewScrollRatio_) && lastPreviewScrollRatio_ >= 0.0) {
        return std::clamp(lastPreviewScrollRatio_, 0.0, 1.0);
    }

    return options_.scrollSyncEnabled ? CurrentEditorScrollRatio() : 0.0;
}

void PluginController::RenderCurrentBuffer() {
    CancelScheduledRender();
    RenderCurrentBufferNow(false);
}

void PluginController::RenderCurrentBufferNow(bool preferCaretSourceLine) {
    if (!previewWindow_.Created()) {
        return;
    }

    PreviewDocumentRequest request = BuildCurrentDocumentRequest();
    PreviewRenderResult rendered = MarkdownRenderer::BuildPreview(request);
    const int sourceLine = options_.scrollSyncEnabled
                               ? (preferCaretSourceLine ? CurrentEditorCaretSourceLine() : CurrentEditorSourceLine())
                               : 0;
    const double scrollRatio = options_.scrollSyncEnabled ? CurrentEditorScrollRatio() : PreviewScrollFallbackRatio();
    const double anchorRatio = options_.scrollSyncEnabled
                                   ? (preferCaretSourceLine ? CurrentEditorCaretViewportRatio() : kPreviewTopAnchorRatio)
                                   : 0.0;
    previewWindow_.SetDocument(
        rendered.document,
        rendered.articleHtml,
        rendered.title,
        request.baseDirectory,
        sourceLine,
        scrollRatio,
        anchorRatio,
        options_.mermaidEnabled);
    previewDirty_ = false;
    pendingRenderUseCaretAnchor_ = false;
}

void PluginController::ScheduleRenderCurrentBuffer() {
    previewDirty_ = true;
    pendingRenderUseCaretAnchor_ = options_.scrollSyncEnabled;

    if (renderDebounceTimer_ != 0) {
        KillTimer(nullptr, renderDebounceTimer_);
        renderDebounceTimer_ = 0;
    }

    if (options_.renderDebounceMs <= 0) {
        RenderCurrentBufferNow(pendingRenderUseCaretAnchor_);
        return;
    }

    renderDebounceTimer_ = SetTimer(nullptr, 0, static_cast<UINT>(options_.renderDebounceMs), PluginController::RenderDebounceTimerProc);
    if (renderDebounceTimer_ == 0) {
        RenderCurrentBufferNow(pendingRenderUseCaretAnchor_);
    }
}

void PluginController::CancelScheduledRender() {
    if (renderDebounceTimer_ == 0) {
        return;
    }

    KillTimer(nullptr, renderDebounceTimer_);
    renderDebounceTimer_ = 0;
    pendingRenderUseCaretAnchor_ = false;
}

void PluginController::OnRenderDebounceTimer(UINT_PTR timerId) {
    if (timerId != renderDebounceTimer_) {
        return;
    }

    const bool preferCaretSourceLine = pendingRenderUseCaretAnchor_;
    CancelScheduledRender();

    if (previewVisible_ && previewDirty_) {
        RenderCurrentBufferNow(preferCaretSourceLine);
    }
}

int PluginController::CurrentEditorSourceLine() const {
    HWND scintilla = CurrentScintilla();
    if (!scintilla) {
        return 1;
    }

    const LRESULT firstVisibleLine = SendMessageW(scintilla, npp::SCI_GETFIRSTVISIBLELINE, 0, 0);
    const LRESULT documentLine = SendMessageW(scintilla, npp::SCI_DOCLINEFROMVISIBLE, firstVisibleLine, 0);
    return static_cast<int>(std::max<LRESULT>(0, documentLine)) + 1;
}

int PluginController::CurrentEditorCaretSourceLine() const {
    HWND scintilla = CurrentScintilla();
    if (!scintilla) {
        return 1;
    }

    const LRESULT currentPosition = SendMessageW(scintilla, npp::SCI_GETCURRENTPOS, 0, 0);
    const LRESULT documentLine = SendMessageW(scintilla, npp::SCI_LINEFROMPOSITION, currentPosition, 0);
    return static_cast<int>(std::max<LRESULT>(0, documentLine)) + 1;
}

double PluginController::CurrentEditorScrollRatio() const {
    HWND scintilla = CurrentScintilla();
    if (!scintilla) {
        return 0.0;
    }

    const LRESULT firstVisibleLine = SendMessageW(scintilla, npp::SCI_GETFIRSTVISIBLELINE, 0, 0);
    const LRESULT lineCount = SendMessageW(scintilla, npp::SCI_GETLINECOUNT, 0, 0);
    const LRESULT linesOnScreen = SendMessageW(scintilla, npp::SCI_LINESONSCREEN, 0, 0);
    const LRESULT maxFirstVisibleLine = std::max<LRESULT>(1, lineCount - std::max<LRESULT>(1, linesOnScreen));

    return std::clamp(static_cast<double>(firstVisibleLine) / static_cast<double>(maxFirstVisibleLine), 0.0, 1.0);
}

double PluginController::CurrentEditorCaretViewportRatio() const {
    HWND scintilla = CurrentScintilla();
    if (!scintilla) {
        return kPreviewTopAnchorRatio;
    }

    const LRESULT currentPosition = SendMessageW(scintilla, npp::SCI_GETCURRENTPOS, 0, 0);
    const LRESULT documentLine = SendMessageW(scintilla, npp::SCI_LINEFROMPOSITION, currentPosition, 0);
    const LRESULT caretVisibleLine = SendMessageW(scintilla, npp::SCI_VISIBLEFROMDOCLINE, documentLine, 0);
    const LRESULT firstVisibleLine = SendMessageW(scintilla, npp::SCI_GETFIRSTVISIBLELINE, 0, 0);
    const LRESULT linesOnScreen = std::max<LRESULT>(1, SendMessageW(scintilla, npp::SCI_LINESONSCREEN, 0, 0));

    return std::clamp(
        static_cast<double>(caretVisibleLine - firstVisibleLine) / static_cast<double>(linesOnScreen),
        0.0,
        1.0);
}

void PluginController::ScrollEditorToSourceLine(int sourceLine) {
    HWND scintilla = CurrentScintilla();
    if (!scintilla || sourceLine <= 0) {
        return;
    }

    const LRESULT documentLine = std::max<LRESULT>(0, sourceLine - 1);
    const LRESULT targetVisibleLine = SendMessageW(scintilla, npp::SCI_VISIBLEFROMDOCLINE, documentLine, 0);
    const LRESULT currentFirstVisibleLine = SendMessageW(scintilla, npp::SCI_GETFIRSTVISIBLELINE, 0, 0);
    const LRESULT delta = targetVisibleLine - currentFirstVisibleLine;

    if (delta == 0) {
        return;
    }

    syncingEditorFromPreview_ = true;
    SendMessageW(scintilla, npp::SCI_LINESCROLL, 0, delta);
    syncingEditorFromPreview_ = false;
}

void PluginController::ScrollEditorToRatio(double ratio) {
    HWND scintilla = CurrentScintilla();
    if (!scintilla || !std::isfinite(ratio)) {
        return;
    }

    ratio = std::clamp(ratio, 0.0, 1.0);

    const LRESULT currentFirstVisibleLine = SendMessageW(scintilla, npp::SCI_GETFIRSTVISIBLELINE, 0, 0);
    const LRESULT lineCount = SendMessageW(scintilla, npp::SCI_GETLINECOUNT, 0, 0);
    const LRESULT linesOnScreen = SendMessageW(scintilla, npp::SCI_LINESONSCREEN, 0, 0);
    const LRESULT maxFirstVisibleLine = std::max<LRESULT>(0, lineCount - std::max<LRESULT>(1, linesOnScreen));
    const LRESULT targetFirstVisibleLine = static_cast<LRESULT>(std::llround(ratio * static_cast<double>(maxFirstVisibleLine)));
    const LRESULT delta = targetFirstVisibleLine - currentFirstVisibleLine;

    if (delta == 0) {
        return;
    }

    syncingEditorFromPreview_ = true;
    SendMessageW(scintilla, npp::SCI_LINESCROLL, 0, delta);
    syncingEditorFromPreview_ = false;
}

void PluginController::SyncPreviewScrollFromEditor(bool preferCaretSourceLine) {
    if (!previewWindow_.Created()) {
        return;
    }

    const ULONGLONG now = GetTickCount64();
    if (!preferCaretSourceLine && now < lastEditorScrollSyncAt_ + kScrollSyncThrottleMs) {
        return;
    }

    const int viewportSourceLine = CurrentEditorSourceLine();
    const int caretSourceLine = CurrentEditorCaretSourceLine();
    const double ratio = CurrentEditorScrollRatio();
    const bool viewportChanged = viewportSourceLine != lastEditorViewportSourceLine_ ||
                                 std::abs(ratio - lastEditorViewportRatio_) >= kScrollRatioEpsilon;
    const bool caretChanged = caretSourceLine != lastEditorCaretSourceLine_;
    const int sourceLine = preferCaretSourceLine || (!viewportChanged && caretChanged) ? caretSourceLine : viewportSourceLine;
    const double anchorRatio = preferCaretSourceLine ? CurrentEditorCaretViewportRatio() : kPreviewTopAnchorRatio;

    lastEditorViewportSourceLine_ = viewportSourceLine;
    lastEditorViewportRatio_ = ratio;
    lastEditorCaretSourceLine_ = caretSourceLine;

    if (SameEditorPreviewTarget(sourceLine, ratio, anchorRatio, lastEditorScrollSourceLine_, lastEditorScrollRatio_, lastEditorScrollAnchorRatio_)) {
        return;
    }

    lastEditorScrollSyncAt_ = now;
    lastEditorScrollSourceLine_ = sourceLine;
    lastEditorScrollRatio_ = ratio;
    lastEditorScrollAnchorRatio_ = anchorRatio;
    suppressPreviewToEditorUntil_ = now + kCrossScrollSuppressMs;

    previewWindow_.SetScrollSourceLine(
        sourceLine,
        ratio,
        anchorRatio);
}

void PluginController::HandlePreviewScroll(int sourceLine, double ratio, bool force) {
    if (!previewVisible_) {
        return;
    }

    const int previousPreviewSourceLine = lastPreviewScrollSourceLine_;
    const double previousPreviewRatio = lastPreviewScrollRatio_;

    if (!options_.scrollSyncEnabled || (!force && PreviewToEditorSuppressed())) {
        lastPreviewScrollSourceLine_ = sourceLine;
        lastPreviewScrollRatio_ = ratio;
        return;
    }

    const ULONGLONG now = GetTickCount64();
    if (!force && now < lastPreviewScrollSyncAt_ + kScrollSyncThrottleMs) {
        return;
    }

    if (!force && SamePreviewScrollPosition(sourceLine, ratio, previousPreviewSourceLine, previousPreviewRatio)) {
        return;
    }

    lastPreviewScrollSyncAt_ = now;
    lastPreviewScrollSourceLine_ = sourceLine;
    lastPreviewScrollRatio_ = ratio;
    suppressEditorToPreviewUntil_ = now + kCrossScrollSuppressMs;

    if (sourceLine > 0) {
        ScrollEditorToSourceLine(sourceLine);
    } else {
        ScrollEditorToRatio(ratio);
    }
}

void PluginController::OnEditorViewportMaybeChanged(HWND hwnd, bool preferCaretSourceLine) {
    if (!options_.scrollSyncEnabled || !previewVisible_ || syncingEditorFromPreview_ || EditorToPreviewSuppressed() || hwnd != CurrentScintilla()) {
        return;
    }

    SyncPreviewScrollFromEditor(preferCaretSourceLine);
}

bool PluginController::EditorToPreviewSuppressed() const {
    return GetTickCount64() < suppressEditorToPreviewUntil_;
}

bool PluginController::PreviewToEditorSuppressed() const {
    return GetTickCount64() < suppressPreviewToEditorUntil_;
}

void PluginController::MarkPreviewDirtyIfNeeded(const npp::SCNotification& notification) {
    if (IsTextChangeNotification(notification)) {
        previewDirty_ = true;
    }
}

}  // namespace markdownplusplus
