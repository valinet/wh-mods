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
#include <thread>
#include <psapi.h>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif

void* memmem(void* haystack, size_t haystacklen, const char* pattern, const char* mask) {
    char* text = (char*)haystack;
    size_t needlelen = strlen(mask);
    char* rv = NULL;

    size_t* out = (size_t*)calloc(needlelen, sizeof(size_t));
    if (!out) return nullptr;
    size_t j, i;

    j = 0, i = 1;
    while (i < needlelen) {
        if (text[j] != text[i]) {
            if (j > 0) {
                j = out[j - 1];
                continue;
            }
            else j--;
        }
        j++;
        out[i] = j;
        i++;
    }

    i = 0, j = 0;
    for (i = 0; i <= haystacklen - needlelen; i++) {
        if (mask[j] == 'x' ? text[i] == pattern[j] : true) {
            j++;
            if (j == needlelen) {
                rv = text + (int)(i - needlelen + 1); //match++; j = out[j - 1];
                break;
            }
        }
        else {
            if (j != 0) {
                j = out[j - 1];
                i--;
            }
        }
    }

    free(out);
    return (void*)rv;
}

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

void PatchKUserSharedDataAccesses() {
    // This function tries to redirect memory reads of KUSER_SHARED_DATA
    // KUSER_SHARED_DATA is a section mapped into all processes at a fixed
    // address (0x7FFE0000) which caches various frequently accessed info
    // about the OS, including the product type. Client should read 1, while
    // Server reads a 3. Here, we try to redirect memory accesses from the
    // NtProductType field to the ComPlusPackage fiels, which should always
    // be 1.
    auto p = GetModuleHandle(nullptr);
    MODULEINFO mi;
    K32GetModuleInformation(GetCurrentProcess(), p, &mi, sizeof(MODULEINFO));
    char bytes[] = { '\x64', '\x02', '\xFE', '\x7F'};
    int len = sizeof(bytes);
    void *newPattern = p;
    auto count = 0;
    while ((newPattern = memmem(newPattern, mi.SizeOfImage - ((char*)newPattern - (char*)p), bytes, "xxxx"))) {
        MEMORY_BASIC_INFORMATION info {};
        if (VirtualQueryEx(GetCurrentProcess(), reinterpret_cast<LPCVOID>(newPattern), &info, sizeof(info)) == sizeof(info) && 
            info.State == MEM_COMMIT && 
            ((info.Protect & PAGE_EXECUTE) || (info.Protect & PAGE_EXECUTE_READ) || (info.Protect & PAGE_EXECUTE_READWRITE) || (info.Protect & PAGE_EXECUTE_WRITECOPY))) {
            int idx = 0;
            for (auto i = 0; i < 6; ++i) {
                WH_DISASM_RESULT result;
                Wh_Disasm((char*)newPattern - i, &result);
                if (strstr(result.text, "[0x000000007FFE0264]")) {
                    idx = i + 1;
                    std::string s(result.text);
                    std::wstring ws(s.size(), L' ');
                    ws.resize(std::mbstowcs(&ws[0], s.c_str(), s.size()));
                    Wh_Log(L">> Potential instruction: \"%s\"\n", ws.c_str());
                    break;
                }
            }
            if (idx) {
                ++count;
                DWORD dwOldProtect = PAGE_EXECUTE_READWRITE;
                VirtualProtect(newPattern, 4, dwOldProtect, &dwOldProtect);
                ((char*)newPattern)[0] = 0xE0;
                ((char*)newPattern)[1] = 0x02;
                VirtualProtect(newPattern, 4, dwOldProtect, &dwOldProtect);
                WH_DISASM_RESULT result;
                Wh_Disasm((char*)newPattern - idx + 1, &result);
                if (strstr(result.text, "[0x000000007FFE")) {
                    std::string s(result.text);
                    std::wstring ws(s.size(), L' ');
                    ws.resize(std::mbstowcs(&ws[0], s.c_str(), s.size()));
                    Wh_Log(L">> Resulting instruction: \"%s\"\n", ws.c_str());
                }
            }
        }
        newPattern = (char*)newPattern + len;
    }
    Wh_Log(L"Performed %d patches.\n", count);
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
    auto isUpxCompressed = [](){
        HMODULE hModule = GetModuleHandleW(nullptr);
        if (hModule) {
            PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
            if (dosHeader) {
                PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
                if (ntHeaders) {
                    PIMAGE_SECTION_HEADER sectionHeaders = (PIMAGE_SECTION_HEADER)((BYTE*)&ntHeaders->OptionalHeader + ntHeaders->FileHeader.SizeOfOptionalHeader);
                    if (sectionHeaders) {
                        for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
                            if (std::string((char*)sectionHeaders[i].Name) == "UPX0") {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }();
    Wh_Log(L"isUpxCompressed=%d\n", isUpxCompressed);
    if (!isUpxCompressed) PatchKUserSharedDataAccesses();
    else {
        HANDLE hFirstThread = INVALID_HANDLE_VALUE;
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &hFirstThread, 0, false, DUPLICATE_SAME_ACCESS);
        if (hFirstThread && hFirstThread != INVALID_HANDLE_VALUE) std::thread([hFirstThread](){
            bool suspended = false;

            // Wait for UPX to unpack executable
            auto start = GetTickCount64();
            while (true) {
                PCONTEXT pContext = nullptr;
                unsigned long len = 4096;
                char buffer[len];
                InitializeContext(reinterpret_cast<void*>(buffer), CONTEXT_FULL, &pContext, &len);
                GetThreadContext(hFirstThread, pContext);
                MEMORY_BASIC_INFORMATION info {};
    #ifdef _WIN64
                if (VirtualQueryEx(GetCurrentProcess(), reinterpret_cast<LPCVOID>(pContext->Rip), &info, sizeof(info)) == sizeof(info) && info.State == MEM_COMMIT && info.Protect == PAGE_EXECUTE_READWRITE) {
    #else
                if (VirtualQueryEx(GetCurrentProcess(), reinterpret_cast<LPCVOID>(pContext->Eip), &info, sizeof(info)) == sizeof(info) && info.State == MEM_COMMIT && info.Protect == PAGE_EXECUTE_READWRITE) {
    #endif
                    suspended = true;
                    SuspendThread(hFirstThread);
                    break;
                }
                if (GetTickCount64() - start > 2000) break;
            }
            PatchKUserSharedDataAccesses();
            if (suspended) ResumeThread(hFirstThread);
            CloseHandle(hFirstThread);
        }).detach();
    }
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
