// ==WindhawkMod==
// @id              valinet-chrome-save-to-downloads-directly
// @name            Chrome: Bypass Save As dialog
// @description     Bypass the Save As dialog in Chrome and immediatly save file
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @include         chrome.exe
// @compilerOptions -lcomdlg32 -lOle32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Chrome: Bypass Save As dialog
Bypass the Save As dialog in Chrome and immediatly save file
*/
// ==/WindhawkModReadme==

#include <initguid.h>
#include <processthreadsapi.h>
#include <shobjidl.h>
#include <windhawk_api.h>
#include <winerror.h>
#include <winnt.h>
#include <thread>
#include <dwmapi.h>

struct ccomobj {
    void** lpVtbl;
};

DWORD dwThreadId = 0;

HWND (*CreateWindowExWFunc)(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) = nullptr;
HWND CreateWindowExWHook(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    return CreateWindowExWFunc(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

BOOL CALLBACK EnumButtons(HWND hWnd, LPARAM lp) {
    if (GetMenu(hWnd) == reinterpret_cast<HMENU>(IDOK)) {
        *(reinterpret_cast<HWND*>(lp)) = hWnd;
        return FALSE;
    }
    return TRUE;
}

INT_PTR (CALLBACK *DlgProcFunc)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) = nullptr;
INT_PTR CALLBACK DlgProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_INITDIALOG) {
        BOOL cloak = TRUE;
        DwmSetWindowAttribute(hWnd, DWMWA_CLOAK, reinterpret_cast<void*>(&cloak), sizeof(BOOL));
    } else if (uMsg == WM_SHOWWINDOW) {
        HWND hBtnSave = nullptr;
        EnumChildWindows(hWnd, &EnumButtons, reinterpret_cast<LPARAM>(&hBtnSave));
        if (hBtnSave) {
            PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), reinterpret_cast<LPARAM>(hBtnSave));
        }
    }
    return DlgProcFunc(hWnd, uMsg, wParam, lParam);
}

INT_PTR (*DialogBoxIndirectParamWFunc)(HINSTANCE hInstance, LPCDLGTEMPLATEW hDialogTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) = nullptr;
INT_PTR DialogBoxIndirectParamWHook(HINSTANCE hInstance, LPDLGTEMPLATEW hDialogTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    if (dwThreadId == GetCurrentThreadId()) {
        DlgProcFunc = lpDialogFunc;
        lpDialogFunc = &DlgProcHook;
        dwThreadId = 0;
    }
    return DialogBoxIndirectParamWFunc(hInstance, hDialogTemplate, hWndParent, lpDialogFunc, dwInitParam);
}

HRESULT (*IModalWindow_ShowFunc)(void* _this, HWND hWndOwner) = nullptr;
HRESULT IModalWindow_ShowHook(void* _this, HWND hWndOwner) {
    dwThreadId = GetCurrentThreadId();
    return IModalWindow_ShowFunc(_this, hWndOwner);
}

HRESULT (*CoCreateInstanceFunc)(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv) = nullptr;
HRESULT CoCreateInstanceHook(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv) {
    if (!(GetAsyncKeyState(VK_MENU) & 0x01) && IsEqualCLSID(rclsid, CLSID_FileSaveDialog) && IsEqualIID(riid, __uuidof(IFileSaveDialog))) {
        auto hr = CoCreateInstanceFunc(rclsid, pUnkOuter, dwClsContext, riid, ppv);
        if (SUCCEEDED(hr)) {
            IFileSaveDialog *pfd = reinterpret_cast<IFileSaveDialog*>(*ppv);
            if (pfd) {
                struct ccomobj* praw = reinterpret_cast<struct ccomobj*>(pfd);
                DWORD dwOldProtect = PAGE_EXECUTE_READWRITE;
                if (VirtualProtect(praw->lpVtbl + 3, sizeof(void*), dwOldProtect, &dwOldProtect)) {
                    IModalWindow_ShowFunc = reinterpret_cast<decltype(IModalWindow_ShowFunc)>(praw->lpVtbl[3]);
                    praw->lpVtbl[3] = reinterpret_cast<void*>(&IModalWindow_ShowHook);
                    VirtualProtect(praw->lpVtbl + 3, sizeof(void*), dwOldProtect, &dwOldProtect);
                }
            }
        }
        return hr;
    }
    return CoCreateInstanceFunc(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

BOOL Wh_ModInit() {
    Wh_Log(L"Init");
    //Wh_SetFunctionHook(reinterpret_cast<void*>(&CreateWindowExW), reinterpret_cast<void*>(&CreateWindowExWHook), reinterpret_cast<void**>(&CreateWindowExWFunc));
    Wh_SetFunctionHook(reinterpret_cast<void*>(&DialogBoxIndirectParamW), reinterpret_cast<void*>(&DialogBoxIndirectParamWHook), reinterpret_cast<void**>(&DialogBoxIndirectParamWFunc));
    Wh_SetFunctionHook(reinterpret_cast<void*>(&CoCreateInstance), reinterpret_cast<void*>(&CoCreateInstanceHook), reinterpret_cast<void**>(&CoCreateInstanceFunc));
    return TRUE;
}

// The mod is being unloaded, free all allocated resources.
void Wh_ModUninit() {
    //if (CreateWindowExWFunc) Wh_RemoveFunctionHook(reinterpret_cast<void*>(&CreateWindowExW));
    if (DialogBoxIndirectParamWFunc) Wh_RemoveFunctionHook(reinterpret_cast<void*>(&DialogBoxIndirectParamW));
    if (CoCreateInstanceFunc) Wh_RemoveFunctionHook(reinterpret_cast<void*>(&CoCreateInstance));
    Wh_Log(L"Uninit");
}

// The mod setting were changed, reload them.
void Wh_ModSettingsChanged() {
    Wh_Log(L"SettingsChanged");
}
