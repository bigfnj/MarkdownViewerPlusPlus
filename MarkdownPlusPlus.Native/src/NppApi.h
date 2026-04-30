#pragma once

#include <windows.h>

namespace markdownplusplus::npp {

constexpr int kFuncItemNameLength = 64;
constexpr UINT kNppMsg = WM_USER + 1000;
constexpr UINT kRunCommandUser = WM_USER + 3000;

constexpr UINT NPPM_GETCURRENTSCINTILLA = kNppMsg + 4;
constexpr UINT NPPM_DMMSHOW = kNppMsg + 30;
constexpr UINT NPPM_DMMHIDE = kNppMsg + 31;
constexpr UINT NPPM_DMMREGASDCKDLG = kNppMsg + 33;
constexpr UINT NPPM_SETMENUITEMCHECK = kNppMsg + 40;
constexpr UINT NPPM_GETPLUGINSCONFIGDIR = kNppMsg + 46;
constexpr UINT NPPM_DOOPEN = kNppMsg + 77;
constexpr UINT NPPM_ALLOCATECMDID = kNppMsg + 81;

constexpr UINT NPPM_GETFULLCURRENTPATH = kRunCommandUser + 1;
constexpr UINT NPPM_GETCURRENTDIRECTORY = kRunCommandUser + 2;
constexpr UINT NPPM_GETFILENAME = kRunCommandUser + 3;

constexpr UINT NPPN_FIRST = 1000;
constexpr UINT NPPN_READY = NPPN_FIRST + 1;
constexpr UINT NPPN_TBMODIFICATION = NPPN_FIRST + 2;
constexpr UINT NPPN_SHUTDOWN = NPPN_FIRST + 9;
constexpr UINT NPPN_BUFFERACTIVATED = NPPN_FIRST + 10;

constexpr UINT SCI_GETLENGTH = 2006;
constexpr UINT SCI_GETCURRENTPOS = 2008;
constexpr UINT SCI_GETTEXT = 2182;
constexpr UINT SCI_GETFIRSTVISIBLELINE = 2152;
constexpr UINT SCI_GETLINECOUNT = 2154;
constexpr UINT SCI_LINEFROMPOSITION = 2166;
constexpr UINT SCI_LINESCROLL = 2168;
constexpr UINT SCI_LINESONSCREEN = 2370;
constexpr UINT SCI_VISIBLEFROMDOCLINE = 2220;
constexpr UINT SCI_DOCLINEFROMVISIBLE = 2221;

constexpr UINT SC_MOD_INSERTTEXT = 0x1;
constexpr UINT SC_MOD_DELETETEXT = 0x2;
constexpr UINT SC_UPDATE_SELECTION = 0x1;
constexpr UINT SC_UPDATE_CONTENT = 0x2;
constexpr UINT SC_UPDATE_V_SCROLL = 0x4;
constexpr UINT SC_UPDATE_H_SCROLL = 0x8;
constexpr UINT SCN_UPDATEUI = 2007;
constexpr UINT SCN_MODIFIED = 2008;

constexpr UINT DWS_DF_CONT_RIGHT = 1u << 28;

struct NppData {
    HWND _nppHandle;
    HWND _scintillaMainHandle;
    HWND _scintillaSecondHandle;
};

using PluginCommand = void (*)();

struct ShortcutKey {
    bool _isCtrl;
    bool _isAlt;
    bool _isShift;
    UCHAR _key;
};

struct FuncItem {
    wchar_t _itemName[kFuncItemNameLength];
    PluginCommand _pFunc;
    int _cmdID;
    bool _init2Check;
    ShortcutKey* _pShKey;
};

struct NppTbData {
    HWND hClient;
    const wchar_t* pszName;
    int dlgID;
    UINT uMask;
    HICON hIconTab;
    const wchar_t* pszAddInfo;
    RECT rcFloat;
    int iPrevCont;
    const wchar_t* pszModuleName;
};

struct SCNotification {
    NMHDR nmhdr;
    INT_PTR position;
    int character;
    int modifiers;
    int modificationType;
    const char* text;
    INT_PTR length;
    INT_PTR linesAdded;
    int message;
    WPARAM wParam;
    LPARAM lParam;
    INT_PTR line;
    int foldLevelNow;
    int foldLevelPrev;
    int margin;
    int listType;
    int x;
    int y;
    int token;
    INT_PTR annotationLinesAdded;
    int updated;
    int listCompletionMethod;
    int characterSource;
};

}  // namespace markdownplusplus::npp
