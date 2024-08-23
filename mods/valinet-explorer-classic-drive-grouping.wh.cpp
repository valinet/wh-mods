// ==WindhawkMod==
// @id              valinet-explorer-classic-drive-grouping
// @name            Explorer: Classic drive grouping
// @description     Explorer: Classic drive grouping
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
HMODULE hPropsys = nullptr;
HMODULE hShell32 = nullptr;

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

typedef HRESULT (WINAPI *VariantToBuffer_t)(LPVARIANT, void *, UINT);
VariantToBuffer_t VariantToBuffer = nullptr;

typedef struct tagDRIVEGROUPI18N {
    LANGID lid;
    WCHAR szHardDisks[256];
    WCHAR szRemovable[256];
    WCHAR szOther[256];
    WCHAR szScanners[256];
    WCHAR szPortableMedia[256];
    WCHAR szPortable[256];
} DRIVEGROUPI18N;

typedef const DRIVEGROUPI18N *LPCDRIVEGROUPI18N;

typedef enum _DRIVEGROUP {
    DG_HARDDISKS = 0,
    DG_REMOVABLE,
    DG_OTHER,
    DG_SCANNERS,
    DG_PORTABLEMEDIA,
    DG_PORTABLE
} DRIVEGROUP;

#pragma region "Drive grouping localization"
const DRIVEGROUPI18N g_driveGroupI18n[] = {
    {
        MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_SAUDI_ARABIA),
        L"محركات الأقراص الثابتة",
        L"أجهزة ذات وحدة تخزين قابلة للنقل",
        L"أخرى",
        L"الماسحات الضوئية والكاميرات",
        L"أجهزة الوسائط المحمولة",
        L"الأجهزة المحمولة"
    },
    {
        MAKELANGID(LANG_BULGARIAN, SUBLANG_DEFAULT),
        L"Твърди дискови устройства",
        L"Устройства със сменяеми носители",
        L"Друго",
        L"Скенери и фотоапарати",
        L"Преносими мултимедийни устройства",
        L"Преносими устройства"
    },
    {
        MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL),
        L"硬碟",
        L"裝置中含有卸除式存放裝置",
        L"其他",
        L"掃描器與數位相機",
        L"可攜式媒體裝置",
        L"可攜式裝置"
    },
    {
        MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT),
        L"Jednotky pevných disků",
        L"Zařízení s vyměnitelným úložištěm",
        L"Jiná",
        L"Skenery a fotoaparáty",
        L"Portable Media Devices",
        L"Přenosná zařízení"
    },
    {
        MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT),
        L"Harddiskdrev",
        L"Enheder med flytbare medier",
        L"Andet",
        L"Scannere og kameraer",
        L"Bærbare medieenheder",
        L"Bærbare enheder"
    },
    {
        MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN),
        L"Festplatten",
        L"Geräte mit Wechselmedien",
        L"Weitere",
        L"Scanner und Kameras",
        L"Tragbare Mediengeräte",
        L"Tragbare Geräte"
    },
    {
        MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT),
        L"Μονάδες σκληρών δίσκων",
        L"Συσκευές με αφαιρούμενα μέσα αποθήκευσης",
        L"Άλλα",
        L"Σαρωτές και κάμερες",
        L"Φορητές συσκευές πολυμέσων",
        L"Φορητές συσκευές"
    },
    {
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        L"Hard Disk Drives",
        L"Devices with Removable Storage",
        L"Other",
        L"Scanners and Cameras",
        L"Portable Media Devices",
        L"Portable Devices"
    },
    {
        MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT),
        L"Kiintolevyasemat",
        L"Laitteet, joissa on siirrettävä tallennusväline",
        L"Muu",
        L"Skannerit ja kamerat",
        L"Kannettavat medialaitteet",
        L"Kannettavat laitteet"
    },
    {
        MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH),
        L"Disques durs",
        L"Périphériques utilisant des supports de stockage amovibles",
        L"Autre",
        L"Scanneurs et appareils photo",
        L"Appareils mobiles multimédias",
        L"Périphériques amovibles"
    },
    {
        MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT),
        L"כוננים קשיחים",
        L"התקנים עם אחסון נשלף",
        L"אחר",
        L"סורקים ומצלמות",
        L"מכשירי מדיה ניידים",
        L"מכשירים ניידים"
    },
    {
        MAKELANGID(LANG_HUNGARIAN, SUBLANG_DEFAULT),
        L"Merevlemez-meghajtók",
        L"Cserélhető adathordozós eszközök",
        L"Egyéb",
        L"Képolvasók és fényképezőgépek",
        L"Hordozható lejátszóeszközök",
        L"Hordozható eszközök"
    },
    {
        MAKELANGID(LANG_ITALIAN, SUBLANG_ITALIAN),
        L"Unità disco rigido",
        L"Dispositivi con archivi rimovibili",
        L"Altro",
        L"Scanner e fotocamere digitali",
        L"Dispositivi audio/video mobili",
        L"Dispositivi portatili"
    },
    {
        MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
        L"ハード ディスク ドライブ",
        L"リムーバブル記憶域があるデバイス",
        L"その他",
        L"スキャナーとカメラ",
        L"ポータブル メディア デバイス",
        L"ポータブル デバイス"
    },
    {
        MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN),
        L"하드 디스크 드라이브",
        L"이동식 미디어 장치",
        L"기타",
        L"스캐너 및 카메라",
        L"휴대용 미디어 장치",
        L"휴대용 장치"
    },
    {
        MAKELANGID(LANG_DUTCH, SUBLANG_DUTCH),
        L"Hardeschijfstations",
        L"Apparaten met verwisselbare opslagmedia",
        L"Overige",
        L"Scanners en camera's",
        L"Draagbare media-apparaten",
        L"Draagbare apparaten"
    },
    {
        MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL),
        L"Harddiskstasjoner",
        L"Flyttbare lagringsmedier",
        L"Annet",
        L"Skannere og kameraer",
        L"Bærbare medieenheter",
        L"Bærbare enheter"
    },
    {
        MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT),
        L"Dyski twarde",
        L"Urządzenia z wymiennymi nośnikami pamięci",
        L"Inne",
        L"Skanery i aparaty fotograficzne",
        L"Przenośne urządzenia multimedialne",
        L"Urządzenia przenośne"
    },
    {
        MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN),
        L"Unidades de Disco Rígido",
        L"Dispositivos com Armazenamento Removível",
        L"Outros",
        L"Scanners e Câmeras",
        L"Dispositivos de Mídia Portáteis",
        L"Dispositivos Portáteis"
    },
    {
        MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT),
        L"Unități de hard disk",
        L"Unități cu stocare detașabilă",
        L"Altul",
        L"Scanere și aparate foto",
        L"Dispozitive media portabile",
        L"Dispozitive portabile"
    },
    {
        MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT),
        L"Жесткие диски",
        L"Устройства со съемными носителями",
        L"Другие",
        L"Сканеры и камеры",
        L"Переносные устройства мультимедиа",
        L"Портативные устройства"
    },
    {
        MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT),
        L"Pogoni tvrdih diskova",
        L"Uređaji s prijenosnom pohranom",
        L"Ostalo",
        L"Skeneri i kamere",
        L"Prijenosni medijski uređaji",
        L"Prijenosni uređaji"
    },
    {
        MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT),
        L"Jednotky pevného disku",
        L"Zariadenia s vymeniteľným ukladacím priestorom",
        L"Iné",
        L"Skenery a fotoaparáty",
        L"Prenosné mediálne zariadenia",
        L"Prenosné zariadenia"
    },
    {
        MAKELANGID(LANG_SWEDISH, SUBLANG_SWEDISH),
        L"Hårddiskar",
        L"Enheter med flyttbara lagringsmedia",
        L"Annan",
        L"Skannrar och kameror",
        L"Bärbara medieenheter",
        L"Bärbara enheter"
    },
    {
        MAKELANGID(LANG_THAI, SUBLANG_DEFAULT),
        L"ฮาร์ดดิสก์ไดรฟ์",
        L"อุปกรณ์ที่มีที่เก็บข้อมูลแบบถอดได้",
        L"อื่นๆ",
        L"สแกนเนอร์และกล้อง",
        L"อุปกรณ์สื่อแบบพกพา",
        L"อุปกรณ์แบบพกพา"
    },
    {
        MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT),
        L"Sabit Disk Sürücüleri",
        L"Çıkarılabilir Depolama Birimi Olan Aygıtlar",
        L"Diğer",
        L"Tarayıcılar ve Kameralar",
        L"Taşınabilir Medya Aygıtları",
        L"Taşınabilir Aygıtlar"
    },
    {
        MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT),
        L"Жорсткі диски",
        L"Пристрої зі знімними носіями",
        L"Інше",
        L"Сканери та камери",
        L"Портативні носії",
        L"Портативні пристрої"
    },
    {
        MAKELANGID(LANG_SLOVENIAN, SUBLANG_DEFAULT),
        L"Trdi diski",
        L"Naprave z izmenljivimi mediji",
        L"Drugo",
        L"Optični bralniki in fotoaparati",
        L"Prenosne predstavnostne naprave",
        L"Prenosne naprave"
    },
    {
        MAKELANGID(LANG_ESTONIAN, SUBLANG_DEFAULT),
        L"Kõvakettad",
        L"Irdsalvestiga seadmed",
        L"Muu",
        L"Skannerid ja kaamerad",
        L"Kantavad meediumiseadmed",
        L"Kandeseadmed"
    },
    {
        MAKELANGID(LANG_LATVIAN, SUBLANG_DEFAULT),
        L"Cietie diski",
        L"Ierīces ar noņemamu krātuvi",
        L"Citi",
        L"Skeneri un kameras",
        L"Portatīvās datu nesēju ierīces",
        L"Portatīvās ierīces"
    },
    {
        MAKELANGID(LANG_LITHUANIAN, SUBLANG_LITHUANIAN),
        L"Standieji diskai",
        L"Įrenginiai su keičiamąja laikmena",
        L"Kita",
        L"Skaitytuvai ir fotoaparatai",
        L"Nešiojamieji medijos įrenginiai",
        L"Nešiojamieji įrenginiai"
    },
    {
        MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
        L"硬盘",
        L"有可移动存储的设备",
        L"其他",
        L"扫描仪和照相机",
        L"便携媒体设备",
        L"便携设备"
    },
    {
        MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE),
        L"Unidades de Disco Rígido",
        L"Dispositivos com Armazenamento Amovível",
        L"Outro",
        L"Scanners e Câmaras",
        L"Dispositivos de Multimédia Portáteis",
        L"Dispositivos Portáteis"
    },
    {
        MAKELANGID(LANG_CROATIAN, 0x2),
        L"Jedinice čvrstog diska",
        L"Uređaji sa prenosivim skladištenjem",
        L"Ostalo",
        L"Skeneri i fotoaparati",
        L"Prenosni medijski uređaji",
        L"Prenosni uređaji"
    },
    {
        MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_MODERN),
        L"Unidades de disco duro",
        L"Dispositivos con almacenamiento extraíble",
        L"Otros",
        L"Escáneres y cámaras",
        L"Dispositivos multimedia portátiles",
        L"Dispositivos portátiles"
    },
};
#pragma endregion

LPCDRIVEGROUPI18N GetCurrentDriveLocale(void) {
    LANGID lid = GetUserDefaultUILanguage();

    /* So we can fallback to English without iterating again. */
    LPCDRIVEGROUPI18N en = nullptr;

    for (int i = 0; i < ARRAYSIZE(g_driveGroupI18n); i++) {
        if (g_driveGroupI18n[i].lid == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)) {
            en = g_driveGroupI18n + i;
        }
        if (g_driveGroupI18n[i].lid == lid) {
            return g_driveGroupI18n + i;
        }
    }

    return en;
}

const struct { DWORD dwDescId; UINT uResId; } g_categoryMap[] = {
    { SHDID_FS_DIRECTORY,        9338 },
    { SHDID_COMPUTER_SHAREDDOCS, 9338 },
    { SHDID_COMPUTER_FIXED,      DG_HARDDISKS },
    { SHDID_COMPUTER_DRIVE35,    DG_REMOVABLE },
    { SHDID_COMPUTER_REMOVABLE,  DG_REMOVABLE },
    { SHDID_COMPUTER_CDROM,      DG_REMOVABLE },
    { SHDID_COMPUTER_DRIVE525,   DG_REMOVABLE },
    { SHDID_COMPUTER_NETDRIVE,   9340 },
    { SHDID_COMPUTER_OTHER,      DG_OTHER },
    { SHDID_COMPUTER_RAMDISK,    DG_OTHER },
    { SHDID_COMPUTER_IMAGING,    DG_SCANNERS },
    { SHDID_COMPUTER_AUDIO,      DG_PORTABLEMEDIA },
    { SHDID_MOBILE_DEVICE,       DG_PORTABLE }
};

void *CStorageSystemTypeCategorizer_GetCategory_addr SHARED_SECTION = nullptr;
HRESULT (STDCALL *CStorageSystemTypeCategorizer_GetCategory_orig)(ICategorizer *, UINT, PCUITEMID_CHILD_ARRAY, DWORD *) = nullptr;
HRESULT STDCALL CStorageSystemTypeCategorizer_GetCategory_hook(
    ICategorizer          *pThis,
    UINT                   cidl,
    PCUITEMID_CHILD_ARRAY  apidl,
    DWORD                 *rgCategoryIds
) {
    HRESULT hr = S_OK;
    IShellFolder2 *pShellFolder = (IShellFolder2 *)*((INT_PTR *)pThis + 3);
    if (pShellFolder) {
        for (UINT i = 0; i < cidl; i++) {
            rgCategoryIds[i] = DG_OTHER;

            VARIANT v;
            VariantInit(&v);

            SHCOLUMNID scid;
            scid.fmtid = FMTID_ShellDetails;
            scid.pid = PID_DESCRIPTIONID;

            hr = pShellFolder->GetDetailsEx(apidl[i], &scid, &v);
            if (SUCCEEDED(hr)) {
                SHDESCRIPTIONID shdid;
                if (VariantToBuffer && SUCCEEDED(VariantToBuffer(&v, &shdid, sizeof(SHDESCRIPTIONID)))) {
                    for (UINT j = 0; j < ARRAYSIZE(g_categoryMap); j++) {
                        if (shdid.dwDescriptionId == g_categoryMap[j].dwDescId) {
                            rgCategoryIds[i] = g_categoryMap[j].uResId;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    return hr;
}

void *CStorageSystemTypeCategorizer_CompareCategory_addr SHARED_SECTION = nullptr;
HRESULT (STDCALL *CStorageSystemTypeCategorizer_CompareCategory_orig)(ICategorizer *, CATSORT_FLAGS, DWORD, DWORD) = nullptr;
HRESULT STDCALL CStorageSystemTypeCategorizer_CompareCategory_hook(
    ICategorizer *pThis,
    CATSORT_FLAGS csfFlags,
    DWORD         dwCategoryId1,
    DWORD         dwCategoryId2
) {
    int categoryArraySize = ARRAYSIZE(g_categoryMap);

    int firstPos = -1;
    int secondPos = -1;

    for (int i = 0; i < categoryArraySize; i++) {
        if (g_categoryMap[i].uResId == dwCategoryId1) {
            firstPos = i;
            break;
        }
    }

    for (int i = 0; i < categoryArraySize; i++) {
        if (g_categoryMap[i].uResId == dwCategoryId2) {
            secondPos = i;
            break;
        }
    }

    int diff = firstPos - secondPos;

    if (diff < 0) {
        return 0xFFFF;
    }

    return diff > 0;
}

void *CStorageSystemTypeCategorizer_GetCategoryInfo_addr SHARED_SECTION = nullptr;
HRESULT (STDCALL *CStorageSystemTypeCategorizer_GetCategoryInfo_orig)(ICategorizer *, DWORD, CATEGORY_INFO *) = nullptr;
HRESULT STDCALL CStorageSystemTypeCategorizer_GetCategoryInfo_hook(
    ICategorizer  *pThis,
    DWORD          dwCategoryId,
    CATEGORY_INFO *pci
) {
    HRESULT hr = CStorageSystemTypeCategorizer_GetCategoryInfo_orig(
        pThis, dwCategoryId, pci
    );
    if (SUCCEEDED(hr)) {
        LPCDRIVEGROUPI18N dgi = GetCurrentDriveLocale();
        LPCWSTR lpszOut = nullptr;

        if (dgi) {
            switch ((DRIVEGROUP)dwCategoryId) {
                case DG_HARDDISKS:
                    lpszOut = dgi->szHardDisks;
                    break;
                case DG_REMOVABLE:
                    lpszOut = dgi->szRemovable;
                    break;
                case DG_OTHER:
                    lpszOut = dgi->szOther;
                    break;
                case DG_SCANNERS:
                    lpszOut = dgi->szScanners;
                    break;
                case DG_PORTABLEMEDIA:
                    lpszOut = dgi->szPortableMedia;
                    break;
                case DG_PORTABLE:
                    lpszOut = dgi->szPortable;
                    break;
            }

            if (lpszOut) {
                wcscpy(pci->wszName, lpszOut);
            }
        }
    }
    return hr;
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
    if (!hPropsys) {
        hPropsys = LoadLibraryW(L"propsys.dll");
    }
    if (!hPropsys) {
        Wh_Log(L"Failed to load propsys.dll");
        return FALSE;
    }
    if (hPropsys) {
        VariantToBuffer = (VariantToBuffer_t)GetProcAddress(hPropsys, "VariantToBuffer");
    }
    if (!VariantToBuffer) {
        Wh_Log(L"Failed to find propsys.dll!VariantToBuffer");
        FreeLibrary(hPropsys);
        return FALSE;
    }

    if (!hShell32) {
        hShell32 = LoadLibraryW(L"shell32.dll");
    }
    if (!hShell32) {
        Wh_Log(L"Failed to load shell32.dll");
        FreeLibrary(hPropsys);
        return FALSE;
    }

    const CMWF_SYMBOL_HOOK shHooks[] = {
        {
            {
                L"public: virtual long "
                SSTDCALL
                L" CStorageSystemTypeCategorizer::GetCategory(unsigned int,struct _ITEMID_CHILD const "
#ifdef _WIN64
                L"__unaligned "
#endif
                L"* const *,unsigned long *)"
            },
            &CStorageSystemTypeCategorizer_GetCategory_orig,
            CStorageSystemTypeCategorizer_GetCategory_hook,
            false,
            &CStorageSystemTypeCategorizer_GetCategory_addr
        },
        {
            {
                L"public: virtual long "
                SSTDCALL
                L" CStorageSystemTypeCategorizer::CompareCategory(enum CATSORT_FLAGS,unsigned long,unsigned long)"
            },
            &CStorageSystemTypeCategorizer_CompareCategory_orig,
            CStorageSystemTypeCategorizer_CompareCategory_hook,
            false,
            &CStorageSystemTypeCategorizer_CompareCategory_addr
        },
        {
            {
                L"public: virtual long "
                SSTDCALL
                L" CStorageSystemTypeCategorizer::GetCategoryInfo(unsigned long,struct CATEGORY_INFO *)"
            },
            &CStorageSystemTypeCategorizer_GetCategoryInfo_orig,
            CStorageSystemTypeCategorizer_GetCategoryInfo_hook,
            false,
            &CStorageSystemTypeCategorizer_GetCategoryInfo_addr
        }
    };

    if (!CmwfHookSymbols(hShell32, shHooks, ARRAYSIZE(shHooks))) {
        Wh_Log(L"Failed to hook one or more symbol functions in shell32.dll");
        return FALSE;
    }

    return TRUE;
}

void Wh_ModUninit(void)
{
    for (auto function : hookedFunctions) {
        Wh_RemoveFunctionHook(function);
    }
    if (hPropsys) {
        FreeLibrary(hPropsys);
        hPropsys = nullptr;
    }
    if (hShell32) {
        FreeLibrary(hShell32);
        hShell32 = nullptr;
    }
}
