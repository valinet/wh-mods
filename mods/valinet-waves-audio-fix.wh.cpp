// ==WindhawkMod==
// @id              valinet-waves-audio-fix
// @name            Waves Audio Fix
// @description     Fix crash of audio service due to Waves Audio under certain setups, like running Windows Server OS.
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @homepage        https://valinet.ro
// @include         audiodg.exe
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Waves Audio Fix
Fix crash of audio service due to Waves Audio under certain setups, like running Windows Server OS.
*/
// ==/WindhawkModReadme==

#include <excpt.h>
#include <memoryapi.h>
#include <winnt.h>

PVOID pHandler = nullptr;

bool patch(char* pAddr, const wchar_t* wszModule, int pEquals) {
    auto pWaves = GetModuleHandleW(wszModule);
    if (pWaves) {
        if (pAddr - reinterpret_cast<char*>(pWaves) == pEquals) {
            DWORD dwOldProtect = PAGE_EXECUTE_READWRITE;
            if (VirtualProtect(pAddr, 6, dwOldProtect, &dwOldProtect)) {
                pAddr[0] = 0x48; pAddr[1] = 0x31; pAddr[2] = 0xc0; // xor rax, rax
                pAddr[3] = 0x48; pAddr[4] = 0xff; pAddr[5] = 0xc8; // dec rax
                VirtualProtect(pAddr, 6, dwOldProtect, &dwOldProtect);
                return true;
            }
        }
    }
    return false;
}

LONG NTAPI OnVex(PEXCEPTION_POINTERS ExceptionInfo) {
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION && (
        patch(reinterpret_cast<char*>(ExceptionInfo->ExceptionRecord->ExceptionAddress), L"MaxxAudioRender64.dll", 0x15c787) ||
        patch(reinterpret_cast<char*>(ExceptionInfo->ExceptionRecord->ExceptionAddress), L"MaxxAudioCapture64.dll", 0x11618a))) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// The mod is being initialized, load settings, hook functions, and do other
// initialization stuff if required.
BOOL Wh_ModInit() {
    pHandler = AddVectoredExceptionHandler(true, OnVex);
    Wh_Log(L"Init: %p\n", pHandler);
    return TRUE;
}

// The mod is being unloaded, free all allocated resources.
void Wh_ModUninit() {
    if (pHandler) RemoveVectoredExceptionHandler(pHandler);
    Wh_Log(L"Uninit");
}

// The mod setting were changed, reload them.
void Wh_ModSettingsChanged() {
    Wh_Log(L"SettingsChanged");
}
