// ==WindhawkMod==
// @id              valinet-custom-keyboard
// @name            Custom Keyboard Mapping
// @description     Hackish way to have different customized key mappings for each physical keyboard.
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @include         windhawk.exe
// @compilerOptions -lWtsapi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Custom Keyboard Mapping
Hackish way to have different customized key mappings for each physical keyboard.
*/
// ==/WindhawkModReadme==

#include <debugapi.h>
#include <handleapi.h>
#include <libloaderapi.h>
#include <processthreadsapi.h>
#include <thread>
#include <psapi.h>
#include <synchapi.h>
#include <sysinfoapi.h>
#include <tlhelp32.h>
#include <hidsdi.h>
#include <winerror.h>
#include <wtsapi32.h>
#include <unordered_map>

#define ID_KB_LAPTOP  0x777528c4
#define ID_KB_DESKTOP 0x86893132

#define Wh_Log_External
#define Wh_Log_Original
//#define Wh_Log_External OutputDebugString
//#define Wh_Log_Original Wh_Log

#define QWORD INT64

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)

uint32_t generateIdFromWString(std::wstring& str) {
    unsigned int h;
    wchar_t *p;
    h = 0;
    for (p = (wchar_t*)str.data(); *p != '\0'; p++)
        h = 31 * h + *p;
    return h; // or, h % ARRAY_SIZE;
}

BOOL IsLocalSystem() {
    HANDLE hToken;
    UCHAR bTokenUser[sizeof(TOKEN_USER) + 8 + 4 * SID_MAX_SUB_AUTHORITIES];
    PTOKEN_USER pTokenUser = (PTOKEN_USER)bTokenUser;
    ULONG cbTokenUser;
    SID_IDENTIFIER_AUTHORITY siaNT = SECURITY_NT_AUTHORITY;
    PSID pSystemSid;
    BOOL bSystem;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) return FALSE;
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, sizeof(bTokenUser), &cbTokenUser)) {
        CloseHandle(hToken);
        return FALSE;
    }
    CloseHandle(hToken);
    if (!AllocateAndInitializeSid(&siaNT, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &pSystemSid)) return FALSE;
    bSystem = EqualSid(pTokenUser->User.Sid, pSystemSid);
    FreeSid(pSystemSid);
    return bSystem;
}

struct keyboardData {
    uint32_t id;
    std::wstring hardwareId;
    std::wstring friendlyName;
} currentKeyboard;
std::unordered_map<HANDLE, struct keyboardData> keyboards;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT* st = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (!(st->flags & LLKHF_INJECTED) && nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_KEYUP)) {
        if (currentKeyboard.id == ID_KB_LAPTOP && st->vkCode == VK_F12) {
            keybd_event(VK_END, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (currentKeyboard.id == ID_KB_LAPTOP && st->vkCode == VK_END) {
            keybd_event(VK_F12, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (currentKeyboard.id == ID_KB_LAPTOP && st->vkCode == VK_F11) {
            keybd_event(VK_HOME, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (currentKeyboard.id == ID_KB_LAPTOP && st->vkCode == VK_HOME) {
            keybd_event(VK_F11, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (currentKeyboard.id == ID_KB_LAPTOP && st->vkCode == VK_F10) {
            keybd_event(VK_PRINT, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (currentKeyboard.id == ID_KB_LAPTOP && st->vkCode == VK_PRINT) {
            keybd_event(VK_F10, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (st->vkCode == VK_F1) {
            keybd_event(VK_MEDIA_PLAY_PAUSE, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (st->vkCode == VK_F2) {
            keybd_event(VK_VOLUME_DOWN, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (st->vkCode == VK_F3) {
            keybd_event(VK_VOLUME_UP, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (st->vkCode == VK_VOLUME_MUTE) {
            keybd_event(VK_F1, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (st->vkCode == VK_VOLUME_DOWN) {
            keybd_event(VK_F2, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (st->vkCode == VK_VOLUME_UP) {
            keybd_event(VK_F3, 0, wParam == WM_KEYUP ? KEYEVENTF_KEYUP : 0, 0);
            return 1;
        } else if (st->vkCode == VK_CAPITAL) {
            if (wParam == WM_KEYUP) keybd_event(VK_F2, 0, KEYEVENTF_KEYUP, 0);
            else keybd_event(VK_F2, 0, 0, 0);
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

extern "C" int procMain(HWND hWnd, HINSTANCE hInstance, LPSTR lpszCmdLine, int nCmdShow) {
    int rv = 0xFFFFFFFF;

    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, true, nullptr, false);
    SECURITY_ATTRIBUTES sa = { };
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = false;
    sa.lpSecurityDescriptor = &sd;
    SetLastError(ERROR_SUCCESS);
    HANDLE mutSingleInstance = CreateMutexW(&sa, false, L"Global\\{4B360CA7-967F-4FA2-BC4A-885F1106ACB0}");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutSingleInstance);
        ExitProcess(ERROR_ALREADY_EXISTS);
    }

    MSG msg;
    PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE);
    wchar_t wszMsg[MAX_PATH * 4];
    currentKeyboard.id = ID_KB_LAPTOP;

    BOOL bRegisteredForWTS = WTSRegisterSessionNotification(hWnd, NOTIFY_FOR_ALL_SESSIONS);
    swprintf_s(wszMsg, L"WTSRegisterSessionNotification: %d\n", bRegisteredForWTS);
    Wh_Log_External(wszMsg);

    DWORD dwProcessSessionId = 0xFFFFFFFF, dwActiveSessionId = WTSGetActiveConsoleSessionId();
    ProcessIdToSessionId(GetCurrentProcessId(), &dwProcessSessionId);
    if (dwActiveSessionId != 0xFFFFFFFF && dwProcessSessionId != dwActiveSessionId) {
        swprintf_s(wszMsg, L"Did not start in active session, exiting...\n");
        Wh_Log_External(wszMsg);
        if (bRegisteredForWTS) WTSUnRegisterSessionNotification(hWnd);
        if (mutSingleInstance) CloseHandle(mutSingleInstance);
        ExitProcess(rv);
    }

    HHOOK hHook = nullptr;
    hHook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc, hInstance, 0);
    swprintf_s(wszMsg, L"SetWindowsHookExW: %d\n", hHook);
    Wh_Log_External(wszMsg);

    RAWINPUTDEVICE rid{};
    rid.dwFlags = RIDEV_NOLEGACY | RIDEV_INPUTSINK;
    rid.usUsagePage = 1;
    rid.usUsage = 6;
    rid.hwndTarget = hWnd;
    swprintf_s(wszMsg, L"RegisterRawInputDevices: %d\n", RegisterRawInputDevices(&rid, 1, sizeof(rid)));
    Wh_Log_External(wszMsg);

    HMODULE hHid = nullptr;
	hHid = LoadLibraryW(L"Hid.dll");
    BOOLEAN WINAPI (*HidD_GetProductStringFunc)(HANDLE, PVOID, ULONG) = nullptr;
    if (hHid) HidD_GetProductStringFunc = reinterpret_cast<BOOLEAN WINAPI (*)(HANDLE, PVOID, ULONG)>(GetProcAddress(hHid, "HidD_GetProductString"));
    swprintf_s(wszMsg, L"HidD_GetProductStringFunc: %p\n", HidD_GetProductStringFunc);
    Wh_Log_External(wszMsg);

    while (true) {
        bool isQuiting = false;
        rv = MsgWaitForMultipleObjects(0, nullptr, false, INFINITE, QS_ALLINPUT);
        if (rv != WAIT_OBJECT_0) break;
        UINT64 rawBuffer[1024 / 8];
        UINT bytes = sizeof(rawBuffer);
        auto count = GetRawInputBuffer((PRAWINPUT)rawBuffer, &bytes, sizeof(RAWINPUTHEADER));
        if (count > 0) {
            const RAWINPUT* raw = (const RAWINPUT*)rawBuffer;
            while (true) { 
                if (raw->header.dwType == RIM_TYPEKEYBOARD && raw->header.hDevice) {
                    if (!keyboards.contains(raw->header.hDevice)) {
                        TCHAR deviceName[MAX_PATH * 4]{};
                        UINT size = sizeof(deviceName);
                        if (GetRawInputDeviceInfoW(raw->header.hDevice, RIDI_DEVICENAME, deviceName, &size) > 0) {
                            HANDLE HIDHandle = CreateFileW(deviceName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
                            if (HIDHandle && HIDHandle != INVALID_HANDLE_VALUE) {
                                wchar_t wszProductString[MAX_PATH * 4]{};
                                if (HidD_GetProductStringFunc) HidD_GetProductStringFunc(HIDHandle, wszProductString, sizeof(wszProductString));
                                struct keyboardData keyboard;
                                keyboard.hardwareId = std::wstring(deviceName);
                                keyboard.id = generateIdFromWString(keyboard.hardwareId);
                                keyboard.friendlyName = std::wstring(wszProductString);
                                keyboards.insert({ raw->header.hDevice, keyboard });
                                swprintf_s(wszMsg, L">>> adding %x, %x\n", raw->header.hDevice, keyboard.id);// : %x <> %s , %s\n", raw->header.hDevice, keyboard.id, keyboard.friendlyName.c_str(), keyboard.hardwareId.c_str());
                                Wh_Log_External(wszMsg);
                                CloseHandle(HIDHandle);
                            }
                        }
                    }
                    if (keyboards.contains(raw->header.hDevice)) {
                        currentKeyboard = keyboards.at(raw->header.hDevice);
                        swprintf_s(wszMsg, L"keyboard: %x\n", keyboards.at(raw->header.hDevice).id);
                        Wh_Log_External(wszMsg);
                    }
                }
                count--;
                if (count <= 0) break;
                raw = NEXTRAWINPUTBLOCK(raw);
            }
            while (PeekMessageW(&msg, 0, WM_INPUT, WM_INPUT, PM_REMOVE)) {};
        }
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                swprintf_s(wszMsg, L"Quitting (WM_QUIT)...\n");
                Wh_Log_External(wszMsg);
                rv = 0xFFFFFFFF;
                isQuiting = true;
                break;
            }
            else if (msg.message == WM_WTSSESSION_CHANGE) {
                if (msg.wParam == WTS_CONSOLE_CONNECT || msg.wParam == WTS_SESSION_LOGOFF) {
                    swprintf_s(wszMsg, L"Quitting (WM_WTSSESSION_CHANGE, %d, %d)...\n", msg.wParam, msg.lParam);
                    Wh_Log_External(wszMsg);
                    rv = msg.lParam;
                    isQuiting = true;
                    break;
                }
            }
            else {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        if (isQuiting) break;
    }

    if (hHid) FreeLibrary(hHid);
    if (hHook) UnhookWindowsHookEx(hHook);
    if (bRegisteredForWTS) WTSUnRegisterSessionNotification(hWnd);
    if (mutSingleInstance) CloseHandle(mutSingleInstance);
    ExitProcess(rv);
}

DWORD WINAPI procClose(LPVOID dwMainTid) {
    return PostThreadMessageW(reinterpret_cast<intptr_t>(dwMainTid), WM_QUIT, 0, 0);
}

PROCESS_INFORMATION pi{};
std::mutex mut_pi;
std::thread worker;

BOOL Wh_ModInit() {
    bool isSystemAccount = IsLocalSystem();
    Wh_Log_Original(L"Init %d\n", isSystemAccount);
    if (isSystemAccount) {
        mut_pi.lock();
        worker = std::thread([](){
            Wh_Log_Original(L"Started worker.\n");
            while (true) {
                wchar_t wszPath[MAX_PATH];
                GetWindowsDirectory(wszPath, MAX_PATH);
                wchar_t wszArguments[MAX_PATH]{};
                GetModuleFileNameW(HINST_THISCOMPONENT, wszArguments, MAX_PATH);
                wchar_t wszCommand[MAX_PATH * 3];
                BOOL amI32Bit = FALSE;
                if (!IsWow64Process(GetCurrentProcess(), &amI32Bit) || amI32Bit) swprintf_s(wszCommand, L"\"%s\\SysWOW64\\rundll32.exe\" %s,procMain", wszPath, wszArguments);
                else swprintf_s(wszCommand, L"\"%s\\System32\\rundll32.exe\" %s,procMain", wszPath, wszArguments);
                Wh_Log_Original(L"%s\n", wszCommand);
                HANDLE procInteractiveWinlogon = INVALID_HANDLE_VALUE;
                PROCESSENTRY32 entry;
                entry.dwSize = sizeof(PROCESSENTRY32);
                HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                if (hSnapshot) {
                    if (Process32First(hSnapshot, &entry) == TRUE) {
                        DWORD dwActiveSessionId = WTSGetActiveConsoleSessionId();
                        if (dwActiveSessionId == 0xFFFFFFFF) {
                            Wh_Log_Original(L"No session is active.\n");
                        }
                        while (Process32Next(hSnapshot, &entry) == TRUE) {
                            if (!wcsicmp(entry.szExeFile, L"winlogon.exe")) {
                                DWORD dwWinLogonSessionId = 0xFFFFFFFF;
                                ProcessIdToSessionId(entry.th32ProcessID, &dwWinLogonSessionId);
                                if (dwActiveSessionId == 0xFFFFFFFF || dwActiveSessionId == dwWinLogonSessionId) {
                                    if ((procInteractiveWinlogon = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID))) {
                                        BOOL bIs32Bit = FALSE;
                                        if (!IsWow64Process(procInteractiveWinlogon, &bIs32Bit) || bIs32Bit) {
                                            CloseHandle(procInteractiveWinlogon);
                                            procInteractiveWinlogon = INVALID_HANDLE_VALUE;
                                            continue;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    CloseHandle(hSnapshot);
                }
                Wh_Log_Original(L"procInteractiveWinlogon: %p\n", procInteractiveWinlogon);
                HANDLE tknInteractive = INVALID_HANDLE_VALUE;
                if (procInteractiveWinlogon && procInteractiveWinlogon != INVALID_HANDLE_VALUE) {
                    HANDLE tknWinlogon = INVALID_HANDLE_VALUE;
                    if (OpenProcessToken(procInteractiveWinlogon, TOKEN_DUPLICATE, &tknWinlogon) && tknWinlogon && tknWinlogon != INVALID_HANDLE_VALUE) {
                        SECURITY_ATTRIBUTES tokenAttributes;
                        ZeroMemory(&tokenAttributes, sizeof(SECURITY_ATTRIBUTES));
                        tokenAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
                        DuplicateTokenEx(tknWinlogon, 0x10000000, &tokenAttributes, SecurityImpersonation, TokenImpersonation, &tknInteractive);
                        CloseHandle(tknWinlogon);
                    }
                    CloseHandle(procInteractiveWinlogon);
                }
                Wh_Log_Original(L"tknInteractive: %p\n", tknInteractive);
                if (tknInteractive && tknInteractive != INVALID_HANDLE_VALUE) {
                    STARTUPINFO si{};
                    si.cb = sizeof(si);
                    si.lpDesktop = (LPWSTR)L"WinSta0\\Default";
                    CreateProcessAsUserW(tknInteractive, nullptr, wszCommand, nullptr, nullptr, false, INHERIT_CALLER_PRIORITY,  nullptr, nullptr, &si, &pi);
                    CloseHandle(tknInteractive);
                    mut_pi.unlock();
                    WaitForSingleObject(pi.hProcess, INFINITE);
                    mut_pi.lock();
                    DWORD dwExitCode = -1;
                    GetExitCodeProcess(pi.hProcess, &dwExitCode);
                    Wh_Log_Original(L"Exited process with %d.\n", dwExitCode);
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    if (dwExitCode == 0xFFFFFFFF) {
                        mut_pi.unlock();
                        break;
                    }
                    Sleep(1000);
                } else {
                    mut_pi.unlock();
                    break;
                }
            }
        });
        return TRUE;
    }
    return FALSE;
}

void Wh_ModUninit() {
    mut_pi.lock();
    if (pi.hThread) {
        HANDLE hThread = CreateRemoteThread(pi.hProcess, nullptr, 0, &procClose, reinterpret_cast<LPVOID>(GetThreadId(pi.hThread)), 0, nullptr);
        if (hThread) {
            DWORD dwExitCode = 1;
            GetExitCodeProcess(hThread, &dwExitCode);
            Wh_Log_Original(L"Exited thread with %d.\n", dwExitCode);
            CloseHandle(hThread);
        }
    }
    mut_pi.unlock();
    if (worker.native_handle()) {
        worker.join();
        Wh_Log_Original(L"Ended worker.\n");
    }
    Wh_Log_Original(L"Uninit");
}

void Wh_ModSettingsChanged() {
    Wh_Log_Original(L"SettingsChanged");
}
