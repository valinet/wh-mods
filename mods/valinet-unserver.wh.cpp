// ==WindhawkMod==
// @id              valinet-unserver
// @name            Treat server OS as client OS
// @description     Make server OS report as client OS for specific programs.
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @homepage        https://valinet.ro
// @include         taskmgr.exe
// @include         disk_image.exe
// @compilerOptions -lshlwapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Treat server OS as client OS
Make server OS report as client OS for specific apps.
*/
// ==/WindhawkModReadme==

#include <handleapi.h>
#include <libloaderapi.h>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <shlwapi.h>
#include <sysinfoapi.h>
#include <windhawk_api.h>
#include <winnt.h>
#include <versionhelpers.h>
#include <string>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif

// https://stackoverflow.com/questions/937044/determine-path-to-registry-key-from-hkey-handle-in-c
std::wstring GetPathFromHKEY(HKEY key) {
    std::wstring keyPath;
    if (key != NULL) {
        HMODULE dll = GetModuleHandleW(L"ntdll.dll");
        if (dll != NULL) {
            typedef NTSTATUS (__stdcall *NtQueryKeyType)(
                HANDLE  KeyHandle,
                int KeyInformationClass,
                PVOID  KeyInformation,
                ULONG  Length,
                PULONG  ResultLength);
            NtQueryKeyType func = reinterpret_cast<NtQueryKeyType>(GetProcAddress(dll, "NtQueryKey"));
            if (func != NULL) {
                DWORD size = 0;
                NTSTATUS result = func(key, 3, 0, 0, &size);
                if (result == STATUS_BUFFER_TOO_SMALL) {
                    size = size + 2;
                    wchar_t* buffer = new (std::nothrow) wchar_t[size/sizeof(wchar_t)]; // size is in bytes
                    if (buffer != NULL) {
                        result = func(key, 3, buffer, size, &size);
                        if (result == STATUS_SUCCESS) {
                            buffer[size / sizeof(wchar_t)] = L'\0';
                            keyPath = std::wstring(buffer + 2);
                        }
                        delete[] buffer;
                    }
                }
            }
        }
    }
    return keyPath;
}

LSTATUS (*RegQueryValueExWFunc)(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
LSTATUS RegQueryValueExWHook(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    Wh_Log(L"RegQueryValueExWHook\n");
    std::wstring path;
    if (!wcsicmp(lpValueName, L"ProductType") && (path = GetPathFromHKEY(hKey), !wcsicmp(path.c_str(), L"\\REGISTRY\\MACHINE\\SYSTEM\\ControlSet001\\Control\\ProductOptions"))) {
        DWORD len = 20;
        wchar_t buffer[len];
        auto rv = RegQueryValueExWFunc(hKey, lpValueName, lpReserved, lpType, (LPBYTE)buffer, &len);
        if (lpData) wcscpy(((wchar_t*)lpData), L"WinNT");
        if (lpcbData) *lpcbData = 12;
        return rv;
    }
    auto rv = RegQueryValueExWFunc(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    return rv;
}

NTSTATUS (*RtlGetVersionFunc)(PRTL_OSVERSIONINFOEXW lpVersionInformation) = nullptr;
NTSTATUS RtlGetVersionHook(PRTL_OSVERSIONINFOEXW lpVersionInformation) {
    Wh_Log(L"RtlGetVersionHook\n");
    auto rv = RtlGetVersionFunc(lpVersionInformation);
    if (lpVersionInformation->dwOSVersionInfoSize == sizeof(RTL_OSVERSIONINFOEXW)) {
        lpVersionInformation->wProductType = VER_NT_WORKSTATION;
    }
    return rv;
}

BOOL (*GetVersionExFunc)(LPOSVERSIONINFOEXW lpVersionInformation) = nullptr;
BOOL GetVersionExWHook(LPOSVERSIONINFOEXW lpVersionInformation) {
    Wh_Log(L"GetVersionExWHook\n");
    auto rv = GetVersionExFunc(lpVersionInformation);
    if (lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXW)) {
        lpVersionInformation->wProductType = VER_NT_WORKSTATION;
    }
    return rv;
}

BOOL(*IsOSFunc)(DWORD dwOS) = nullptr;
BOOL IsOSHook(DWORD dwOS) {
    Wh_Log(L"IsOSHook\n");
    if (dwOS == OS_ANYSERVER) return FALSE;
    return IsOSFunc(dwOS);
}

BOOL(*IsWindowsServerFunc)() = nullptr;
BOOL IsWindowsServerHook() {
    Wh_Log(L"IsWindowsServerHook\n");
    return FALSE;
}

BOOL Wh_ModInit() {
    Wh_Log(L"Init\n");
#ifdef _WIN64
    const size_t OFFSET_SAME_TEB_FLAGS = 0x17EE;
#else
    const size_t OFFSET_SAME_TEB_FLAGS = 0x0FCA;
#endif
    bool isInitialThread = *(USHORT*)((BYTE*)NtCurrentTeb() + OFFSET_SAME_TEB_FLAGS) & 0x0400;
    Wh_Log(L"isInitialThread=%d", isInitialThread);
    Wh_SetFunctionHook(reinterpret_cast<void*>(&RegQueryValueExW), reinterpret_cast<void*>(&RegQueryValueExWHook), reinterpret_cast<void**>(&RegQueryValueExWFunc));
    Wh_SetFunctionHook(reinterpret_cast<void*>(&GetVersionExW), reinterpret_cast<void*>(&GetVersionExWHook), reinterpret_cast<void**>(&GetVersionExFunc));
    Wh_SetFunctionHook(reinterpret_cast<void*>(&IsOS), reinterpret_cast<void*>(&IsOSHook), reinterpret_cast<void**>(&IsOSFunc));
    Wh_SetFunctionHook(reinterpret_cast<void*>(&IsWindowsServer), reinterpret_cast<void*>(&IsWindowsServerHook), reinterpret_cast<void**>(&IsWindowsServerFunc));
    Wh_SetFunctionHook(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")), reinterpret_cast<void*>(&RtlGetVersionHook), reinterpret_cast<void**>(&RtlGetVersionFunc));
    return TRUE;
}

void Wh_ModUninit() {
    if (RegQueryValueExWFunc) Wh_RemoveFunctionHook(reinterpret_cast<void*>(&RegQueryValueExW));
    if (GetVersionExFunc) Wh_RemoveFunctionHook(reinterpret_cast<void*>(&GetVersionExW));
    if (IsOSFunc) Wh_RemoveFunctionHook(reinterpret_cast<void*>(&IsOS));
    if (IsWindowsServerFunc) Wh_RemoveFunctionHook(reinterpret_cast<void*>(&IsWindowsServer));
    if (RtlGetVersionFunc) Wh_RemoveFunctionHook(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")));
    Wh_Log(L"Uninit");
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"SettingsChanged");
}
