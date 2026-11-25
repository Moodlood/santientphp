// ============================================================================
// SantientPHP (SPH) - SIMD Framebuffer Canvas & Hardware Blitting
// Tier 2: 32-bit RGBA Pixel Surface accelerated by 128-bit SIMD & OpenGL
// ============================================================================

#include "gui.h"
#include <stdio.h>
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


void sph_canvas_apply_lighting_simd(sph_canvas* canvas, const sph_light_c* lights, int32_t light_count,
                                    float ambient_r, float ambient_g, float ambient_b) {
    if (!canvas || !canvas->pixels || canvas->width <= 0 || canvas->height <= 0) return;

    int total_pixels = canvas->width * canvas->height;
    uint32_t* p = canvas->pixels;

    // Fast lightmap accumulator
    static float* lightmap_r = NULL;
    static float* lightmap_g = NULL;
    static float* lightmap_b = NULL;
    static int lm_size = 0;

    if (lm_size != total_pixels) {
        if (lightmap_r) free(lightmap_r);
        if (lightmap_g) free(lightmap_g);
        if (lightmap_b) free(lightmap_b);
        lightmap_r = (float*)malloc(total_pixels * sizeof(float));
        lightmap_g = (float*)malloc(total_pixels * sizeof(float));
        lightmap_b = (float*)malloc(total_pixels * sizeof(float));
        lm_size = total_pixels;
    }

    // Initialize with ambient
    for (int i = 0; i < total_pixels; ++i) {
        lightmap_r[i] = ambient_r;
        lightmap_g[i] = ambient_g;
        lightmap_b[i] = ambient_b;
    }

    // Accumulate each light source
    for (int l = 0; l < light_count; ++l) {
        const sph_light_c* light = &lights[l];
        float rad = light->radius;
        if (rad <= 0.0f) continue;
        float rad_sq = rad * rad;
        float inv_rad_sq = 1.0f / rad_sq;

        int min_x = (int)(light->x - rad);
        int max_x = (int)(light->x + rad + 1.0f);
        int min_y = (int)(light->y - rad);
        int max_y = (int)(light->y + rad + 1.0f);

        if (min_x < 0) min_x = 0;
        if (max_x > canvas->width) max_x = canvas->width;
        if (min_y < 0) min_y = 0;
        if (max_y > canvas->height) max_y = canvas->height;

        for (int y = min_y; y < max_y; ++y) {
            float dy = (float)y - light->y;
            float dy_sq = dy * dy;
            int row_idx = y * canvas->width;

            for (int x = min_x; x < max_x; ++x) {
                float dx = (float)x - light->x;
                float d_sq = dx * dx + dy_sq;
                if (d_sq < rad_sq) {
                    float att = (1.0f - d_sq * inv_rad_sq) * light->intensity;
                    int idx = row_idx + x;
                    lightmap_r[idx] += light->r * att;
                    lightmap_g[idx] += light->g * att;
                    lightmap_b[idx] += light->b * att;
                }
            }
        }
    }

    // Apply lighting modulation to canvas pixels (SIMD/unrolled)
    for (int i = 0; i < total_pixels; ++i) {
        uint32_t px = p[i];
        float lr = lightmap_r[i];
        float lg = lightmap_g[i];
        float lb = lightmap_b[i];
        if (lr > 1.0f) lr = 1.0f;
        if (lg > 1.0f) lg = 1.0f;
        if (lb > 1.0f) lb = 1.0f;

        uint32_t r = (uint32_t)((px >> 16 & 0xFF) * lr);
        uint32_t g = (uint32_t)((px >> 8 & 0xFF) * lg);
        uint32_t b = (uint32_t)((px & 0xFF) * lb);

        p[i] = (px & 0xFF000000) | (r << 16) | (g << 8) | b;
    }
}

void sph_canvas_present(sph_canvas* canvas) {
    if (!canvas) return;

#if defined(_WIN32)
    if (canvas->hwnd && canvas->hdc_mem) {
        HDC hdc_win = GetDC((HWND)canvas->hwnd);
        if (hdc_win) {
            RECT rc;
            if (GetClientRect((HWND)canvas->hwnd, &rc)) {
                int win_w = rc.right - rc.left;
                int win_h = rc.bottom - rc.top;

                if (win_w <= 0 || win_h <= 0) {
                    ReleaseDC((HWND)canvas->hwnd, hdc_win);
                    return;
                }

                if (win_w == canvas->width && win_h == canvas->height) {
                    BitBlt(hdc_win, 0, 0, canvas->width, canvas->height,
                           (HDC)canvas->hdc_mem, 0, 0, SRCCOPY);
                } else {
                    float scale_x = (float)win_w / (float)canvas->width;
                    float scale_y = (float)win_h / (float)canvas->height;
                    float scale = scale_x < scale_y ? scale_x : scale_y;

                    int dst_w = (int)(canvas->width * scale);
                    int dst_h = (int)(canvas->height * scale);
                    int dst_x = (win_w - dst_w) / 2;
                    int dst_y = (win_h - dst_h) / 2;

                    SetStretchBltMode(hdc_win, COLORONCOLOR);

                    if (dst_x > 0) {
                        RECT left_bar = { 0, 0, dst_x, win_h };
                        RECT right_bar = { dst_x + dst_w, 0, win_w, win_h };
                        FillRect(hdc_win, &left_bar, (HBRUSH)GetStockObject(BLACK_BRUSH));
                        FillRect(hdc_win, &right_bar, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    }
                    if (dst_y > 0) {
                        RECT top_bar = { 0, 0, win_w, dst_y };
                        RECT bottom_bar = { 0, dst_y + dst_h, win_w, win_h };
                        FillRect(hdc_win, &top_bar, (HBRUSH)GetStockObject(BLACK_BRUSH));
                        FillRect(hdc_win, &bottom_bar, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    }

                    StretchBlt(hdc_win, dst_x, dst_y, dst_w, dst_h,
                               (HDC)canvas->hdc_mem, 0, 0, canvas->width, canvas->height, SRCCOPY);
                }
            }
            ReleaseDC((HWND)canvas->hwnd, hdc_win);
        }
    }
#elif defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))
    sph_x11_canvas_present(canvas);
#endif
}

void sph_canvas_map_mouse(sph_canvas* canvas, int32_t client_x, int32_t client_y, int32_t* out_x, int32_t* out_y) {
    if (!canvas || !out_x || !out_y) return;
#if defined(_WIN32)
    if (canvas->hwnd) {
        RECT rc;
        if (GetClientRect((HWND)canvas->hwnd, &rc)) {
            int win_w = rc.right - rc.left;
            int win_h = rc.bottom - rc.top;
            if (win_w > 0 && win_h > 0 && (win_w != canvas->width || win_h != canvas->height)) {
                float scale_x = (float)win_w / (float)canvas->width;
                float scale_y = (float)win_h / (float)canvas->height;
                float scale = scale_x < scale_y ? scale_x : scale_y;
                int dst_w = (int)(canvas->width * scale);
                int dst_h = (int)(canvas->height * scale);
                int dst_x = (win_w - dst_w) / 2;
                int dst_y = (win_h - dst_h) / 2;

                int cx = (int)((client_x - dst_x) / scale);
                int cy = (int)((client_y - dst_y) / scale);
                if (cx < 0) cx = 0;
                if (cx >= canvas->width) cx = canvas->width - 1;
                if (cy < 0) cy = 0;
                if (cy >= canvas->height) cy = canvas->height - 1;
                *out_x = cx;
                *out_y = cy;
                return;
            }
        }
    }
#elif defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))
    if (canvas->hwnd) {
        int32_t win_w = 0, win_h = 0;
        if (sph_x11_get_window_size(canvas->hwnd, &win_w, &win_h) && win_w > 0 && win_h > 0) {
            int dst_x = (win_w > canvas->width) ? (win_w - canvas->width) / 2 : 0;
            int dst_y = (win_h > canvas->height) ? (win_h - canvas->height) / 2 : 0;
            int cx = client_x - dst_x;
            int cy = client_y - dst_y;
            if (cx < 0) cx = 0;
            if (cx >= canvas->width) cx = canvas->width - 1;
            if (cy < 0) cy = 0;
            if (cy >= canvas->height) cy = canvas->height - 1;
            *out_x = cx;
            *out_y = cy;
            return;
        }
    }
#endif
    *out_x = client_x;
    *out_y = client_y;
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
#elif defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))
    return sph_x11_gl_context_create(hwnd);
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
#elif defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))
    sph_x11_gl_swap_buffers(hwnd);
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
#elif defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))
    sph_x11_gl_make_current(hwnd, gl_ctx);
#else
    (void)hwnd;
    (void)gl_ctx;
#endif
}



#include "bigfoot_data.h"

void sph_canvas_inject_bigfoot(uint32_t* atlas_pixels, int32_t atlas_w, int32_t atlas_h, int32_t dst_x, int32_t dst_y) {
    if (!atlas_pixels || atlas_w <= 0 || atlas_h <= 0) return;

    // 1. Try loading external bigfoot.bmp if available
    int loaded = 0;
    FILE* f = fopen("bigfoot.bmp", "rb");
    if (!f) f = fopen("bin/bigfoot.bmp", "rb");

    if (f) {
        uint8_t header[54];
        if (fread(header, 1, 54, f) == 54) {
            uint32_t data_offset = *(uint32_t*)&header[10];
            int32_t bmp_w = *(int32_t*)&header[18];
            int32_t bmp_h = *(int32_t*)&header[22];
            uint16_t bpp = *(uint16_t*)&header[28];
            int is_top_down = (bmp_h < 0);
            int abs_h = is_top_down ? -bmp_h : bmp_h;

            if (bmp_w == BIGFOOT_SHEET_W && abs_h == BIGFOOT_SHEET_H && (bpp == 32 || bpp == 24)) {
                fseek(f, data_offset, SEEK_SET);
                int row_size = ((bmp_w * (bpp / 8) + 3) / 4) * 4;
                uint8_t* row_buf = (uint8_t*)malloc(row_size);
                if (row_buf) {
                    for (int y = 0; y < abs_h; ++y) {
                        int src_y = is_top_down ? y : (abs_h - 1 - y);
                        if (fread(row_buf, 1, row_size, f) != (size_t)row_size) break;
                        for (int x = 0; x < bmp_w; ++x) {
                            uint32_t pixel = 0;
                            if (bpp == 32) {
                                uint8_t b = row_buf[x * 4 + 0];
                                uint8_t g = row_buf[x * 4 + 1];
                                uint8_t r = row_buf[x * 4 + 2];
                                uint8_t a = row_buf[x * 4 + 3];
                                pixel = (a << 24) | (r << 16) | (g << 8) | b;
                            } else {
                                uint8_t b = row_buf[x * 3 + 0];
                                uint8_t g = row_buf[x * 3 + 1];
                                uint8_t r = row_buf[x * 3 + 2];
                                if (r > 240 && g > 240 && b > 240) {
                                    pixel = 0;
                                } else {
                                    pixel = (0xFF << 24) | (r << 16) | (g << 8) | b;
                                }
                            }
                            int ax = dst_x + x;
                            int ay = dst_y + src_y;
                            if (ax >= 0 && ax < atlas_w && ay >= 0 && ay < atlas_h) {
                                atlas_pixels[ay * atlas_w + ax] = pixel;
                            }
                        }
                    }
                    free(row_buf);
                    loaded = 1;
                }
            }
        }
        fclose(f);
    }

    // 2. Fallback: unpack embedded sprite sheet from bigfoot_data.h
    if (!loaded) {
        for (int y = 0; y < BIGFOOT_SHEET_H; ++y) {
            for (int x = 0; x < BIGFOOT_SHEET_W; ++x) {
                int ax = dst_x + x;
                int ay = dst_y + y;
                if (ax >= 0 && ax < atlas_w && ay >= 0 && ay < atlas_h) {
                    atlas_pixels[ay * atlas_w + ax] = s_bigfoot_pixels[y * BIGFOOT_SHEET_W + x];
                }
            }
        }
    }
}

