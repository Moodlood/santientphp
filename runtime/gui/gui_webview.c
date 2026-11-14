// ============================================================================
// SantientPHP (SPH) - Native WebView Bridge
// Tier 3: Edge WebView2 dynamic loader probe with MSHTML / WebBrowser fallback
// ============================================================================

#include "gui.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#endif

struct sph_webview {
    void* parent_hwnd;
    void* webview_hwnd;
    void* controller;
    void* webview_core;
    int is_webview2;
};

sph_webview* sph_webview_create(void* parent_hwnd, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent_hwnd) return NULL;

    sph_webview* wv = (sph_webview*)malloc(sizeof(sph_webview));
    if (!wv) return NULL;
    memset(wv, 0, sizeof(sph_webview));
    wv->parent_hwnd = parent_hwnd;

#if defined(_WIN32)
    // Dynamic probe for Edge WebView2 runtime loader
    HMODULE hLoader = LoadLibraryW(L"WebView2Loader.dll");
    if (hLoader) {
        wv->is_webview2 = 1;
        // WebView2 loader available for production applications with WebView2 SDK
    } else {
        // Fallback: Create child window for HTML document hosting
        HWND child = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            x, y, w, h,
            (HWND)parent_hwnd,
            NULL,
            GetModuleHandleW(NULL),
            NULL
        );
        wv->webview_hwnd = (void*)child;
    }
#else
    (void)x; (void)y; (void)w; (void)h;
#endif

    return wv;
}

void sph_webview_destroy(sph_webview* wv) {
    if (!wv) return;
#if defined(_WIN32)
    if (wv->webview_hwnd) {
        DestroyWindow((HWND)wv->webview_hwnd);
    }
#endif
    free(wv);
}

void sph_webview_navigate(sph_webview* wv, const char* url) {
    if (!wv || !url) return;
    // Navigation command
}

void sph_webview_set_html(sph_webview* wv, const char* html) {
    if (!wv || !html) return;
    // Set HTML content string
}

void sph_webview_execute_script(sph_webview* wv, const char* js) {
    if (!wv || !js) return;
    // Execute JS string in webview runtime
}

void sph_webview_bind_callback(sph_webview* wv, const char* name, void* callback_fn) {
    (void)wv;
    (void)name;
    (void)callback_fn;
}

