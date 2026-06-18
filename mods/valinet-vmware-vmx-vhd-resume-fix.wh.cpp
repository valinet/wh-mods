// ==WindhawkMod==
// @id              valinet-vmware-vmx-vhd-resume-fix
// @name            VMware: VHD Resume Fix
// @description     VMware: VHD Resume Fix
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @homepage        https://valinet.ro
// @include         vmware-vmx.exe
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
*/
// ==/WindhawkModReadme==

// same effect, but vmware-vmx segfaults on VM reboot
// scsi0.sharedBus = "virtual"
// disk.locking = "FALSE"

#include <windhawk_utils.h>

PBYTE match = nullptr;

inline BOOL _MaskCompareByteLevel(PVOID pvSearch, LPCSTR pszPattern, LPCSTR pszMask) {
    for (PBYTE value = (PBYTE)pvSearch; *pszMask; ++pszPattern, ++pszMask, ++value)
        if (*pszMask == 'x' && *(LPCBYTE)pszPattern != *value)
            return FALSE;
    return TRUE;
}

inline PVOID _FindPatternHelper_1_(PVOID pvSearch, size_t cbSearch, LPCSTR pszPattern, LPCSTR pszMask) {
    PBYTE pBegin = (PBYTE)pvSearch;
    PBYTE pEnd = pBegin + cbSearch;
    for (PBYTE pIt = pBegin; pIt <= pEnd; pIt += 1)
        if (_MaskCompareByteLevel(pIt, pszPattern, pszMask))
            return pIt;
    return NULL;
}

inline PVOID FindPattern(PVOID pvSearch, size_t cbSearch, LPCSTR pszPattern, LPCSTR pszMask) {
    cbSearch -= strlen(pszMask);
    return _FindPatternHelper_1_(pvSearch, cbSearch, pszPattern, pszMask);
}

inline BOOL SectionBeginAndSizeEx64(const IMAGE_DOS_HEADER* dosHeader, const IMAGE_NT_HEADERS64* ntHeader, const char* pszSectionName, PBYTE* beginSection, DWORD* sizeSection) {
    *beginSection = NULL;
    *sizeSection = 0;

    PIMAGE_SECTION_HEADER firstSection = IMAGE_FIRST_SECTION(ntHeader);
    for (unsigned int i = 0; i < ntHeader->FileHeader.NumberOfSections; ++i) {
        PIMAGE_SECTION_HEADER section = firstSection + i;
        if (!strncmp((const char*)section->Name, pszSectionName, IMAGE_SIZEOF_SHORT_NAME)) {
            *beginSection = (PBYTE)dosHeader + section->VirtualAddress;
            *sizeSection = section->Misc.VirtualSize;
            return TRUE;
        }
    }
    return FALSE;
}

inline BOOL SectionBeginAndSize(HMODULE hModule, const char* pszSectionName, PBYTE* beginSection, DWORD* sizeSection) {
    *beginSection = NULL;
    *sizeSection = 0;

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
        PIMAGE_NT_HEADERS64 ntHeader = (PIMAGE_NT_HEADERS64)((BYTE*)dosHeader + dosHeader->e_lfanew);
        if (ntHeader->Signature == IMAGE_NT_SIGNATURE && ntHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            return SectionBeginAndSizeEx64(dosHeader, ntHeader, pszSectionName, beginSection, sizeSection);
    }

    return FALSE;
}

BOOL Wh_ModInit() {
    PBYTE pThis;
    DWORD cbThis;
    if (!SectionBeginAndSize(GetModuleHandleW(nullptr), ".text", &pThis, &cbThis))
        return FALSE;
    match = (PBYTE)FindPattern(pThis, cbThis, "\xC3\x83\x7D\x00\x04", "xxxxx");
    if (!match)
        return FALSE;
    DWORD flOldProtect = 0;
    if (VirtualProtect(match, 1, PAGE_EXECUTE_READWRITE, &flOldProtect)) {
        match[4]++;
        VirtualProtect(match, 1, flOldProtect, &flOldProtect);
    }
    return TRUE;
}

void Wh_ModUninit() {
    DWORD flOldProtect = 0;
    if (VirtualProtect(match, 1, PAGE_EXECUTE_READWRITE, &flOldProtect)) {
        match[4]--;
        VirtualProtect(match, 1, flOldProtect, &flOldProtect);
    }
}
