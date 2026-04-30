#include "PluginController.h"

namespace {
HINSTANCE g_module = nullptr;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        markdownplusplus::PluginController::Instance().SetModule(module);
        DisableThreadLibraryCalls(module);
    }

    return TRUE;
}

extern "C" {

__declspec(dllexport) BOOL isUnicode() {
    return TRUE;
}

__declspec(dllexport) void setInfo(markdownplusplus::npp::NppData notepadPlusData) {
    markdownplusplus::PluginController::Instance().SetModule(g_module);
    markdownplusplus::PluginController::Instance().Initialize(notepadPlusData);
}

__declspec(dllexport) const wchar_t* getName() {
    return markdownplusplus::PluginController::Instance().Name();
}

__declspec(dllexport) markdownplusplus::npp::FuncItem* getFuncsArray(int* count) {
    return markdownplusplus::PluginController::Instance().GetFunctions(count);
}

__declspec(dllexport) void beNotified(markdownplusplus::npp::SCNotification* notification) {
    markdownplusplus::PluginController::Instance().OnNotification(notification);
}

__declspec(dllexport) LRESULT messageProc(UINT message, WPARAM wParam, LPARAM lParam) {
    return markdownplusplus::PluginController::Instance().MessageProc(message, wParam, lParam);
}

}
