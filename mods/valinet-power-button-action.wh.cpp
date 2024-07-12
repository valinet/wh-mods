// ==WindhawkMod==
// @id              valinet-power-button-action
// @name            Power Button Action
// @description     Performs custom actions when chassis power button is pressed
// @version         1.0
// @author          valinet
// @github          https://github.com/valinet
// @include         windhawk.exe
// @compilerOptions -lWtsapi32 -lShlwapi -lOle32 -lOleAut32 -lShell32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Power Button Action
Performs custom actions when chassis power button is pressed
*/
// ==/WindhawkModReadme==

#include <initguid.h>
#include <debugapi.h>
#include <handleapi.h>
#include <libloaderapi.h>
#include <oleauto.h>
#include <processthreadsapi.h>
#include <thread>
#include <psapi.h>
#include <synchapi.h>
#include <sysinfoapi.h>
#include <tlhelp32.h>
#include <hidsdi.h>
#include <winerror.h>
#include <wtsapi32.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <shlobj.h>

#define Wh_Log_External
#define Wh_Log_Original
//#define Wh_Log_External OutputDebugString
//#define Wh_Log_Original Wh_Log

#define QWORD INT64

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)

DEFINE_GUID(This_CLSID_ShellWindows, 0x9ba05972, 0xf6a8, 0x11cf, 0xa4,0x42, 0x00,0xa0,0xc9,0x0a,0x8f,0x39);

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
    HANDLE mutSingleInstance = CreateMutexW(&sa, false, L"Global\\{FF8295EF-01FE-40BF-A319-57BC3EDD1FB6}");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutSingleInstance);
        return ERROR_ALREADY_EXISTS;
    }

    MSG msg;
    PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE);
    CoInitialize(0);
    wchar_t wszMsg[MAX_PATH * 4];

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
        return rv;
    }

    HANDLE hPowerButtonPressEvent = CreateEventW(nullptr, false, false, L"Global\\{29DFB10F-5CFF-4A9D-B9D1-31B053A1AE95}");
    while (hPowerButtonPressEvent) {
        bool isQuiting = false;
        rv = MsgWaitForMultipleObjects(1, &hPowerButtonPressEvent, false, INFINITE, QS_ALLINPUT);
        if (rv != WAIT_OBJECT_0 && (rv != WAIT_OBJECT_0 + 1)) break;
        if (rv == WAIT_OBJECT_0) {
            IShellWindows *psw = nullptr;
            HRESULT hr = CoCreateInstance(This_CLSID_ShellWindows, NULL, CLSCTX_ALL, IID_PPV_ARGS(&psw));
            if (SUCCEEDED(hr)) {
                HWND hwnd = nullptr;
                IDispatch* pdisp = nullptr;
                VARIANT vEmpty = {}; // VT_EMPTY
                if (S_OK == psw->FindWindowSW(&vEmpty, &vEmpty, SWC_DESKTOP, (long*)&hwnd, SWFO_NEEDDISPATCH, &pdisp)) {
                    IShellBrowser *psb = nullptr;
                    hr = IUnknown_QueryService(pdisp, SID_STopLevelBrowser, IID_PPV_ARGS(&psb));
                    if (SUCCEEDED(hr)) {
                        IShellView *psv = nullptr;
                        hr = psb->QueryActiveShellView(&psv);
                        IDispatch *pdispBackground;
                        HRESULT hr = psv->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&pdispBackground));
                        if (SUCCEEDED(hr)) {
                            IShellFolderViewDual *psfvd = nullptr;
                            hr = pdispBackground->QueryInterface(IID_PPV_ARGS(&psfvd));
                            if (SUCCEEDED(hr)) {
                                IDispatch *pdisp = nullptr;
                                hr = psfvd->get_Application(&pdisp);
                                if (SUCCEEDED(hr)) {
                                    IShellDispatch2 *psd = nullptr;
                                    hr = pdisp->QueryInterface(IID_PPV_ARGS(&psd));
                                    if (SUCCEEDED(hr)) {
                                        //PVOID OldValue;
                                        //BOOL amI32Bit = FALSE;
                                        //if (!IsWow64Process(GetCurrentProcess(), &amI32Bit) || amI32Bit) Wow64DisableWow64FsRedirection(&OldValue);
                                        wchar_t wszPath[MAX_PATH];
                                        hr = SHGetFolderPathW(nullptr, CSIDL_SYSTEM, nullptr, SHGFP_TYPE_CURRENT, wszPath);
                                        if (SUCCEEDED(hr)) {
                                            wcscat_s(wszPath, L"\\calc.exe");
                                            BSTR bstrExe = nullptr;
                                            bstrExe = SysAllocString(wszPath);
                                            BSTR bstrOperation = nullptr;
                                            bstrOperation = SysAllocString(L"open");
                                            if (bstrExe && bstrOperation) {
                                                VARIANT varOperation;
                                                varOperation.vt = VT_BSTR;
                                                varOperation.bstrVal = bstrOperation;                                            
                                                hr = psd->ShellExecuteW(bstrExe, vEmpty, vEmpty, varOperation, vEmpty);
                                                if (SUCCEEDED(hr)) {
                                                    auto start = GetTickCount64();
                                                    HWND hWnd = nullptr;
                                                    while (GetClassWord(hWnd = GetForegroundWindow(), GCW_ATOM) != RegisterWindowMessageW(L"CalcFrame")) {
                                                        SleepEx(0, true);
                                                        if (GetTickCount64() - start > 1000) break;
                                                    }
                                                    POINT pt{};
                                                    GetCursorPos(&pt);
                                                    RECT rc{};
                                                    GetWindowRect(hWnd, &rc);
                                                    SetWindowPos(hWnd, nullptr, pt.x - ((rc.right - rc.left) / 2), pt.y - ((rc.bottom - rc.top) / 2), 0, 0, SWP_NOSIZE);
                                                }
                                            }
                                            if (bstrExe) SysFreeString(bstrExe);
                                            if (bstrOperation) SysFreeString(bstrOperation);
                                        }
                                        //if (!IsWow64Process(GetCurrentProcess(), &amI32Bit) || amI32Bit) Wow64RevertWow64FsRedirection(OldValue);
                                        psd->Release();                                    
                                    }
                                    pdisp->Release();
                                }
                                psfvd->Release();
                            }
                            pdispBackground->Release();
                        }
                        psb->Release();
                    }
                    pdisp->Release();
                }
                psw->Release();
            }
            swprintf_s(wszMsg, L"Power button pressed\n");
            Wh_Log_External(wszMsg);
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

    if (hPowerButtonPressEvent) CloseHandle(hPowerButtonPressEvent);
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
    bool serviceProcess = false;

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (!argv) {
        Wh_Log_Original(L"CommandLineToArgvW failed");
        return FALSE;
    }

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-service") == 0) {
            serviceProcess = true;
            break;
        }
    }

    LocalFree(argv);

    Wh_Log_Original(L"Init %d\n", serviceProcess);
    if (!serviceProcess) {
        return FALSE;
    }

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
