// ==WindhawkMod==
// @id              valinet-outlook-classic-hooks
// @name            Outlook (classic) hooks
// @description     Collection of quality of life improvements for Outlook (classic).
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @twitter         https://twitter.com/jack
// @homepage        https://valinet.ro
// @include         OUTLOOK.EXE
// @compilerOptions -lcomctl32 -lole32 -loleaut32 -loleacc -ldwmapi
// @license         GPLv3
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Outlook (classic) hooks
Collection of quality of life improvements for Outlook (classic).

In order to determine names of mail boxes, add some dummy one and then watch mod logs for names of the mailboxes in your setup.

Disable scheduled send/receive for monitored mailboxes in Outlook settings in order for IMAP IDLE to work.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- mbox_monitor:
  - ""
  $name: Mailboxes to monitor
  $description: List of mailboxes which Outlook should maintain a persistent IMAP connection with (IMAP IDLE) (requires restarting Outlook).
- hideOnClose: true
  $name: Hide on close
  $description: Hide main window instead of closing when clicking the Close button.
- toggleStateTray: true
  $name: Show/hide on tray icon click
  $description: Show or hide main window when clicking the tray icon.
- hideWhenStartedMinimized: true
  $name: Hide on startup when minimized
  $description: Hide main window at startup when app is set to launch minimized.
- quitInTray: true
  $name: Show Quit in tray menu
  $description: Display a Quit Outlook item in the tray (right click) menu.
*/
// ==/WindhawkModSettings==
struct {
    bool hideOnClose;
    bool toggleStateTray;
    bool hideWhenStartedMinimized;
    bool quitInTray;
} settings;

#include <windhawk_utils.h>
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <oleacc.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <netlistmgr.h>

HWND hOutlookWnd = NULL;
bool registered = false;
bool triggered = false;
bool shouldHide = false;
IDispatch* explorers[10]{};
HWND g_trayHwnd = nullptr;
HWND g_trayEnvelopeHwnd = nullptr;
ITaskbarList* pTaskbar = nullptr;
bool allowNextSwShow = false;
INetworkListManager* g_pNLM = nullptr;
IConnectionPoint* g_pCP = nullptr;
DWORD g_netEventsCookie = 0;

static const IID IID_IUnknown_ = {
    0x00000000, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

static const IID IID_INetworkListManagerEvents_ = {
    0xDCB00001, 0x570F, 0x4A9B,
    { 0x8D, 0x69, 0x19, 0x9F, 0xDB, 0xA5, 0x72, 0x3B }
};

class NetworkEvents : public INetworkListManagerEvents {
public:
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (memcmp(&riid, &IID_IUnknown_, sizeof(IID)) == 0 ||
            memcmp(&riid, &IID_INetworkListManagerEvents_, sizeof(IID)) == 0) {
            *ppv = this;
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE ConnectivityChanged(NLM_CONNECTIVITY connectivity) override {
        bool isOnline = (connectivity & NLM_CONNECTIVITY_IPV4_INTERNET) || 
                        (connectivity & NLM_CONNECTIVITY_IPV6_INTERNET);
        Wh_Log(L"Connectivity changed: %s", isOnline ? L"online" : L"offline");
        if (isOnline) {
            //PostMessageW(hOutlookWnd, WM_TIMER, 0x133A, 0);
            SetTimer(hOutlookWnd, 0x133B, 20000, NULL);
        } else {
            //PostMessageW(hOutlookWnd, WM_TIMER, 0x133C, 0);
            //KillTimer(hOutlookWnd, 0x133A);
            //KillTimer(hOutlookWnd, 0x133B);
        }
        return S_OK;
    }
} g_netEvents;

static const IID IID_INetworkListManager_ = {
    0xDCB00000, 0x570F, 0x4A9B,
    { 0x8D, 0x69, 0x19, 0x9F, 0xDB, 0xA5, 0x72, 0x3B }
};
static const CLSID CLSID_NetworkListManager_ = {
    0xDCB00C01, 0x570F, 0x4A9B,
    { 0x8D, 0x69, 0x19, 0x9F, 0xDB, 0xA5, 0x72, 0x3B }
};

static const IID IID_IConnectionPointContainer_ = {
    0xB196B284, 0xBAB4, 0x101A,
    { 0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1D, 0x07 }
};

void SetupNetworkNotifications() {
    if (g_pNLM)
        return;

    HRESULT hr = CoCreateInstance(CLSID_NetworkListManager_, nullptr, CLSCTX_INPROC_SERVER,
                     IID_INetworkListManager_, reinterpret_cast<void**>(&g_pNLM));
    if (!g_pNLM) { Wh_Log(L"Failed to create NetworkListManager: %x", hr); return; }

    VARIANT_BOOL isConnected = VARIANT_FALSE;
    g_pNLM->IsConnectedToInternet(&isConnected);
    Wh_Log(L"Currently connected: %d", isConnected);

    IConnectionPointContainer* pCPC = nullptr;
    g_pNLM->QueryInterface(IID_IConnectionPointContainer_,
                            reinterpret_cast<void**>(&pCPC));
    if (!pCPC) { Wh_Log(L"Failed to get CPC"); return; }

    pCPC->FindConnectionPoint(IID_INetworkListManagerEvents_,  &g_pCP);
    pCPC->Release();
    if (!g_pCP) { Wh_Log(L"Failed to get CP"); return; }

    hr = g_pCP->Advise(&g_netEvents, &g_netEventsCookie);
    Wh_Log(L"Network notifications set up, cookie=%d, hr=%x", g_netEventsCookie, hr);
}

const CLSID CLSID_OutlookApplication = {
    0x0006F03A, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

static const IID IID_ITaskbarList_ = {
    0x56FDF342, 0xFD6D, 0x11d0,
    { 0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90 }
};

static const CLSID CLSID_TaskbarList_ = {
    0x56FDF344, 0xFD6D, 0x11d0,
    { 0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90 }
};

HRESULT GetProperty(IDispatch* pDisp, LPCOLESTR name, VARIANT* pResult) {
    if (!pDisp) { Wh_Log(L"GetProperty: null pDisp for %s", name); return E_POINTER; }
    
    DISPID dispid;
    HRESULT hr = pDisp->GetIDsOfNames({}, const_cast<LPOLESTR*>(&name),
                                       1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { Wh_Log(L"GetProperty: GetIDsOfNames failed for %s: %08X", name, hr); return hr; }

    DISPPARAMS dp = {};
    VariantInit(pResult);
    hr = pDisp->Invoke(dispid, {}, LOCALE_USER_DEFAULT,
                       DISPATCH_PROPERTYGET, &dp, pResult, nullptr, nullptr);
    if (FAILED(hr)) { Wh_Log(L"GetProperty: Invoke failed for %s: %08X", name, hr); }
    return hr;
}

HRESULT CallMethod(IDispatch* pDisp, LPCOLESTR name,
                   VARIANT* args, int argCount, VARIANT* pResult) {
    if (!pDisp) { Wh_Log(L"CallMethod: null pDisp for %s", name); return E_POINTER; }

    DISPID dispid;
    HRESULT hr = pDisp->GetIDsOfNames({}, const_cast<LPOLESTR*>(&name),
                                       1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { Wh_Log(L"CallMethod: GetIDsOfNames failed for %s: %08X", name, hr); return hr; }

    DISPPARAMS dp = { args, nullptr, (UINT)argCount, 0 };
    if (pResult) VariantInit(pResult);
    hr = pDisp->Invoke(dispid, {}, LOCALE_USER_DEFAULT,
                       DISPATCH_METHOD, &dp, pResult, nullptr, nullptr);
    if (FAILED(hr)) { Wh_Log(L"CallMethod: Invoke failed for %s: %08X", name, hr); }
    return hr;
}

void WorkOnline(int& k) {
    IDispatch* pApp = nullptr;
    HRESULT hr = GetActiveObject(CLSID_OutlookApplication, nullptr,
                                 reinterpret_cast<IUnknown**>(&pApp));
    if (FAILED(hr) || !pApp) { Wh_Log(L"GetActiveObject failed: %08X", hr); return; }
    Wh_Log(L"Got Outlook Application");

    k = 2;

    VARIANT vSession = {};
    GetProperty(pApp, L"Session", &vSession);
    if (vSession.vt == VT_DISPATCH && vSession.pdispVal) {
        VARIANT vOffline = {};
        GetProperty(vSession.pdispVal, L"Offline", &vOffline);
        Wh_Log(L"Offline=%d", vOffline.boolVal);
        if (vOffline.boolVal)
            k = 1;
        vSession.pdispVal->Release();
    }

    // get ActiveExplorer
    VARIANT vExplorer = {};
    if (FAILED(GetProperty(pApp, L"ActiveExplorer", &vExplorer)) ||
        vExplorer.vt != VT_DISPATCH || !vExplorer.pdispVal) {
        Wh_Log(L"Failed to get ActiveExplorer");
        return;
    }
    IDispatch* pExplorer = vExplorer.pdispVal;

    // get CommandBars
    VARIANT vCB = {};
    if (FAILED(GetProperty(pExplorer, L"CommandBars", &vCB)) ||
        vCB.vt != VT_DISPATCH || !vCB.pdispVal) {
        Wh_Log(L"Failed to get CommandBars");
        pExplorer->Release();
        return;
    }
    IDispatch* pCB = vCB.pdispVal;

    VARIANT vArg = {};
    vArg.vt = VT_BSTR;
    vArg.bstrVal = SysAllocString(L"ToggleOnline");
    hr = CallMethod(pCB, L"ExecuteMso", &vArg, 1, nullptr);
    Wh_Log(L"ExecuteMso ToggleOnline hr=%08X", hr);
    SysFreeString(vArg.bstrVal);

    pCB->Release();
    pExplorer->Release();
    pApp->Release();
}

void TriggerSendAndReceive(VARIANT_BOOL visible = VARIANT_FALSE) {
    IDispatch* pApp = nullptr;
    HRESULT hr = GetActiveObject(CLSID_OutlookApplication, nullptr,
                                 reinterpret_cast<IUnknown**>(&pApp));
    if (FAILED(hr) || !pApp) { Wh_Log(L"GetActiveObject failed: %08X", hr); return; }
    Wh_Log(L"Got Outlook Application");

    VARIANT vSession = {};
    if (FAILED(GetProperty(pApp, L"Session", &vSession)) || 
        vSession.vt != VT_DISPATCH || !vSession.pdispVal) {
        Wh_Log(L"Failed to get Session");
        return;
    }
    IDispatch* pSession = vSession.pdispVal;

    VARIANT vArg = {};
    vArg.vt = VT_BOOL;
    vArg.boolVal = visible;

    hr = CallMethod(pSession, L"SendAndReceive", &vArg, 1, nullptr);
    Wh_Log(L"SendAndReceive hr=%08X", hr);

    pSession->Release();
    pApp->Release();
}

void OpenMailboxInNewWindow(int slot, LPCWSTR accountName) {
    Wh_Log(L"OpenMailboxInNewWindow: starting for %s", accountName);

    IDispatch* pApp = nullptr;
    HRESULT hr = GetActiveObject(CLSID_OutlookApplication, nullptr,
                                 reinterpret_cast<IUnknown**>(&pApp));
    if (FAILED(hr) || !pApp) { Wh_Log(L"GetActiveObject failed: %08X", hr); return; }
    Wh_Log(L"Got Outlook Application");

    VARIANT vSession = {};
    if (FAILED(GetProperty(pApp, L"Session", &vSession)) || !vSession.pdispVal)
        { Wh_Log(L"Failed to get Session"); pApp->Release(); return; }
    IDispatch* pSession = vSession.pdispVal;
    Wh_Log(L"Got Session");

    VARIANT vStores = {};
    if (FAILED(GetProperty(pSession, L"Stores", &vStores)) || !vStores.pdispVal)
        { Wh_Log(L"Failed to get Stores"); pSession->Release(); pApp->Release(); return; }
    IDispatch* pStores = vStores.pdispVal;
    Wh_Log(L"Got Stores");

    VARIANT vCount = {};
    if (FAILED(GetProperty(pStores, L"Count", &vCount)))
        { Wh_Log(L"Failed to get Count"); pStores->Release(); pSession->Release(); pApp->Release(); return; }
    long count = vCount.lVal;
    Wh_Log(L"Store count: %d", count);

    IDispatch* pTargetFolder = nullptr;

    for (long i = 1; i <= count; i++) {
        VARIANT vIndex = {};
        vIndex.vt = VT_I4;
        vIndex.lVal = i;

        VARIANT vStore = {};
        if (FAILED(CallMethod(pStores, L"Item", &vIndex, 1, &vStore)) || !vStore.pdispVal) {
            Wh_Log(L"Failed to get store %d", i);
            continue;
        }
        IDispatch* pStore = vStore.pdispVal;

        VARIANT vName = {};
        if (FAILED(GetProperty(pStore, L"DisplayName", &vName))) {
            Wh_Log(L"Failed to get DisplayName for store %d", i);
            pStore->Release();
            continue;
        }
        Wh_Log(L"Store %d: %s", i, vName.bstrVal);

        if (vName.bstrVal && wcscmp(vName.bstrVal, accountName) == 0) {
            VARIANT vFolderType = {};
            vFolderType.vt = VT_I4;
            vFolderType.lVal = 6; // olFolderInbox

            VARIANT vFolder = {};
            if (FAILED(CallMethod(pStore, L"GetDefaultFolder", &vFolderType, 1, &vFolder)) || !vFolder.pdispVal) {
                Wh_Log(L"Failed to get inbox folder");
            } else {
                pTargetFolder = vFolder.pdispVal;
                pTargetFolder->AddRef();
            }

            SysFreeString(vName.bstrVal);
            pStore->Release();
            break;
        }

        SysFreeString(vName.bstrVal);
        pStore->Release();
    }

    pStores->Release();
    pSession->Release();

    if (!pTargetFolder) {
        Wh_Log(L"Target folder not found");
        pApp->Release();
        return;
    }
    Wh_Log(L"Got target folder");

    VARIANT vExplorers = {};
    if (FAILED(GetProperty(pApp, L"Explorers", &vExplorers)) || !vExplorers.pdispVal) {
        Wh_Log(L"Failed to get Explorers");
        pTargetFolder->Release();
        pApp->Release();
        return;
    }
    IDispatch* pExplorers = vExplorers.pdispVal;

    VARIANT args[2] = {};
    args[0].vt = VT_I4;       args[0].lVal = 1;
    args[1].vt = VT_DISPATCH; args[1].pdispVal = pTargetFolder;

    VARIANT vExplorer = {};
    if (FAILED(CallMethod(pExplorers, L"Add", args, 2, &vExplorer)) || !vExplorer.pdispVal) {
        Wh_Log(L"Failed to create explorer");
        pExplorers->Release();
        pTargetFolder->Release();
        pApp->Release();
        return;
    }
    IDispatch* pExplorer = vExplorer.pdispVal;
    Wh_Log(L"Created explorer");

    //CallMethod(pExplorer, L"Display", nullptr, 0, nullptr);
    //Wh_Log(L"Called Display");

    //ShowWindow(FindWindowW(L"rctrl_renwnd32", NULL), SW_HIDE);

    //ShowWindow(GetExplorerHwnd(pExplorer), SW_HIDE);

    explorers[slot] = pExplorer; //pExplorer->Release();
    pExplorers->Release();
    pTargetFolder->Release();
    pApp->Release();
}

LRESULT CALLBACK TraySubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD_PTR dwRefData) {
    if (uMsg == WM_NCDESTROY) {
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd, TraySubclassProc);
    } else if (settings.toggleStateTray && 
               ((hWnd == g_trayHwnd && uMsg == 1026 && wParam == 0x3039 && lParam == 0x202) ||
                (hWnd == g_trayEnvelopeHwnd && uMsg == 1068 && wParam == 0 && lParam == 0x202))) {
        //Wh_Log(L"tray callback: uMsg=0x%04X wp=0x%llX lp=0x%llX", uMsg, wParam, lParam);
        if (IsWindowVisible(hOutlookWnd))
            ShowWindow(hOutlookWnd, SW_HIDE);
        else
            PostMessageW(hOutlookWnd, WM_TIMER, 0x1339, 0);
        return 0;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

using Shell_NotifyIconW_t = decltype(&Shell_NotifyIconW);
Shell_NotifyIconW_t Shell_NotifyIconW_orig;
BOOL WINAPI Shell_NotifyIconW_hook(DWORD dwMessage, NOTIFYICONDATAW* lpData) {
    if (!g_trayHwnd && lpData->uID == 12345 && (dwMessage == NIM_ADD || dwMessage == NIM_MODIFY) && 
        lpData->hWnd && WindhawkUtils::SetWindowSubclassFromAnyThread(lpData->hWnd, TraySubclassProc, 0)) {
        g_trayHwnd = lpData->hWnd;    
    } else if (!g_trayEnvelopeHwnd && lpData->uID == 0 && (dwMessage == NIM_ADD || dwMessage == NIM_MODIFY) && 
        lpData->hWnd && WindhawkUtils::SetWindowSubclassFromAnyThread(lpData->hWnd, TraySubclassProc, 0)) {
        g_trayEnvelopeHwnd = lpData->hWnd;    
    }
    Wh_Log(L"Tray icon: hWnd=%p uID=%d uCallbackMessage=%d", lpData->hWnd, lpData->uID, lpData->uCallbackMessage);
    return Shell_NotifyIconW_orig(dwMessage, lpData);
}

using TrackPopupMenuEx_t = decltype(&TrackPopupMenuEx);
TrackPopupMenuEx_t TrackPopupMenuEx_orig;
BOOL WINAPI TrackPopupMenuEx_hook(HMENU hMenu, UINT uFlags, int x, int y,
                                   HWND hWnd, LPTPMPARAMS lptpm) {
    if (hWnd != g_trayHwnd && hWnd != g_trayEnvelopeHwnd)
        return TrackPopupMenuEx_orig(hMenu, uFlags, x, y, hWnd, lptpm);

    if (settings.quitInTray) {
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 0x1337, L"&Quit Outlook");
    }

    UINT origFlags = uFlags;
    BOOL result = TrackPopupMenuEx_orig(hMenu, uFlags | TPM_RETURNCMD | TPM_NONOTIFY, x, y, hWnd, lptpm);

    Wh_Log(L"TrackPopupMenuEx result: %d", result);

    if (result == 0x1337) {
        PostMessageW(hOutlookWnd, WM_USER + 0x47, 0, 0x413);
    } else if (result == 65100) {
        PostMessageW(hOutlookWnd, WM_TIMER, 0x1339, 0);
    } else if (result != 0) {
        if (origFlags & TPM_RETURNCMD) {
            return result;
        } else {
            PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(result, 0), 0);
        }
    }
    return (origFlags & TPM_RETURNCMD) ? result : TRUE;
}

using TrackPopupMenu_t = decltype(&TrackPopupMenu);
TrackPopupMenu_t TrackPopupMenu_orig;
BOOL WINAPI TrackPopupMenu_hook(HMENU hMenu, UINT uFlags, int x, int y,
                                 int nReserved, HWND hWnd, const RECT* prcRect) {
    if (hWnd != g_trayEnvelopeHwnd)
        return TrackPopupMenu_orig(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);

    TPMPARAMS tpm = { sizeof(tpm) };
    if (prcRect) tpm.rcExclude = *prcRect;
    return TrackPopupMenuEx_hook(hMenu, uFlags, x, y, hWnd, prcRect ? &tpm : nullptr);
}

void CloakTaskbarWindow(HWND hWnd) {
    BOOL cloak = TRUE;
    DwmSetWindowAttribute(hWnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
    if (!pTaskbar)
        CoCreateInstance(CLSID_TaskbarList_, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList_, reinterpret_cast<void**>(&pTaskbar));
    if (pTaskbar) {
        pTaskbar->HrInit();
        pTaskbar->DeleteTab(hWnd);
    }
}

void UncloakTaskbarWindow(HWND hWnd) {
    BOOL cloak = FALSE;
    DwmSetWindowAttribute(hWnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
    if (!pTaskbar)
        CoCreateInstance(CLSID_TaskbarList_, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList_, reinterpret_cast<void**>(&pTaskbar));
    if (pTaskbar) {
        pTaskbar->HrInit();
        pTaskbar->AddTab(hWnd);
    }
}

LRESULT CALLBACK OutlookSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD_PTR dwRefData) {
    //Wh_Log(L"MSG: 0x%04X wp=0x%llX lp=0x%llX", uMsg, wParam, lParam);
    if (uMsg == WM_NCDESTROY) {
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd, OutlookSubclassProc);
    //} else if (uMsg == WM_WINDOWPOSCHANGED && !(((WINDOWPOS*)lParam)->flags & SWP_HIDEWINDOW)) {
        //ShowWindow(hWnd, SW_SHOW);
    } else if (uMsg == WM_WINDOWPOSCHANGING) {
        WINDOWPOS* wp = (WINDOWPOS*)lParam;
        if (wp->flags & SWP_SHOWWINDOW) {
            if (allowNextSwShow)
                allowNextSwShow = false;
            else
                wp->flags &= ~SWP_SHOWWINDOW;
        }
    } else if ((uMsg == WM_SYSCOMMAND && wParam  == SC_CLOSE && (InSendMessageEx(nullptr) & ISMEX_SEND)) || (uMsg == WM_USER + 0x47 && lParam == 0x413)) {
        if (uMsg == WM_SYSCOMMAND) {
            allowNextSwShow = true;
            ShowWindow(hWnd, SW_SHOW);
        }
        for (IDispatch* pDisp : explorers) {
            if (!pDisp)
                break;
            CallMethod(pDisp, L"Close", nullptr, 0, nullptr);
            pDisp->Release();
            pDisp = nullptr;
        }
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd, OutlookSubclassProc);
    } else if (uMsg == WM_SYSCOMMAND && wParam == SC_CLOSE) {
        if (settings.hideOnClose)
            ShowWindow(hWnd, SW_HIDE);
        else {
            PCWSTR str = Wh_GetStringSetting(L"mbox_monitor[0]");
            if (*str)
                PostMessageW(hOutlookWnd, WM_USER + 0x47, 0, 0x413);
            Wh_FreeStringSetting(str);
        }
        return 0;
    } else if (uMsg == WM_DWMNCRENDERINGCHANGED && !triggered) {
        triggered = true;
        ShowWindow(hWnd, SW_SHOW);
        SetTimer(hWnd, 0x1338, 2000, 0);
    } else if (uMsg == WM_TIMER && wParam == 0x1337) {
        KillTimer(hWnd, 0x1337);
        for (unsigned i = 0; i < sizeof(explorers) / sizeof(explorers[0]); ++i) {
            PCWSTR str = Wh_GetStringSetting(L"mbox_monitor[%d]", i);
            if (!*str) {
                Wh_FreeStringSetting(str);
                break;
            }
            OpenMailboxInNewWindow(i, str);
            Wh_FreeStringSetting(str);
        }
        if (shouldHide)
            PostMessageW(hOutlookWnd, WM_TIMER, 0x133B, 0);
    } else if (uMsg == WM_TIMER && wParam == 0x1338) {
        KillTimer(hWnd, 0x1338);
        PCWSTR str = Wh_GetStringSetting(L"mbox_monitor[0]");
        if (*str)
            ShowWindow(hWnd, SW_HIDE);
        Wh_FreeStringSetting(str);
        if (shouldHide)
            UncloakTaskbarWindow(hWnd);
        else {
            allowNextSwShow = true;
            ShowWindow(hWnd, SW_SHOW);
        }
    } else if (uMsg == WM_TIMER && wParam == 0x1339) {
        KillTimer(hWnd, 0x1339);
        allowNextSwShow = true;
        ShowWindow(hWnd, SW_SHOW);
        if (IsIconic(hWnd))
            ShowWindow(hWnd, SW_RESTORE);
        SetForegroundWindow(hWnd);
    } else if (uMsg == WM_TIMER && wParam == 0x133A) {
        KillTimer(hWnd, 0x133A);
        int k = 0;
        WorkOnline(k);
        //if (k == 2)
        //    SetTimer(hWnd, 0x133C, 1000, NULL);
    } else if (uMsg == WM_TIMER && wParam == 0x133C) {
        KillTimer(hWnd, 0x133C);
        int k = 0;
        WorkOnline(k);
    } else if (uMsg == WM_TIMER && wParam == 0x133B) {
        KillTimer(hWnd, 0x133B);
        TriggerSendAndReceive();
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

using CreateWindowExWHook_t = decltype(&CreateWindowExW);
CreateWindowExWHook_t CreateWindowExW_orig;
HWND CreateWindowExW_hook(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    HWND hWnd = CreateWindowExW_orig(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (!hOutlookWnd && HIWORD((ULONG_PTR)lpClassName) && !wcscmp(lpClassName, L"rctrl_renwnd32") && !hWndParent && WindhawkUtils::SetWindowSubclassFromAnyThread(hWnd, OutlookSubclassProc, 0)) {
        hOutlookWnd = hWnd;
        Wh_Log(L">> OutlookWnd %p", hOutlookWnd);
        SetPropW(hOutlookWnd, L"OMW", hOutlookWnd);
        if (shouldHide)
            CloakTaskbarWindow(hOutlookWnd);
        SetupNetworkNotifications();
    }
    if (HIWORD((ULONG_PTR)lpClassName) && !wcscmp(lpClassName, L"MsoSplash") && shouldHide) {
        CloakTaskbarWindow(hWnd);
    }
    return hWnd;
}

using RegisterActiveObject_t = decltype(&RegisterActiveObject);
RegisterActiveObject_t RegisterActiveObject_orig;
HRESULT WINAPI RegisterActiveObject_hook(IUnknown* punk, REFCLSID rclsid,
                                          DWORD dwFlags, DWORD* pdwRegister) {
    HRESULT hr = RegisterActiveObject_orig(punk, rclsid, dwFlags, pdwRegister);
    if (SUCCEEDED(hr) && rclsid == CLSID_OutlookApplication && !registered) {
        registered = true;
        Wh_Log(L"Outlook called RegisterActiveObject");
        PCWSTR str = Wh_GetStringSetting(L"mbox_monitor[0]");
        if (*str)
            SetTimer(hOutlookWnd, 0x1337, 0, 0);
        Wh_FreeStringSetting(str);
    }
    return hr;
}

BOOL CALLBACK OMWFind(HWND hWnd, LPARAM lParam) {
    if (GetClassWord(hWnd, GCW_ATOM) == lParam && GetPropW(hWnd, L"OMW")) {
        PostMessageW(hWnd, WM_TIMER, 0x1339, 0);
        return FALSE;
    }
    return TRUE;
}

void LoadSettings() {
    settings.hideOnClose = Wh_GetIntSetting(L"hideOnClose");
    settings.toggleStateTray = Wh_GetIntSetting(L"toggleStateTray");
    settings.hideWhenStartedMinimized = Wh_GetIntSetting(L"hideWhenStartedMinimized");
    settings.quitInTray = Wh_GetIntSetting(L"quitInTray");
}

BOOL Wh_ModInit() {
    Wh_Log(L"Init");
    LoadSettings();

    HWND hWnd = FindWindowW(L"rctrl_renwnd32", NULL);
    if (hWnd) {
        DWORD dwPid = 0;
        GetWindowThreadProcessId(hWnd, &dwPid);
        if (dwPid == GetCurrentProcessId()) {
            MessageBoxW(hWnd, L"Restart Outlook to properly initialize mod.", L"Outlook Hooks", MB_ICONINFORMATION);
            return TRUE;
        } else {
            EnumWindows(OMWFind, RegisterWindowMessageW(L"rctrl_renwnd32"));
            ExitProcess(0);
        }
    }
    else {
        Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_hook, (void**)&CreateWindowExW_orig);
        Wh_SetFunctionHook((void*)RegisterActiveObject, (void*)RegisterActiveObject_hook, (void**)&RegisterActiveObject_orig);
    }
    Wh_SetFunctionHook((void*)Shell_NotifyIconW, (void*)Shell_NotifyIconW_hook, (void**)&Shell_NotifyIconW_orig);
    Wh_SetFunctionHook((void*)TrackPopupMenuEx, (void*)TrackPopupMenuEx_hook, (void**)&TrackPopupMenuEx_orig);
    Wh_SetFunctionHook((void*)TrackPopupMenu, (void*)TrackPopupMenu_hook, (void**)&TrackPopupMenu_orig);

    if (settings.hideWhenStartedMinimized) {
        STARTUPINFOW si = { sizeof(si) };
        GetStartupInfoW(&si);

        if (si.dwFlags & STARTF_USESHOWWINDOW) {
            Wh_Log(L"nShowCmd=%d", si.wShowWindow);
            if (si.wShowWindow == SW_SHOWMINIMIZED || 
                si.wShowWindow == SW_MINIMIZE ||
                si.wShowWindow == SW_SHOWMINNOACTIVE) {
                shouldHide = true;
            }
        }
    }

    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Uninit");

    if (TrackPopupMenu_orig)
        Wh_RemoveFunctionHook((void*)TrackPopupMenu);
    if (TrackPopupMenuEx_orig)
        Wh_RemoveFunctionHook((void*)TrackPopupMenuEx);
    if (Shell_NotifyIconW_orig)
        Wh_RemoveFunctionHook((void*)Shell_NotifyIconW);
    if (RegisterActiveObject_orig)
        Wh_RemoveFunctionHook((void*)RegisterActiveObject);
    if (CreateWindowExW_orig)
        Wh_RemoveFunctionHook((void*)CreateWindowExW);

    if (hOutlookWnd)
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(hOutlookWnd, OutlookSubclassProc);
    if (g_trayHwnd)
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(g_trayHwnd, TraySubclassProc);
    if (g_trayEnvelopeHwnd)
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(g_trayHwnd, TraySubclassProc);
    
    for (IDispatch* pDisp : explorers) {
        if (!pDisp)
            break;
        pDisp->Release();
        pDisp = nullptr;
    }

    if (pTaskbar)
        pTaskbar->Release();

    if (g_pCP && g_netEventsCookie)
        g_pCP->Unadvise(g_netEventsCookie);
    if (g_pCP)
        g_pCP->Release();
    if (g_pNLM)
        g_pNLM->Release();
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"SettingsChanged");

    LoadSettings();
}
