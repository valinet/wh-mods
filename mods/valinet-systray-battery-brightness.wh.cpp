// ==WindhawkMod==
// @id              valinet-systray-battery-brightness
// @name            Systray Battery Brightness
// @description     Change display brightness by scrolling over the battery icon.
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @include         explorer.exe
// @compilerOptions -lcomctl32 -lDxva2
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Systray Battery Brightness
Change display brightness by scrolling over the battery icon.
*/
// ==/WindhawkModReadme==

#include <thread>
#include <processthreadsapi.h>
#include <sysinfoapi.h>
#include <windhawk_utils.h>
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>

#define UID_TRAYICONVOLUME  100
#define GUID_TRAYICONVOLUME { 0x7820AE73, 0x23E3, 0x4229, { 0x82, 0xC1, 0xE4, 0x1C, 0xB6, 0x7D, 0x5B, 0x9C } };

#define UID_TRAYICONPOWER  1225
#define GUID_TRAYICONPOWER { 0x7820AE75, 0x23E3, 0x4229, { 0x82, 0xC1, 0xE4, 0x1C, 0xB6, 0x7D, 0x5B, 0x9C } };

#define UID_TRAYICONHOTPLUG  1226
#define GUID_TRAYICONHOTPLUG { 0x7820AE78, 0x23E3, 0x4229, { 0x82, 0xC1, 0xE4, 0x1C, 0xB6, 0x7D, 0x5B, 0x9C } };

bool subclassed = false;
HWND hWnd = nullptr;
int brightness = 0;
#define STEP 5

BOOL CALLBACK MonitorBrightness(HMONITOR unnamedParam1, HDC unnamedParam2, LPRECT unnamedParam3, LPARAM unnamedParam4) {
    LPPHYSICAL_MONITOR pPhysicalMonitors = nullptr;
    DWORD cPhysicalMonitors = 0;
    BOOL bSuccess = GetNumberOfPhysicalMonitorsFromHMONITOR(unnamedParam1, &cPhysicalMonitors);
    if (bSuccess) {
        pPhysicalMonitors = (LPPHYSICAL_MONITOR)malloc(cPhysicalMonitors * sizeof(PHYSICAL_MONITOR));
        if (pPhysicalMonitors != NULL) {
            bSuccess = GetPhysicalMonitorsFromHMONITOR(unnamedParam1, cPhysicalMonitors, pPhysicalMonitors);
            if (unnamedParam4 > 100) {
                DWORD min, max, cur = 0;
                GetMonitorBrightness(pPhysicalMonitors[0].hPhysicalMonitor, &min, &cur, &max);
                DWORD* c = (DWORD*)unnamedParam4;
                *c = cur;
                return false;
            }
            SetMonitorBrightness(pPhysicalMonitors[0].hPhysicalMonitor, unnamedParam4);
            bSuccess = DestroyPhysicalMonitors(cPhysicalMonitors, pPhysicalMonitors);
            free(pPhysicalMonitors);
        }
    }
    return true;
}

LRESULT WINAPI SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD_PTR dwRefData) {
    if (uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL) {
        NOTIFYICONIDENTIFIER nid {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hWnd;
        nid.uID = UID_TRAYICONPOWER;
        nid.guidItem = GUID_TRAYICONPOWER;
        RECT rc{};
        POINT pt{};
        GetCursorPos(&pt);
        if (SUCCEEDED(Shell_NotifyIconGetRect(&nid, &rc)) && PtInRect(&rc, pt)) {
            int factor = GET_WHEEL_DELTA_WPARAM(wParam) / 120;
            brightness += (factor * STEP);
            if (brightness < 0) brightness = 0;
            else if (brightness > 100) brightness = 100;
            EnumDisplayMonitors(nullptr, nullptr, MonitorBrightness, static_cast<LPARAM>(brightness));
        }
    } else if (uMsg == WM_DESTROY) {
        subclassed = false;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

BOOL Wh_ModInit() {
    Wh_Log(L"Init");
    std::thread([](){
        auto start = GetTickCount64();
        while (true) {
            hWnd = FindWindowW(L"Shell_TrayWnd", nullptr);
            if (hWnd) {
                hWnd = FindWindowExW(hWnd, nullptr, L"TrayNotifyWnd", nullptr);
                if (hWnd) {
                    hWnd = FindWindowExW(hWnd, nullptr, L"SysPager", nullptr);
                    if (hWnd) {
                        hWnd = FindWindowExW(hWnd, nullptr, L"ToolbarWindow32", nullptr);
                    }
                }
            }
            if (hWnd || GetTickCount64() - start > 5000) break;
        }
        if (hWnd) {
            DWORD dwProcessId = 0;
            GetWindowThreadProcessId(hWnd, &dwProcessId);
            if (dwProcessId == GetCurrentProcessId()) {
                EnumDisplayMonitors(nullptr, nullptr, MonitorBrightness, reinterpret_cast<LPARAM>(&brightness));
                subclassed = WindhawkUtils::SetWindowSubclassFromAnyThread(hWnd, &SubclassProc, 0);
            }
        }
    }).detach();
    return TRUE;
}

void Wh_ModUninit() {
    if (subclassed) WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd, &SubclassProc);
    Wh_Log(L"Uninit");
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"SettingsChanged");
}
