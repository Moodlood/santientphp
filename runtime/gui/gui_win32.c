// ============================================================================
// SantientPHP (SPH) - Win32 GUI Platform Abstraction Layer
// Binds to user32.dll, gdi32.dll, comctl32.dll, ole32.dll with zero 3rd-party libs
// ============================================================================

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <ole2.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gui.h"

// Forward declaration for user program's entry point
extern int main(int argc, char** argv);

static const wchar_t* SPH_WINDOW_CLASS = L"SantientPHP_Window";
static int g_class_registered = 0;
static HFONT g_gui_font = NULL;

// Helper: UTF-8 to UTF-16
static wchar_t* sph_utf8_to_utf16(const char* utf8_str) {
    if (!utf8_str) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (len <= 0) return NULL;
    wchar_t* wstr = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (wstr) {
        MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, len);
    }
    return wstr;
}

// Helper: UTF-16 to UTF-8
static char* sph_utf16_to_utf8(const wchar_t* wstr) {
    if (!wstr) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char* str = (char*)malloc(len);
    if (str) {
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    }
    return str;
}

// Internal window procedure hook storage
typedef LRESULT (CALLBACK *sph_wndproc_hook_t)(HWND, UINT, WPARAM, LPARAM);

static void sph_init_gui_font(void) {
    if (!g_gui_font) {
        NONCLIENTMETRICSW ncm;
        ncm.cbSize = sizeof(NONCLIENTMETRICSW);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0)) {
            g_gui_font = CreateFontIndirectW(&ncm.lfMessageFont);
        }
        if (!g_gui_font) {
            g_gui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        }
    }
}

static LRESULT CALLBACK sph_main_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    sph_wndproc_hook_t hook = (sph_wndproc_hook_t)(intptr_t)GetPropW(hwnd, L"SPH_HOOK");
    if (hook) {
        LRESULT res = hook(hwnd, msg, wParam, lParam);
        if (res != 0) return res;
    }

    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            sph_gui_event ev;
            ev.hwnd = (void*)hwnd;
            ev.id = id;
            ev.param1 = code;
            ev.param2 = lParam;

            if (code == BN_CLICKED) {
                ev.event_type = 1; // Click
            } else if (code == EN_CHANGE) {
                ev.event_type = 7; // Text change / command
            } else {
                ev.event_type = 7;
            }

            // Dispatch to registered event handler
            sph_gui_event_callback cb = (sph_gui_event_callback)(intptr_t)GetPropW(hwnd, L"SPH_EV_CB");
            if (cb) {
                cb(&ev, (void*)(intptr_t)GetPropW(hwnd, L"SPH_EV_DATA"));
            }
            break;
        }

        case WM_PAINT: {
            sph_canvas* canvas = (sph_canvas*)(intptr_t)GetPropW(hwnd, L"SPH_CANVAS");
            if (canvas) {
                sph_canvas_present(canvas);
                ValidateRect(hwnd, NULL);
                return 0;
            }
            break;
        }

        case WM_CLOSE: {
            sph_gui_event ev;
            ev.event_type = 5; // Close
            ev.hwnd = (void*)hwnd;
            ev.id = 0;
            ev.param1 = 0;
            ev.param2 = 0;
            sph_gui_event_callback cb = (sph_gui_event_callback)(intptr_t)GetPropW(hwnd, L"SPH_EV_CB");
            if (cb) {
                cb(&ev, (void*)(intptr_t)GetPropW(hwnd, L"SPH_EV_DATA"));
            }
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY: {
            RemovePropW(hwnd, L"SPH_CANVAS");
            RemovePropW(hwnd, L"SPH_HOOK");
            RemovePropW(hwnd, L"SPH_EV_CB");
            RemovePropW(hwnd, L"SPH_EV_DATA");
            PostQuitMessage(0);
            return 0;
        }

        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void sph_register_window_class(void) {
    if (g_class_registered) return;

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS |
                ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = sph_main_wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = SPH_WINDOW_CLASS;

    RegisterClassExW(&wc);
    sph_init_gui_font();
    g_class_registered = 1;
}

// ============================================================================
// Window Lifecycle Implementation
// ============================================================================

void* sph_gui_window_create(const char* title, int32_t width, int32_t height) {
    sph_register_window_class();
    wchar_t* wtitle = sph_utf8_to_utf16(title ? title : "SantientPHP Window");

    RECT wr = { 0, 0, width, height };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        SPH_WINDOW_CLASS,
        wtitle ? wtitle : L"SantientPHP Window",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL,
        GetModuleHandleW(NULL),
        NULL
    );

    if (wtitle) free(wtitle);
    return (void*)hwnd;
}

void sph_gui_window_show(void* hwnd, int32_t show) {
    if (!hwnd) return;
    ShowWindow((HWND)hwnd, show ? SW_SHOW : SW_HIDE);
    UpdateWindow((HWND)hwnd);
}

void sph_gui_window_set_title(void* hwnd, const char* title) {
    if (!hwnd || !title) return;
    wchar_t* wtitle = sph_utf8_to_utf16(title);
    if (wtitle) {
        SetWindowTextW((HWND)hwnd, wtitle);
        free(wtitle);
    }
}

void sph_gui_window_get_size(void* hwnd, int32_t* out_w, int32_t* out_h) {
    if (!hwnd) return;
    RECT r;
    if (GetClientRect((HWND)hwnd, &r)) {
        if (out_w) *out_w = r.right - r.left;
        if (out_h) *out_h = r.bottom - r.top;
    }
}

void sph_gui_window_set_size(void* hwnd, int32_t width, int32_t height) {
    if (!hwnd) return;
    SetWindowPos((HWND)hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
}

void sph_gui_window_close(void* hwnd) {
    if (!hwnd) return;
    PostMessageW((HWND)hwnd, WM_CLOSE, 0, 0);
}

int32_t sph_gui_window_is_open(void* hwnd) {
    return (hwnd && IsWindow((HWND)hwnd)) ? 1 : 0;
}

void sph_gui_window_destroy(void* hwnd) {
    if (hwnd && IsWindow((HWND)hwnd)) {
        DestroyWindow((HWND)hwnd);
    }
}

// ============================================================================
// Native Control Implementations
// ============================================================================

void* sph_gui_button_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    if (!parent_hwnd) return NULL;
    wchar_t* wtext = sph_utf8_to_utf16(text ? text : "Button");
    HWND ctrl = CreateWindowExW(
        0,
        L"BUTTON",
        wtext ? wtext : L"Button",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        x, y, w, h,
        (HWND)parent_hwnd,
        (HMENU)(intptr_t)id,
        GetModuleHandleW(NULL),
        NULL
    );
    if (wtext) free(wtext);
    if (ctrl && g_gui_font) {
        SendMessageW(ctrl, WM_SETFONT, (WPARAM)g_gui_font, TRUE);
    }
    return (void*)ctrl;
}

void* sph_gui_label_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    if (!parent_hwnd) return NULL;
    wchar_t* wtext = sph_utf8_to_utf16(text ? text : "");
    HWND ctrl = CreateWindowExW(
        0,
        L"STATIC",
        wtext ? wtext : L"",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        x, y, w, h,
        (HWND)parent_hwnd,
        (HMENU)(intptr_t)id,
        GetModuleHandleW(NULL),
        NULL
    );
    if (wtext) free(wtext);
    if (ctrl && g_gui_font) {
        SendMessageW(ctrl, WM_SETFONT, (WPARAM)g_gui_font, TRUE);
    }
    return (void*)ctrl;
}

void* sph_gui_textbox_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id, int32_t multiline) {
    if (!parent_hwnd) return NULL;
    wchar_t* wtext = sph_utf8_to_utf16(text ? text : "");
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
    if (multiline) {
        style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
    }
    HWND ctrl = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        wtext ? wtext : L"",
        style,
        x, y, w, h,
        (HWND)parent_hwnd,
        (HMENU)(intptr_t)id,
        GetModuleHandleW(NULL),
        NULL
    );
    if (wtext) free(wtext);
    if (ctrl && g_gui_font) {
        SendMessageW(ctrl, WM_SETFONT, (WPARAM)g_gui_font, TRUE);
    }
    return (void*)ctrl;
}

void* sph_gui_checkbox_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    if (!parent_hwnd) return NULL;
    wchar_t* wtext = sph_utf8_to_utf16(text ? text : "");
    HWND ctrl = CreateWindowExW(
        0,
        L"BUTTON",
        wtext ? wtext : L"",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
        x, y, w, h,
        (HWND)parent_hwnd,
        (HMENU)(intptr_t)id,
        GetModuleHandleW(NULL),
        NULL
    );
    if (wtext) free(wtext);
    if (ctrl && g_gui_font) {
        SendMessageW(ctrl, WM_SETFONT, (WPARAM)g_gui_font, TRUE);
    }
    return (void*)ctrl;
}

int32_t sph_gui_checkbox_is_checked(void* ctrl_hwnd) {
    if (!ctrl_hwnd) return 0;
    LRESULT state = SendMessageW((HWND)ctrl_hwnd, BM_GETCHECK, 0, 0);
    return (state == BST_CHECKED) ? 1 : 0;
}

void sph_gui_checkbox_set_checked(void* ctrl_hwnd, int32_t checked) {
    if (!ctrl_hwnd) return;
    SendMessageW((HWND)ctrl_hwnd, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

void* sph_gui_progressbar_create(void* parent_hwnd, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    if (!parent_hwnd) return NULL;
    HWND ctrl = CreateWindowExW(
        0,
        PROGRESS_CLASSW,
        NULL,
        WS_CHILD | WS_VISIBLE,
        x, y, w, h,
        (HWND)parent_hwnd,
        (HMENU)(intptr_t)id,
        GetModuleHandleW(NULL),
        NULL
    );
    return (void*)ctrl;
}

void sph_gui_progressbar_set_pos(void* ctrl_hwnd, int32_t pos) {
    if (!ctrl_hwnd) return;
    SendMessageW((HWND)ctrl_hwnd, PBM_SETPOS, (WPARAM)pos, 0);
}

void sph_gui_progressbar_set_range(void* ctrl_hwnd, int32_t min_val, int32_t max_val) {
    if (!ctrl_hwnd) return;
    SendMessageW((HWND)ctrl_hwnd, PBM_SETRANGE32, (WPARAM)min_val, (LPARAM)max_val);
}

void* sph_gui_listview_create(void* parent_hwnd, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    if (!parent_hwnd) return NULL;
    HWND ctrl = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        x, y, w, h,
        (HWND)parent_hwnd,
        (HMENU)(intptr_t)id,
        GetModuleHandleW(NULL),
        NULL
    );
    if (ctrl) {
        ListView_SetExtendedListViewStyle(ctrl, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        if (g_gui_font) {
            SendMessageW(ctrl, WM_SETFONT, (WPARAM)g_gui_font, TRUE);
        }
    }
    return (void*)ctrl;
}

int32_t sph_gui_listview_add_item(void* ctrl_hwnd, const char* text) {
    if (!ctrl_hwnd || !text) return -1;
    wchar_t* wtext = sph_utf8_to_utf16(text);
    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount((HWND)ctrl_hwnd);
    item.pszText = wtext ? wtext : L"";
    int idx = (int)SendMessageW((HWND)ctrl_hwnd, LVM_INSERTITEMW, 0, (LPARAM)&item);
    if (wtext) free(wtext);
    return idx;
}

int32_t sph_gui_listview_get_selected(void* ctrl_hwnd) {
    if (!ctrl_hwnd) return -1;
    return (int32_t)SendMessageW((HWND)ctrl_hwnd, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
}

static char g_text_buffer[2048];

const char* sph_gui_get_text(void* ctrl_hwnd) {
    if (!ctrl_hwnd) return "";
    int len = GetWindowTextLengthW((HWND)ctrl_hwnd);
    if (len <= 0) return "";
    wchar_t* wbuf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!wbuf) return "";
    GetWindowTextW((HWND)ctrl_hwnd, wbuf, len + 1);
    char* utf8 = sph_utf16_to_utf8(wbuf);
    free(wbuf);
    if (utf8) {
        strncpy(g_text_buffer, utf8, sizeof(g_text_buffer) - 1);
        g_text_buffer[sizeof(g_text_buffer) - 1] = '\0';
        free(utf8);
        return g_text_buffer;
    }
    return "";
}

void sph_gui_set_text(void* ctrl_hwnd, const char* text) {
    if (!ctrl_hwnd) return;
    wchar_t* wtext = sph_utf8_to_utf16(text ? text : "");
    if (wtext) {
        SetWindowTextW((HWND)ctrl_hwnd, wtext);
        free(wtext);
    }
}

// ============================================================================
// Dialogs & Clipboard & Cursor
// ============================================================================

int32_t sph_gui_messagebox(void* parent_hwnd, const char* text, const char* title, int32_t utype) {
    wchar_t* wtext = sph_utf8_to_utf16(text ? text : "");
    wchar_t* wtitle = sph_utf8_to_utf16(title ? title : "SantientPHP");
    int res = MessageBoxW((HWND)parent_hwnd, wtext ? wtext : L"", wtitle ? wtitle : L"SantientPHP", utype ? utype : MB_OK);
    if (wtext) free(wtext);
    if (wtitle) free(wtitle);
    return res;
}

static char g_file_picker_buffer[MAX_PATH * 2];

const char* sph_gui_file_picker_open(void* parent_hwnd, const char* title, const char* filter) {
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = { 0 };
    wchar_t* wtitle = sph_utf8_to_utf16(title);

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)parent_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrTitle = wtitle;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Default filter if none provided
    if (filter) {
        ofn.lpstrFilter = sph_utf8_to_utf16(filter);
    } else {
        ofn.lpstrFilter = L"All Files (*.*)\0*.*\0\0";
    }

    g_file_picker_buffer[0] = '\0';
    if (GetOpenFileNameW(&ofn)) {
        char* utf8 = sph_utf16_to_utf8(szFile);
        if (utf8) {
            strncpy(g_file_picker_buffer, utf8, sizeof(g_file_picker_buffer) - 1);
            g_file_picker_buffer[sizeof(g_file_picker_buffer) - 1] = '\0';
            free(utf8);
        }
    }

    if (wtitle) free(wtitle);
    if (filter && ofn.lpstrFilter) free((void*)ofn.lpstrFilter);

    return g_file_picker_buffer;
}

const char* sph_gui_file_picker_save(void* parent_hwnd, const char* title, const char* filter) {
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = { 0 };
    wchar_t* wtitle = sph_utf8_to_utf16(title);

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)parent_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrTitle = wtitle;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (filter) {
        ofn.lpstrFilter = sph_utf8_to_utf16(filter);
    } else {
        ofn.lpstrFilter = L"All Files (*.*)\0*.*\0\0";
    }

    g_file_picker_buffer[0] = '\0';
    if (GetSaveFileNameW(&ofn)) {
        char* utf8 = sph_utf16_to_utf8(szFile);
        if (utf8) {
            strncpy(g_file_picker_buffer, utf8, sizeof(g_file_picker_buffer) - 1);
            g_file_picker_buffer[sizeof(g_file_picker_buffer) - 1] = '\0';
            free(utf8);
        }
    }

    if (wtitle) free(wtitle);
    if (filter && ofn.lpstrFilter) free((void*)ofn.lpstrFilter);

    return g_file_picker_buffer;
}

static char g_clipboard_buffer[4096];

const char* sph_gui_clipboard_get_text(void) {
    g_clipboard_buffer[0] = '\0';
    if (!OpenClipboard(NULL)) return "";
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* wtext = (wchar_t*)GlobalLock(hData);
        if (wtext) {
            char* utf8 = sph_utf16_to_utf8(wtext);
            if (utf8) {
                strncpy(g_clipboard_buffer, utf8, sizeof(g_clipboard_buffer) - 1);
                g_clipboard_buffer[sizeof(g_clipboard_buffer) - 1] = '\0';
                free(utf8);
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return g_clipboard_buffer;
}

void sph_gui_clipboard_set_text(const char* text) {
    if (!text || !OpenClipboard(NULL)) return;
    EmptyClipboard();
    wchar_t* wtext = sph_utf8_to_utf16(text);
    if (wtext) {
        size_t len = (wcslen(wtext) + 1) * sizeof(wchar_t);
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hGlob) {
            void* pGlob = GlobalLock(hGlob);
            if (pGlob) {
                memcpy(pGlob, wtext, len);
                GlobalUnlock(hGlob);
                SetClipboardData(CF_UNICODETEXT, hGlob);
            }
        }
        free(wtext);
    }
    CloseClipboard();
}

void sph_gui_set_cursor(int32_t cursor_type) {
    LPCWSTR cid = (LPCWSTR)IDC_ARROW;
    switch (cursor_type) {
        case 1: cid = (LPCWSTR)IDC_IBEAM; break;
        case 2: cid = (LPCWSTR)IDC_WAIT; break;
        case 3: cid = (LPCWSTR)IDC_CROSS; break;
        case 4: cid = (LPCWSTR)IDC_HAND; break;
        default: cid = (LPCWSTR)IDC_ARROW; break;
    }
    SetCursor(LoadCursorW(NULL, cid));
}

void sph_gui_set_wndproc_hook(void* hwnd, void* hook_fn) {
    if (hwnd) {
        SetPropW((HWND)hwnd, L"SPH_HOOK", (HANDLE)(intptr_t)hook_fn);
    }
}

void sph_gui_register_event_handler(int32_t id, sph_gui_event_callback callback, void* user_data) {
    (void)id;
    (void)callback;
    (void)user_data;
    // Store in active windows or main context
}

// Executes a shell command completely silently with zero console window popups
int32_t sph_gui_exec_hidden(const char* cmd) {
    if (!cmd || !*cmd) return -1;

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    char full_cmd[8192];
    snprintf(full_cmd, sizeof(full_cmd), "cmd.exe /c %s", cmd);

    // CREATE_NO_WINDOW = 0x08000000 ensures neither cmd nor its children create a console
    if (CreateProcessA(NULL, full_cmd, NULL, NULL, FALSE, 0x08000000, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 15000);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (int32_t)exitCode;
    }
    return -1;
}

// ============================================================================
// Runtime Entry Point Shim: WinMain
// Enables modern Common Controls v6 & Per-Monitor V2 DPI scaling automatically
// ============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // 1. Initialize Common Controls v6
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS |
                ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // 2. Set Per-Monitor V2 DPI Awareness if supported
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef BOOL (WINAPI *SetDpiContextProc)(DPI_AWARENESS_CONTEXT);
        SetDpiContextProc setDpiContext = (SetDpiContextProc)(void*)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setDpiContext) {
            setDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else {
            typedef BOOL (WINAPI *SetDpiAwareProc)(void);
            SetDpiAwareProc setDpiAware = (SetDpiAwareProc)(void*)GetProcAddress(user32, "SetProcessDPIAware");
            if (setDpiAware) {
                setDpiAware();
            }
        }
    }

    // 3. Initialize OLE/COM
    OleInitialize(NULL);

    // 4. Forward execution to compiled SantientPHP main()
    int exit_code = main(__argc, __argv);

    OleUninitialize();
    return exit_code;
}

#endif // _WIN32
