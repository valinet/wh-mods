// ==WindhawkMod==
// @id              valinet-explorer7-tweaks
// @name            explorer7 tweaks
// @description     Tweaks for Explorer7 project
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @homepage        https://valinet.ro
// @include         explorer.exe
// @compilerOptions -lComctl32 -lMsimg32 -lgdi32 -lShcore
// @license         GPLv3
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# valinet explorer7 tweaks
Tweaks for Explorer7 project:
* Change taskbar large icons size
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- taskbar_icon_size: 24
  $name: Taskbar Large Icon Size
  $description: Specifies the size of the large icons on Windows 7 taskbar. 24 is the default in Windows 10. Value can range from 1 to 32, with 32 being the Windows 7 default.
*/
// ==/WindhawkModSettings==

#include <Windows.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <shellscalingapi.h>

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

const IID IID_ImageList2 = { 0x192B9D83, 0x50FC, 0x457B, { 0x90, 0xA0, 0x2B, 0x82, 0xA8, 0xB5, 0xDA, 0xE1 } };

struct MyIImageList2Vtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IImageList *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IImageList *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IImageList *This);

    HRESULT (STDMETHODCALLTYPE *Add)(
        IImageList *This,
        HBITMAP hbmImage,
        HBITMAP hbmMask,
        int *pi);

    HRESULT (STDMETHODCALLTYPE *ReplaceIcon)(
        IImageList *This,
        int i,
        HICON hicon,
        int *pi);

    HRESULT (STDMETHODCALLTYPE *SetOverlayImage)(
        IImageList *This,
        int iImage,
        int iOverlay);

    HRESULT (STDMETHODCALLTYPE *Replace)(
        IImageList *This,
        int i,
        HBITMAP hbmImage,
        HBITMAP hbmMask);

    HRESULT (STDMETHODCALLTYPE *AddMasked)(
        IImageList *This,
        HBITMAP hbmImage,
        COLORREF crMask,
        int *pi);

    HRESULT (STDMETHODCALLTYPE *Draw)(
        IImageList *This,
        IMAGELISTDRAWPARAMS *pimldp);

    // truncated...
};
MyIImageList2Vtbl* pImageList2Tbl;

struct MyIImageList2 {
    MyIImageList2Vtbl* lpVtbl;
};

HRESULT (STDMETHODCALLTYPE *ImageList_Draw_Orig)(IImageList *This, IMAGELISTDRAWPARAMS *pimldp);

void DrawImageListScaled(IImageList* pImageList, int iImage,
                         HDC hdcTarget, int x, int y,
                         int cx, int cy,
                         int newCx, int newCy)
{
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = cx;
    bmi.bmiHeader.biHeight      = -cy;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdcFull = CreateCompatibleDC(hdcTarget);
    void* pBitsFull = nullptr;
    HBITMAP hbmpFull = CreateDIBSection(hdcFull, &bmi, DIB_RGB_COLORS, &pBitsFull, NULL, 0);
    if (!hbmpFull) { DeleteDC(hdcFull); return; }
    HBITMAP hOldFull = (HBITMAP)SelectObject(hdcFull, hbmpFull);
    memset(pBitsFull, 0, cx * cy * 4);

    IMAGELISTDRAWPARAMS ildp{};
    ildp.cbSize = sizeof(ildp);
    ildp.i      = iImage;
    ildp.hdcDst = hdcFull;
    ildp.x      = 0;
    ildp.y      = 0;
    ildp.cx     = 0;
    ildp.cy     = 0;
    ildp.fStyle = ILD_NORMAL;
    ildp.dwRop  = SRCCOPY;
    ildp.fState = ILS_NORMAL;
    ImageList_Draw_Orig(pImageList, &ildp);

    bmi.bmiHeader.biWidth  = newCx;
    bmi.bmiHeader.biHeight = -newCy;

    HDC hdcSmall = CreateCompatibleDC(hdcTarget);
    void* pBitsSmall = nullptr;
    HBITMAP hbmpSmall = CreateDIBSection(hdcSmall, &bmi, DIB_RGB_COLORS, &pBitsSmall, NULL, 0);
    if (!hbmpSmall) {
        SelectObject(hdcFull, hOldFull);
        DeleteObject(hbmpFull);
        DeleteDC(hdcFull);
        DeleteDC(hdcSmall);
        return;
    }
    HBITMAP hOldSmall = (HBITMAP)SelectObject(hdcSmall, hbmpSmall);

    BYTE* src = (BYTE*)pBitsFull;
    BYTE* dst = (BYTE*)pBitsSmall;

    for (int dy = 0; dy < newCy; dy++)
    {
        int sy0 = dy * cy / newCy;
        int sy1 = (dy + 1) * cy / newCy;
        if (sy1 == sy0) sy1 = sy0 + 1;

        for (int dx = 0; dx < newCx; dx++)
        {
            int sx0 = dx * cx / newCx;
            int sx1 = (dx + 1) * cx / newCx;
            if (sx1 == sx0) sx1 = sx0 + 1;

            int sumB = 0, sumG = 0, sumR = 0, sumA = 0;
            int count = 0;
            for (int sy = sy0; sy < sy1; sy++)
            {
                for (int sx = sx0; sx < sx1; sx++)
                {
                    BYTE* p = src + (sy * cx + sx) * 4;
                    sumB += p[0];
                    sumG += p[1];
                    sumR += p[2];
                    sumA += p[3];
                    count++;
                }
            }

            BYTE* d = dst + (dy * newCx + dx) * 4;
            d[0] = (BYTE)(sumB / count);
            d[1] = (BYTE)(sumG / count);
            d[2] = (BYTE)(sumR / count);
            d[3] = (BYTE)(sumA / count);
        }
    }

    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdcTarget, x, y, newCx, newCy,
               hdcSmall, 0, 0, newCx, newCy, bf);

    SelectObject(hdcFull, hOldFull);
    DeleteObject(hbmpFull);
    DeleteDC(hdcFull);
    SelectObject(hdcSmall, hOldSmall);
    DeleteObject(hbmpSmall);
    DeleteDC(hdcSmall);
}

HRESULT STDMETHODCALLTYPE ImageList_Draw_Hook(IImageList *_this, IMAGELISTDRAWPARAMS *pimldp) {
    if (pimldp->fStyle & ILD_TRANSPARENT) {
        int cx = 0, cy = 0;
        _this->GetIconSize(&cx, &cy);
        HMONITOR hMon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
        UINT dpiX = 96, dpiY = 96;
        GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
        if (pimldp->x != 0 && pimldp->y != 0 && MulDiv(cx, 96, dpiX) == 32 && MulDiv(cy, 96, dpiY) == 32) {
            int sz = MAX(1, MIN(Wh_GetIntSetting(L"taskbar_icon_size"), 32));
            int new_cx = MulDiv(cx, sz, 32);
            int new_cy = MulDiv(cy, sz, 32);
            if (cx != new_cx && cy != new_cy) {
                DrawImageListScaled(_this, pimldp->i, pimldp->hdcDst, pimldp->x + (cx - new_cx) / 2, pimldp->y + (cy - new_cy) / 2, cx, cy, new_cx, new_cy);
                return S_OK;
            }
        }
    }
    return ImageList_Draw_Orig(_this, pimldp);
}

// The mod is being initialized, load settings, hook functions, and do other
// initialization stuff if required.
BOOL Wh_ModInit() {
    //Wh_Log(L"Init");
    HIMAGELIST himl = ImageList_Create(16, 16, 0x21, 1, 1);
    if (himl) {
        IImageList2* pIml = nullptr;
        HIMAGELIST_QueryInterface(himl, IID_ImageList2, (void**)&pIml);
        if (pIml) {
            DWORD flOldProtect = 0;
            if (VirtualProtect((LPVOID)(((MyIImageList2*)pIml)->lpVtbl), sizeof(MyIImageList2Vtbl), PAGE_EXECUTE_READWRITE, &flOldProtect)) {
                ImageList_Draw_Orig = ((MyIImageList2*)pIml)->lpVtbl->Draw;
                ((MyIImageList2*)pIml)->lpVtbl->Draw = ImageList_Draw_Hook;
                VirtualProtect((LPVOID)(((MyIImageList2*)pIml)->lpVtbl), sizeof(MyIImageList2Vtbl), flOldProtect, &flOldProtect);
                pImageList2Tbl = ((MyIImageList2*)pIml)->lpVtbl;
                //Wh_Log(L"%p %p", ((MyIImageList2*)pIml)->lpVtbl->Draw, *(uintptr_t*)(((char*)GetModuleHandleW(nullptr)) + 0xF0340));
            }
            pIml->Release();
        }
        ImageList_Destroy(himl);
    }
    SendMessageW(FindWindowW(L"Shell_TrayWnd", nullptr), WM_SETTINGCHANGE, 0, 0);
    return TRUE;
}

// The mod is being unloaded, free all allocated resources.
void Wh_ModUninit() {
    //Wh_Log(L"Uninit");
    if (pImageList2Tbl && ImageList_Draw_Orig) {
        DWORD flOldProtect = 0;
        if (VirtualProtect((LPVOID)pImageList2Tbl, sizeof(MyIImageList2Vtbl), PAGE_EXECUTE_READWRITE, &flOldProtect)) {
            pImageList2Tbl->Draw = ImageList_Draw_Orig;
            VirtualProtect((LPVOID)pImageList2Tbl, sizeof(MyIImageList2Vtbl), flOldProtect, &flOldProtect);
        }
    }
}

// The mod setting were changed, reload them.
void Wh_ModSettingsChanged() {
    //Wh_Log(L"SettingsChanged");
}
