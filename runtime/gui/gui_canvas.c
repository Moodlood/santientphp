// ============================================================================
// SantientPHP (SPH) - SIMD Framebuffer Canvas & Hardware Blitting
// Tier 2: 32-bit RGBA Pixel Surface accelerated by 128-bit SIMD & OpenGL
// ============================================================================

#include "gui.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#include <GL/gl.h>
#endif

// Include x86 SIMD intrinsics if available
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <emmintrin.h> // SSE2
#define SPH_HAS_SSE2 1
#endif

sph_canvas* sph_canvas_create(void* hwnd, int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) return NULL;

    sph_canvas* canvas = (sph_canvas*)malloc(sizeof(sph_canvas));
    if (!canvas) return NULL;
    memset(canvas, 0, sizeof(sph_canvas));

    canvas->hwnd = hwnd;
    canvas->width = width;
    canvas->height = height;

#if defined(_WIN32)
    HDC screen_dc = GetDC(NULL);
    canvas->hdc_mem = CreateCompatibleDC(screen_dc);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down DIB so (0,0) is top-left
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    canvas->hbitmap = CreateDIBSection((HDC)canvas->hdc_mem, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen_dc);

    if (canvas->hbitmap && bits) {
        canvas->pixels = (uint32_t*)bits;
        canvas->old_bitmap = SelectObject((HDC)canvas->hdc_mem, (HBITMAP)canvas->hbitmap);
    } else {
        canvas->pixels = (uint32_t*)malloc(width * height * sizeof(uint32_t));
    }

    if (hwnd) {
        SetPropW((HWND)hwnd, L"SPH_CANVAS", (HANDLE)canvas);
    }
#else
    canvas->pixels = (uint32_t*)malloc(width * height * sizeof(uint32_t));
#endif

    if (canvas->pixels) {
        sph_canvas_clear(canvas, 0xFF000000); // Default black surface
    }

    return canvas;
}

void sph_canvas_destroy(sph_canvas* canvas) {
    if (!canvas) return;

#if defined(_WIN32)
    if (canvas->hwnd) {
        RemovePropW((HWND)canvas->hwnd, L"SPH_CANVAS");
    }
    if (canvas->hdc_mem && canvas->old_bitmap) {
        SelectObject((HDC)canvas->hdc_mem, (HBITMAP)canvas->old_bitmap);
    }
    if (canvas->hbitmap) {
        DeleteObject((HBITMAP)canvas->hbitmap);
    } else if (canvas->pixels) {
        free(canvas->pixels);
    }
    if (canvas->hdc_mem) {
        DeleteDC((HDC)canvas->hdc_mem);
    }
#else
    if (canvas->pixels) {
        free(canvas->pixels);
    }
#endif

    free(canvas);
}

uint32_t* sph_canvas_get_pixels(sph_canvas* canvas) {
    return canvas ? canvas->pixels : NULL;
}

void sph_canvas_clear(sph_canvas* canvas, uint32_t color) {
    if (!canvas || !canvas->pixels) return;

    size_t total_pixels = (size_t)canvas->width * (size_t)canvas->height;
    uint32_t* p = canvas->pixels;
    size_t i = 0;

#if defined(SPH_HAS_SSE2)
    __m128i color_vec = _mm_set1_epi32((int)color);
    size_t vec_count = total_pixels / 4;
    for (; i < vec_count * 4; i += 4) {
        _mm_storeu_si128((__m128i*)(p + i), color_vec);
    }
#endif

    for (; i < total_pixels; ++i) {
        p[i] = color;
    }
}

void sph_canvas_fill_rect(sph_canvas* canvas, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (!canvas || !canvas->pixels || w <= 0 || h <= 0) return;

    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = (x + w) > canvas->width ? canvas->width : (x + w);
    int32_t y1 = (y + h) > canvas->height ? canvas->height : (y + h);

    if (x0 >= x1 || y0 >= y1) return;

    int32_t span = x1 - x0;

#if defined(SPH_HAS_SSE2)
    __m128i color_vec = _mm_set1_epi32((int)color);
#endif

    for (int32_t row = y0; row < y1; ++row) {
        uint32_t* line = canvas->pixels + (row * canvas->width + x0);
        int32_t col = 0;

#if defined(SPH_HAS_SSE2)
        int32_t vec_end = (span / 4) * 4;
        for (; col < vec_end; col += 4) {
            _mm_storeu_si128((__m128i*)(line + col), color_vec);
        }
#endif

        for (; col < span; ++col) {
            line[col] = color;
        }
    }
}

void sph_canvas_draw_rect(sph_canvas* canvas, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (!canvas || !canvas->pixels || w <= 0 || h <= 0) return;
    // Top and bottom horizontal edges
    sph_canvas_fill_rect(canvas, x, y, w, 1, color);
    sph_canvas_fill_rect(canvas, x, y + h - 1, w, 1, color);
    // Left and right vertical edges
    sph_canvas_fill_rect(canvas, x, y, 1, h, color);
    sph_canvas_fill_rect(canvas, x + w - 1, y, 1, h, color);
}

// Alpha blend pixel helper (scalar)
static inline uint32_t blend_pixel(uint32_t dst, uint32_t src, uint32_t a) {
    uint32_t inv_a = 255 - a;
    uint32_t r = ((src >> 16 & 0xFF) * a + (dst >> 16 & 0xFF) * inv_a) >> 8;
    uint32_t g = ((src >> 8 & 0xFF) * a + (dst >> 8 & 0xFF) * inv_a) >> 8;
    uint32_t b = ((src & 0xFF) * a + (dst & 0xFF) * inv_a) >> 8;
    return (0xFF000000) | (r << 16) | (g << 8) | b;
}

void sph_canvas_blend_simd(sph_canvas* canvas, const uint32_t* src_pixels, int32_t src_w, int32_t src_h,
                           int32_t dst_x, int32_t dst_y, float alpha) {
    if (!canvas || !canvas->pixels || !src_pixels || src_w <= 0 || src_h <= 0) return;

    if (alpha <= 0.0f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    uint32_t a_val = (uint32_t)(alpha * 255.0f);

    for (int32_t sy = 0; sy < src_h; ++sy) {
        int32_t dy = dst_y + sy;
        if (dy < 0 || dy >= canvas->height) continue;

        for (int32_t sx = 0; sx < src_w; ++sx) {
            int32_t dx = dst_x + sx;
            if (dx < 0 || dx >= canvas->width) continue;

            uint32_t src_col = src_pixels[sy * src_w + sx];
            uint32_t* dst_ptr = &canvas->pixels[dy * canvas->width + dx];
            *dst_ptr = blend_pixel(*dst_ptr, src_col, a_val);
        }
    }
}

void sph_canvas_present(sph_canvas* canvas) {
    if (!canvas) return;

#if defined(_WIN32)
    if (canvas->hwnd && canvas->hdc_mem) {
        HDC hdc_win = GetDC((HWND)canvas->hwnd);
        if (hdc_win) {
            BitBlt(hdc_win, 0, 0, canvas->width, canvas->height,
                   (HDC)canvas->hdc_mem, 0, 0, SRCCOPY);
            ReleaseDC((HWND)canvas->hwnd, hdc_win);
        }
    }
#endif
}

// ============================================================================
// Hardware OpenGL Context Acceleration
// ============================================================================

void* sph_gl_context_create(void* hwnd) {
#if defined(_WIN32)
    if (!hwnd) return NULL;
    HDC hdc = GetDC((HWND)hwnd);
    if (!hdc) return NULL;

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixel_format = ChoosePixelFormat(hdc, &pfd);
    if (!pixel_format) {
        ReleaseDC((HWND)hwnd, hdc);
        return NULL;
    }
    SetPixelFormat(hdc, pixel_format, &pfd);

    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc) {
        wglMakeCurrent(hdc, hglrc);
    }
    ReleaseDC((HWND)hwnd, hdc);
    return (void*)hglrc;
#else
    (void)hwnd;
    return NULL;
#endif
}

void sph_gl_swap_buffers(void* hwnd) {
#if defined(_WIN32)
    if (hwnd) {
        HDC hdc = GetDC((HWND)hwnd);
        if (hdc) {
            SwapBuffers(hdc);
            ReleaseDC((HWND)hwnd, hdc);
        }
    }
#else
    (void)hwnd;
#endif
}

void sph_gl_make_current(void* hwnd, void* gl_ctx) {
#if defined(_WIN32)
    if (hwnd && gl_ctx) {
        HDC hdc = GetDC((HWND)hwnd);
        if (hdc) {
            wglMakeCurrent(hdc, (HGLRC)gl_ctx);
            ReleaseDC((HWND)hwnd, hdc);
        }
    } else {
        wglMakeCurrent(NULL, NULL);
    }
#else
    (void)hwnd;
    (void)gl_ctx;
#endif
}

