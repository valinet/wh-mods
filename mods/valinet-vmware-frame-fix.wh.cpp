// ==WindhawkMod==
// @id              valinet-vmware-frame-fix
// @name            VMware: Fix window frame
// @description     VMware: Fix window frame
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @homepage        https://valinet.ro
// @include         vmware.exe
// @compilerOptions -lcomctl32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*...*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

HWND subclassed = nullptr;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD_PTR dwRefData) {
    if (uMsg == WM_NCCALCSIZE || uMsg == WM_NCPAINT || uMsg == WM_NCACTIVATE) {
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

using GetClientRect_t = decltype(&GetClientRect);
GetClientRect_t GetClientRectFunc;
BOOL WINAPI GetClientRectHook(HWND hWnd, LPRECT lpRect) {
    if (!subclassed && GetClassWord(hWnd, GCW_ATOM) == RegisterWindowMessageW(L"VMUIFrame") && WindhawkUtils::SetWindowSubclassFromAnyThread(hWnd, WndProc, 0)) {
        subclassed = hWnd;
    }
    return GetClientRectFunc(hWnd, lpRect);
}

BOOL Wh_ModInit() {
    subclassed = FindWindowExW(nullptr, nullptr, L"VMUIFrame", nullptr);
    if (subclassed) {
        if (!WindhawkUtils::SetWindowSubclassFromAnyThread(subclassed, WndProc, 0)) subclassed = nullptr;
    } else {
        Wh_SetFunctionHook(reinterpret_cast<void*>(&GetClientRect), reinterpret_cast<void*>(&GetClientRectHook), reinterpret_cast<void**>(&GetClientRectFunc));
    }
    return TRUE;
}

void Wh_ModUninit() {
    if (GetClientRectFunc) Wh_RemoveFunctionHook(reinterpret_cast<void*>(&GetClientRect));
    if (subclassed) WindhawkUtils::RemoveWindowSubclassFromAnyThread(subclassed, WndProc);
}
