// ==WindhawkMod==
// @id              valinet-explorer-hide-quick-access-pins
// @name            Explorer: Hide Quick Access pins
// @description     Explorer: Hide Quick Access pins
// @version         1.5.7
// @author          valinet
// @github          https://github.com/valinet
// @include         *
// @compilerOptions -loleaut32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*...*/
// ==/WindhawkModReadme==

#include <libloaderapi.h>
#include <windhawk_api.h>
#include <windhawk_utils.h>
#include <initguid.h>
#include <propvarutil.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <vssym32.h>
#include <vector>

std::vector<void*> hookedFunctions;
HMODULE hExplorerFrame = nullptr;

#ifdef _WIN64
#   define THISCALL  __cdecl
#   define STHISCALL L"__cdecl"

#   define STDCALL  __cdecl
#   define SSTDCALL L"__cdecl"
#else
#   define THISCALL  __thiscall
#   define STHISCALL L"__thiscall"

#   define STDCALL  __stdcall
#   define SSTDCALL L"__stdcall"
#endif

#define SHARED_SECTION __attribute__((section(".shared")))
asm(".section .shared,\"dws\"\n");

/* Remove pins from Quick access */
void *CNscTree_SetStateImageList_addr SHARED_SECTION = nullptr;
HRESULT (STDCALL *CNscTree_SetStateImageList_orig)(void *, HIMAGELIST) = nullptr;
HRESULT STDCALL CNscTree_SetStateImageList_hook(
    void       *pThis,
    HIMAGELIST  himl
)
{
    return S_OK;
}

struct CMWF_SYMBOL_HOOK {
    std::vector<std::wstring> symbols;
    void** pOriginalFunction;
    void* hookFunction = nullptr;
    bool optional = false;
    void **pSharedMemoryCache = nullptr;

    template <typename Prototype>
    CMWF_SYMBOL_HOOK(
            std::vector<std::wstring> symbols,
            Prototype **originalFunction,
            std::type_identity_t<Prototype *> hookFunction = nullptr,
            bool optional = false,
            void *pSharedMemoryCache = nullptr
    ) : symbols(std::move(symbols)),
        pOriginalFunction((void **)originalFunction),
        hookFunction((void *)hookFunction),
        optional(optional),
        pSharedMemoryCache((void **)pSharedMemoryCache) {}

    CMWF_SYMBOL_HOOK() = default;
};

bool CmwfHookSymbols(
        HMODULE module,
        const CMWF_SYMBOL_HOOK *symbolHooks,
        size_t symbolHooksCount
)
{
    int anyUncachedHooks = 0;
    void*** proxyAddresses = reinterpret_cast<void***>(calloc(symbolHooksCount, sizeof(void**)));
    if (!proxyAddresses) {
        return false;
    }
    WindhawkUtils::SYMBOL_HOOK* proxyHooks = new (std::nothrow) WindhawkUtils::SYMBOL_HOOK[symbolHooksCount];
    if (!proxyHooks) {
        free(proxyAddresses);
        return false;
    }

    for (size_t i = 0; i < symbolHooksCount; i++) {
        void *address = nullptr;
        if (symbolHooks[i].pSharedMemoryCache && *(symbolHooks[i].pSharedMemoryCache) != nullptr) {
            address = *(symbolHooks[i].pSharedMemoryCache);
            if (address == nullptr) {
                continue;
            }
            Wh_SetFunctionHook(
                address,
                symbolHooks[i].hookFunction,
                symbolHooks[i].pOriginalFunction
            );
        }
        else
        {
            address = nullptr;
            anyUncachedHooks++;

            proxyHooks[i] = {
                { symbolHooks[i].symbols[0] },
                &proxyAddresses[i],
                nullptr,
                symbolHooks[i].optional
            };
        }
    }

    if (anyUncachedHooks)
    {
        if (!WindhawkUtils::HookSymbols(module, proxyHooks, symbolHooksCount)) {
            delete[] proxyHooks;
            free(proxyAddresses);
            return false;
        }

        for (auto curProxyHook = 0; curProxyHook < symbolHooksCount; ++curProxyHook) {
            if (proxyAddresses[curProxyHook] == nullptr) {
                continue;
            }

            if (
                symbolHooks[curProxyHook].pSharedMemoryCache && 
                *(symbolHooks[curProxyHook].pSharedMemoryCache) == nullptr
            ) {
                *(symbolHooks[curProxyHook].pSharedMemoryCache) = proxyAddresses[curProxyHook];
            }

            if (symbolHooks[curProxyHook].hookFunction && symbolHooks[curProxyHook].pOriginalFunction) {
                if (Wh_SetFunctionHook(
                    proxyAddresses[curProxyHook],
                    symbolHooks[curProxyHook].hookFunction,
                    symbolHooks[curProxyHook].pOriginalFunction
                )) {
                    hookedFunctions.push_back(proxyAddresses[curProxyHook]);
                } else {
                    for (auto i = 0; i < curProxyHook; ++i) {
                        if (proxyAddresses[i] == nullptr) {
                            continue;
                        }
                        Wh_RemoveFunctionHook(proxyAddresses[i]);
                    }
                    hookedFunctions.clear();
                    delete[] proxyHooks;
                    free(proxyAddresses);
                    return false;
                }
            }
        }
    }

    delete[] proxyHooks;
    free(proxyAddresses);
    return true;
}

BOOL Wh_ModInit(void)
{
    if (!hExplorerFrame) {
        hExplorerFrame = LoadLibraryW(L"ExplorerFrame.dll");
    }
    if (!hExplorerFrame) {
        Wh_Log(L"Failed to load ExplorerFrame.dll");
        return FALSE;
    }

    const CMWF_SYMBOL_HOOK efHooks[] = {
        {
            {
                L"public: virtual long "
                SSTDCALL
                L" CNscTree::SetStateImageList(struct _IMAGELIST *)"
            },
            &CNscTree_SetStateImageList_orig,
            CNscTree_SetStateImageList_hook,
            false,
            &CNscTree_SetStateImageList_addr
        },
    };

    if (!CmwfHookSymbols(hExplorerFrame, efHooks, ARRAYSIZE(efHooks))) {
        Wh_Log(L"Failed to hook one or more symbol functions in ExplorerFrame.dll");
        return FALSE;
    }

    return TRUE;
}

void Wh_ModUninit(void)
{
    for (auto function : hookedFunctions) {
        Wh_RemoveFunctionHook(function);
    }
    if (hExplorerFrame) {
        FreeLibrary(hExplorerFrame);
        hExplorerFrame = nullptr;
    }
}
