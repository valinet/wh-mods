// ==WindhawkMod==
// @id              valinet-vcxsrv-dark-titlebars
// @name            vcxsrv dark titlebars
// @description     Make titlebars of select X11 windows displayed using vcxsrv dark
// @version         0.1
// @author          valinet
// @github          https://github.com/valinet
// @homepage        https://valinet.ro
// @include         vcxsrv.exe
// @compilerOptions -ldwmapi
// @license         GPLv3
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# vcxsrv dark titlebars
Make titlebars of select X11 windows displayed using vcxsrv dark
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- res_name:
  - ""
  $name: res_name
  $description: List of res_name's whose windows will have dark title bar
- res_class:
  - "sourcegit"
  - "qtcreator"
  $name: res_class
  $description: List of res_class'es whose windows will have dark title bar
*/
// ==/WindhawkModSettings==

#include <stdint.h>
#include <dwmapi.h>
#include <iostream>

typedef unsigned long XID;
typedef struct _Screen *ScreenPtr;
typedef struct _Drawable {
    unsigned char type;         /* DRAWABLE_<type> */
    unsigned char _class;        /* specific to type */
    unsigned char depth;
    unsigned char bitsPerPixel;
    XID id;                     /* resource id */
    short x;                    /* window: screen absolute, pixmap: 0 */
    short y;                    /* window: screen absolute, pixmap: 0 */
    unsigned short width;
    unsigned short height;
    ScreenPtr pScreen;
    unsigned long serialNumber;
} DrawableRec;
typedef struct _Private PrivateRec, *PrivatePtr;
typedef struct _Window *WindowPtr;
struct pixman_box16_t
{
    int16_t x1, y1, x2, y2;
};
struct pixman_region16
{
    pixman_box16_t          extents;
    void *data;
};
typedef struct pixman_region16 RegionRec, *RegionPtr;
typedef unsigned long Mask;
typedef struct _xPoint {
    INT16   x, y;
} xPoint;
typedef xPoint DDXPointRec;
typedef struct _Pixmap *PixmapPtr;
typedef union _PixUnion {
    PixmapPtr pixmap;
    unsigned long pixel;
} PixUnion;
typedef struct _Cursor *CursorPtr;
typedef unsigned long VisualID;
typedef XID Colormap;
typedef struct _Property {
    struct _Property *next;
    uint32_t propertyName;
    uint32_t type;                  /* ignored by server */
    uint32_t format;            /* format of data for swapping - 8,16,32 */
    uint32_t size;              /* size of data in (format/8) bytes */
    void *data;                 /* private to client */
    PrivateRec *devPrivates;
} PropertyRec;
typedef struct _Property *PropertyPtr;
typedef struct _WindowOpt {
    CursorPtr cursor;           /* default: window.cursorNone */
    VisualID visual;            /* default: same as parent */
    Colormap colormap;          /* default: same as parent */
    Mask dontPropagateMask;     /* default: window.dontPropagate */
    Mask otherEventMasks;       /* default: 0 */
    struct _OtherClients *otherClients; /* default: NULL */
    struct _GrabRec *passiveGrabs;      /* default: NULL */
    PropertyPtr userProps;      /* default: NULL */
    // ... truncated
} WindowOptRec, *WindowOptPtr;
typedef struct _Window {
    DrawableRec drawable;
    PrivateRec *devPrivates;
    WindowPtr parent;           /* ancestor chain */
    WindowPtr nextSib;          /* next lower sibling */
    WindowPtr prevSib;          /* next higher sibling */
    WindowPtr firstChild;       /* top-most child */
    WindowPtr lastChild;        /* bottom-most child */
    RegionRec clipList;         /* clipping rectangle for output */
    RegionRec borderClip;       /* NotClippedByChildren + border */
    union _Validate *valdata;
    RegionRec winSize;
    RegionRec borderSize;
    DDXPointRec origin;         /* position relative to parent */
    unsigned short borderWidth;
    unsigned long deliverableEvents;   /* all masks from all clients */
    Mask eventMask;             /* mask from the creating client */
    PixUnion background;
    PixUnion border;
    WindowOptPtr optional;
    // ... truncated
} WindowRec;

#define XA_WM_CLASS 67
#define XA_STRING 31
const char WIN_WINDOW_PROP[]=     "vcxsrv_window_prop_rl";
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20

// https://github.com/marchaesen/vcxsrv/blob/d0a1eaf7ee15fcdf4f683388a88fec49078e6408/xorg-server/hw/xwin/winmultiwindowclass.c#L47
int
winMultiWindowGetClassHint(WindowPtr pWin, char **res_name, char **res_class)
{
    struct _Window *pwin;
    struct _Property *prop;
    int len_name, len_class;

    if (!pWin || !res_name || !res_class) {
        Wh_Log(L"winMultiWindowGetClassHint - pWin, res_name, or res_class was "
               "NULL");
        return 0;
    }

    pwin = (struct _Window *) pWin;
    
    
    if (pwin->optional)
        prop = (struct _Property *) pwin->optional->userProps;
    else
        prop = NULL;

    *res_name = *res_class = NULL;

    while (prop) {
        if (prop->propertyName == XA_WM_CLASS
            && prop->type == XA_STRING && prop->format == 8 && prop->data) {
            /*
              WM_CLASS property should consist of 2 null terminated strings, but we
              must handle the cases when one or both is absent or not null terminated
            */
            len_name = strlen((char *) prop->data);
            if (len_name > prop->size) len_name = prop->size;

            (*res_name) = (char*)malloc(len_name + 1);

            if (!*res_name) {
                Wh_Log(L"winMultiWindowGetClassHint - *res_name was NULL\n");
                return 0;
            }

            /* Copy name and ensure null terminated */
            strncpy((*res_name), (char*)prop->data, len_name);
            (*res_name)[len_name] = '\0';

            /* Compute length of class name, it could be that it is absent or not null terminated */
            len_class = (len_name >= prop->size) ? 0 : (strlen(((char *) prop->data) + 1 + len_name));
            if (len_class > prop->size - 1 - len_name) len_class = prop->size - 1 - len_name;

            (*res_class) = (char*)malloc(len_class + 1);

            if (!*res_class) {
                Wh_Log(L"winMultiWindowGetClassHint - *res_class was NULL\n");

                /* Free the previously allocated res_name */
                free(*res_name);
                return 0;
            }

            /* Copy class name and ensure null terminated */
            strncpy((*res_class), ((char *) prop->data) + 1 + len_name, len_class);
            (*res_class)[len_class] = '\0';

            return 1;
        }
        else
            prop = prop->next;
    }

    return 0;
}

using CreateWindowExA_t = decltype(&CreateWindowExA);
CreateWindowExA_t CreateWindowExA_Original;
HWND WINAPI CreateWindowExA_Hook(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    HWND hWnd = CreateWindowExA_Original(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (hWnd) {
        WindowPtr pWin = (WindowPtr)GetPropA(hWnd, WIN_WINDOW_PROP);
        if (pWin) {
            char *res_name = nullptr, *res_class = nullptr;
            if (winMultiWindowGetClassHint(pWin, &res_name, &res_class)) {
                Wh_Log(L">> res_name: [%S], res_class: [%S]", res_name, res_class);
                bool ok = false;
                for (int i = 0;; i++) {
                    PCWSTR str = Wh_GetStringSetting(L"res_name[%d]", i);
                    bool empty = !*str;
                    if (!empty) {
                        char setting[100]{};
                        sprintf_s(setting, "%S", str);
                        if (!strcmp(res_name, setting)) {
                            BOOL use = TRUE;
                            DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &use, sizeof(use));
                            ok = true;
                            break;
                        }
                    }
                    Wh_FreeStringSetting(str);
                    if (empty)
                        break;
                }
                if (!ok) {
                    for (int i = 0;; i++) {
                        PCWSTR str = Wh_GetStringSetting(L"res_class[%d]", i);
                        bool empty = !*str;
                        if (!empty) {
                            char setting[100]{};
                            sprintf_s(setting, "%S", str);
                            if (!strcmp(res_name, setting)) {
                                BOOL use = TRUE;
                                DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &use, sizeof(use));
                                break;
                            }
                        }
                        Wh_FreeStringSetting(str);
                        if (empty)
                            break;
                    }
                }
                free(res_name);
                free(res_class);
            }
        }
    }
    return hWnd;
}

// The mod is being initialized, load settings, hook functions, and do other
// initialization stuff if required.
BOOL Wh_ModInit() {
    //Wh_Log(L"Init");
    Wh_SetFunctionHook((void*)CreateWindowExA, (void*)CreateWindowExA_Hook, (void**)&CreateWindowExA_Original);
    return TRUE;
}

// The mod is being unloaded, free all allocated resources.
void Wh_ModUninit() {
    //Wh_Log(L"Uninit");
    if (CreateWindowExA_Original)
        Wh_RemoveFunctionHook((void*)CreateWindowExA);
}

// The mod setting were changed, reload them.
void Wh_ModSettingsChanged() {
    //Wh_Log(L"SettingsChanged");
}
