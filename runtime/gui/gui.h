#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Core GUI Types and Geometry
// ============================================================================

typedef struct sph_gui_point {
    int32_t x;
    int32_t y;
} sph_gui_point;

typedef struct sph_gui_rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} sph_gui_rect;

// 32-bit RGBA SIMD Framebuffer Canvas Surface
typedef struct sph_canvas {
    void* hwnd;
    int32_t width;
    int32_t height;
    uint32_t* pixels;  // Contiguous RGBA 32-bit pixel memory buffer
    void* hdc_mem;     // Memory device context
    void* hbitmap;     // DIBSection bitmap handle
    void* old_bitmap;  // Previous selected bitmap handle
} sph_canvas;

// Event structure exposed to SantientPHP across all 3 modes
// (matches #[repr(C)] struct GuiEvent in stdlib/gui.sph)
typedef struct sph_gui_event {
    int32_t event_type; // 1=click, 2=resize, 3=key, 4=mouse, 5=close, 6=paint, 7=command
    void* hwnd;
    int32_t id;
    int64_t param1;
    int64_t param2;
} sph_gui_event;

typedef void (*sph_gui_event_callback)(sph_gui_event* event, void* user_data);
typedef void (*sph_gui_task_fn)(void* arg);

// ============================================================================
// Window Management API
// ============================================================================

void* sph_gui_window_create(const char* title, int32_t width, int32_t height);
void sph_gui_window_show(void* hwnd, int32_t show);
void sph_gui_window_set_title(void* hwnd, const char* title);
void sph_gui_window_get_size(void* hwnd, int32_t* out_w, int32_t* out_h);
void sph_gui_window_set_size(void* hwnd, int32_t width, int32_t height);
void sph_gui_window_close(void* hwnd);
int32_t sph_gui_window_is_open(void* hwnd);
void sph_gui_window_destroy(void* hwnd);

// ============================================================================
// Native Widget Primitives
// ============================================================================

void* sph_gui_button_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id);
void* sph_gui_label_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id);
void* sph_gui_textbox_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id, int32_t multiline);
void* sph_gui_checkbox_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id);
int32_t sph_gui_checkbox_is_checked(void* ctrl_hwnd);
void sph_gui_checkbox_set_checked(void* ctrl_hwnd, int32_t checked);
void* sph_gui_progressbar_create(void* parent_hwnd, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id);
void sph_gui_progressbar_set_pos(void* ctrl_hwnd, int32_t pos);
void sph_gui_progressbar_set_range(void* ctrl_hwnd, int32_t min_val, int32_t max_val);
void* sph_gui_listview_create(void* parent_hwnd, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id);
int32_t sph_gui_listview_add_item(void* ctrl_hwnd, const char* text);
int32_t sph_gui_listview_get_selected(void* ctrl_hwnd);
const char* sph_gui_get_text(void* ctrl_hwnd);
void sph_gui_set_text(void* ctrl_hwnd, const char* text);

// ============================================================================
// Dialogs & OS Clipboard / Cursor
// ============================================================================

int32_t sph_gui_messagebox(void* parent_hwnd, const char* text, const char* title, int32_t utype);
const char* sph_gui_file_picker_open(void* parent_hwnd, const char* title, const char* filter);
const char* sph_gui_file_picker_save(void* parent_hwnd, const char* title, const char* filter);
const char* sph_gui_clipboard_get_text(void);
void sph_gui_clipboard_set_text(const char* text);
void sph_gui_set_cursor(int32_t cursor_type);

// ============================================================================
// Tier 2: SIMD Framebuffer Canvas & Hardware Blitting
// ============================================================================

sph_canvas* sph_canvas_create(void* hwnd, int32_t width, int32_t height);
void sph_canvas_destroy(sph_canvas* canvas);
uint32_t* sph_canvas_get_pixels(sph_canvas* canvas);
void sph_canvas_clear(sph_canvas* canvas, uint32_t color);
void sph_canvas_draw_rect(sph_canvas* canvas, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void sph_canvas_fill_rect(sph_canvas* canvas, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void sph_canvas_blend_simd(sph_canvas* canvas, const uint32_t* src_pixels, int32_t src_w, int32_t src_h, int32_t dst_x, int32_t dst_y, float alpha);
void sph_canvas_present(sph_canvas* canvas);

// Hardware OpenGL Surface Integration
void* sph_gl_context_create(void* hwnd);
void sph_gl_swap_buffers(void* hwnd);
void sph_gl_make_current(void* hwnd, void* gl_ctx);

// ============================================================================
// Tier 3: Native WebView Bridge
// ============================================================================

typedef struct sph_webview sph_webview;

sph_webview* sph_webview_create(void* parent_hwnd, int32_t x, int32_t y, int32_t w, int32_t h);
void sph_webview_destroy(sph_webview* wv);
void sph_webview_navigate(sph_webview* wv, const char* url);
void sph_webview_set_html(sph_webview* wv, const char* html);
void sph_webview_execute_script(sph_webview* wv, const char* js);
void sph_webview_bind_callback(sph_webview* wv, const char* name, void* callback_fn);

// ============================================================================
// Non-Blocking Message Pump, Fiber EventLoop & Thread-Safe UI Dispatch
// ============================================================================

int32_t sph_gui_pump_events(void);
void sph_gui_run_event_loop(void);
void sph_gui_quit(void);
int32_t sph_gui_is_running(void);
int32_t sph_gui_exec_hidden(const char* cmd);

// Thread-safe dispatch from Thread::spawn workers to UI thread
void sph_gui_post_to_main(sph_gui_task_fn fn, void* arg);
void sph_gui_dispatch_posted_tasks(void);

// Event registration and custom hooks
void sph_gui_register_event_handler(int32_t id, sph_gui_event_callback callback, void* user_data);
void sph_gui_set_wndproc_hook(void* hwnd, void* hook_fn);

#ifdef __cplusplus
}
#endif

