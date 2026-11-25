// ============================================================================
// SantientPHP (SPH) - Linux Native X11 / GLX / Epoll GUI Platform Layer
// Zero third-party UI toolkits: pure libX11, libGL (GLX), epoll + eventfd.
// ============================================================================

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include "gui.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(SPH_HAS_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#if defined(SPH_HAS_GLX)
#include <GL/gl.h>
#include <GL/glx.h>
#endif
#endif

typedef enum {
    SPH_X11_WINDOW = 1,
    SPH_X11_BUTTON = 2,
    SPH_X11_LABEL = 3,
    SPH_X11_TEXTBOX = 4,
    SPH_X11_CHECKBOX = 5,
    SPH_X11_PROGRESSBAR = 6,
    SPH_X11_LISTVIEW = 7
} SphX11Kind;

#define SPH_X11_MAX_ITEMS 256
#define SPH_X11_MAX_WIDGETS 1024
#define SPH_X11_MAX_HANDLERS 128

typedef struct SphX11Widget {
    SphX11Kind kind;
    int32_t id;
    int32_t x, y, w, h;
    int32_t visible;
    int32_t open;
    int32_t checked;
    int32_t pushed;
    int32_t multiline;
    int32_t progress_min;
    int32_t progress_max;
    int32_t progress_pos;
    int32_t list_selected;
    int32_t list_count;
    char* list_items[SPH_X11_MAX_ITEMS];
    char* text;
    struct SphX11Widget* parent;
    sph_canvas* canvas;
    sph_gui_event_callback hook_cb;
    void* hook_user_data;
#if defined(SPH_HAS_X11)
    Window xwin;
    GC xgc;
#if defined(SPH_HAS_GLX)
    GLXContext glx_ctx;
#endif
#endif
} SphX11Widget;

typedef struct SphX11HandlerEntry {
    int32_t used;
    int32_t id;
    sph_gui_event_callback cb;
    void* user_data;
} SphX11HandlerEntry;

static pthread_mutex_t g_x11_lock = PTHREAD_MUTEX_INITIALIZER;
static SphX11Widget* g_x11_widgets[SPH_X11_MAX_WIDGETS];
static int32_t g_x11_widget_count = 0;
static SphX11HandlerEntry g_x11_handlers[SPH_X11_MAX_HANDLERS];

static int g_x11_epoll_fd = -1;
static int g_x11_wake_fd = -1;
static int g_x11_init_attempted = 0;

static char g_x11_text_ret[4096];
static char g_x11_clipboard_text[65536];
static char g_x11_picker_ret[4096];

#if defined(SPH_HAS_X11)
static Display* g_x11_dpy = NULL;
static int g_x11_screen = 0;
static Atom g_atom_wm_delete = None;
static Atom g_atom_clipboard = None;
static Atom g_atom_utf8 = None;
static Atom g_atom_targets = None;
static Atom g_atom_wm_state = None;
static Atom g_atom_fullscreen = None;
static Window g_x11_clip_owner = None;
#endif

static char* sph_x11_strdup(const char* s) {
    if (!s) s = "";
    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);
    if (out) memcpy(out, s, len + 1);
    return out;
}

static void sph_x11_ensure_epoll(void) {
    if (g_x11_wake_fd < 0) {
        g_x11_wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    }
    if (g_x11_epoll_fd < 0) {
        g_x11_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (g_x11_epoll_fd >= 0 && g_x11_wake_fd >= 0) {
            struct epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.events = EPOLLIN;
            ev.data.fd = g_x11_wake_fd;
            epoll_ctl(g_x11_epoll_fd, EPOLL_CTL_ADD, g_x11_wake_fd, &ev);
        }
    }
}

static void sph_x11_ensure_display(void) {
    sph_x11_ensure_epoll();
#if defined(SPH_HAS_X11)
    if (!g_x11_init_attempted) {
        g_x11_init_attempted = 1;
        XInitThreads();
        const char* dispEnv = getenv("DISPLAY");
        if (dispEnv && *dispEnv) {
            g_x11_dpy = XOpenDisplay(dispEnv);
        }
        if (g_x11_dpy) {
            g_x11_screen = DefaultScreen(g_x11_dpy);
            g_atom_wm_delete = XInternAtom(g_x11_dpy, "WM_DELETE_WINDOW", False);
            g_atom_clipboard = XInternAtom(g_x11_dpy, "CLIPBOARD", False);
            g_atom_utf8 = XInternAtom(g_x11_dpy, "UTF8_STRING", False);
            g_atom_targets = XInternAtom(g_x11_dpy, "TARGETS", False);
            g_atom_wm_state = XInternAtom(g_x11_dpy, "_NET_WM_STATE", False);
            g_atom_fullscreen = XInternAtom(g_x11_dpy, "_NET_WM_STATE_FULLSCREEN", False);
            g_x11_clip_owner = XCreateSimpleWindow(
                g_x11_dpy, RootWindow(g_x11_dpy, g_x11_screen), 0, 0, 1, 1, 0, 0, 0);

            int xfd = ConnectionNumber(g_x11_dpy);
            if (g_x11_epoll_fd >= 0 && xfd >= 0) {
                struct epoll_event ev;
                memset(&ev, 0, sizeof(ev));
                ev.events = EPOLLIN;
                ev.data.fd = xfd;
                epoll_ctl(g_x11_epoll_fd, EPOLL_CTL_ADD, xfd, &ev);
            }
        }
    }
#else
    g_x11_init_attempted = 1;
#endif
}

static void sph_x11_wake_event_loop(void) {
    if (g_x11_wake_fd >= 0) {
        uint64_t val = 1;
        ssize_t n = write(g_x11_wake_fd, &val, sizeof(val));
        (void)n;
    }
}

static void sph_x11_register_widget(SphX11Widget* w) {
    pthread_mutex_lock(&g_x11_lock);
    if (g_x11_widget_count < SPH_X11_MAX_WIDGETS) {
        g_x11_widgets[g_x11_widget_count++] = w;
    }
    pthread_mutex_unlock(&g_x11_lock);
}

static void sph_x11_unregister_widget(SphX11Widget* w) {
    pthread_mutex_lock(&g_x11_lock);
    for (int32_t i = 0; i < g_x11_widget_count; ++i) {
        if (g_x11_widgets[i] == w) {
            g_x11_widgets[i] = g_x11_widgets[--g_x11_widget_count];
            break;
        }
    }
    pthread_mutex_unlock(&g_x11_lock);
}

#if defined(SPH_HAS_X11)
static SphX11Widget* sph_x11_find_by_xwin(Window xwin) {
    SphX11Widget* found = NULL;
    pthread_mutex_lock(&g_x11_lock);
    for (int32_t i = 0; i < g_x11_widget_count; ++i) {
        if (g_x11_widgets[i] && g_x11_widgets[i]->xwin == xwin) {
            found = g_x11_widgets[i];
            break;
        }
    }
    pthread_mutex_unlock(&g_x11_lock);
    return found;
}
#endif

static void sph_x11_dispatch_event(SphX11Widget* target, int32_t type, int32_t id, int64_t p1, int64_t p2) {
    sph_gui_event ev;
    ev.event_type = type;
    ev.hwnd = (void*)target;
    ev.id = id;
    ev.param1 = p1;
    ev.param2 = p2;

    if (target && target->hook_cb) {
        target->hook_cb(&ev, target->hook_user_data);
    }
    for (int32_t i = 0; i < SPH_X11_MAX_HANDLERS; ++i) {
        if (g_x11_handlers[i].used && g_x11_handlers[i].cb &&
            (g_x11_handlers[i].id == id || g_x11_handlers[i].id == 0)) {
            g_x11_handlers[i].cb(&ev, g_x11_handlers[i].user_data);
        }
    }
}

#if defined(SPH_HAS_X11)
static void sph_x11_draw_widget(SphX11Widget* w) {
    if (!g_x11_dpy || !w || !w->xwin || !w->xgc) return;
    if (w->kind == SPH_X11_WINDOW) {
        if (w->canvas) {
            sph_canvas_present(w->canvas);
        }
        return;
    }

    unsigned long bg = w->pushed ? 0xD0D8E8UL : 0xF0F0F0UL;
    if (w->kind == SPH_X11_TEXTBOX || w->kind == SPH_X11_LISTVIEW || w->kind == SPH_X11_PROGRESSBAR) {
        bg = 0xFFFFFFUL;
    }
    XSetForeground(g_x11_dpy, w->xgc, bg);
    XFillRectangle(g_x11_dpy, w->xwin, w->xgc, 0, 0, (unsigned)w->w, (unsigned)w->h);

    if (w->kind != SPH_X11_LABEL) {
        XSetForeground(g_x11_dpy, w->xgc, 0x888888UL);
        XDrawRectangle(g_x11_dpy, w->xwin, w->xgc, 0, 0, (unsigned)(w->w > 1 ? w->w - 1 : 1),
                       (unsigned)(w->h > 1 ? w->h - 1 : 1));
    }

    if (w->kind == SPH_X11_CHECKBOX) {
        XSetForeground(g_x11_dpy, w->xgc, 0x222222UL);
        int box_y = (w->h > 16) ? (w->h - 14) / 2 : 1;
        XDrawRectangle(g_x11_dpy, w->xwin, w->xgc, 4, box_y, 14, 14);
        if (w->checked) {
            XFillRectangle(g_x11_dpy, w->xwin, w->xgc, 7, box_y + 3, 9, 9);
        }
        if (w->text) {
            XDrawString(g_x11_dpy, w->xwin, w->xgc, 24, w->h / 2 + 5, w->text, (int)strlen(w->text));
        }
    } else if (w->kind == SPH_X11_PROGRESSBAR) {
        int32_t range = w->progress_max - w->progress_min;
        if (range > 0 && w->w > 4 && w->h > 4) {
            int32_t clamped = w->progress_pos - w->progress_min;
            if (clamped < 0) clamped = 0;
            if (clamped > range) clamped = range;
            unsigned fill_w = (unsigned)(((int64_t)(w->w - 4) * clamped) / range);
            XSetForeground(g_x11_dpy, w->xgc, 0x2E8B57UL);
            XFillRectangle(g_x11_dpy, w->xwin, w->xgc, 2, 2, fill_w, (unsigned)(w->h - 4));
        }
    } else if (w->kind == SPH_X11_LISTVIEW) {
        int row_h = 20;
        for (int32_t i = 0; i < w->list_count; ++i) {
            int y = 4 + i * row_h;
            if (y > w->h) break;
            if (i == w->list_selected) {
                XSetForeground(g_x11_dpy, w->xgc, 0x3366CCUL);
                XFillRectangle(g_x11_dpy, w->xwin, w->xgc, 2, y, (unsigned)(w->w - 4), (unsigned)(row_h - 2));
                XSetForeground(g_x11_dpy, w->xgc, 0xFFFFFFUL);
            } else {
                XSetForeground(g_x11_dpy, w->xgc, 0x111111UL);
            }
            if (w->list_items[i]) {
                XDrawString(g_x11_dpy, w->xwin, w->xgc, 8, y + 14, w->list_items[i], (int)strlen(w->list_items[i]));
            }
        }
    } else if (w->text && *w->text) {
        XSetForeground(g_x11_dpy, w->xgc, 0x111111UL);
        int text_x = (w->kind == SPH_X11_BUTTON) ? (w->w > 40 ? 12 : 4) : 6;
        XDrawString(g_x11_dpy, w->xwin, w->xgc, text_x, w->h / 2 + 5, w->text, (int)strlen(w->text));
    }
}
#endif

static SphX11Widget* sph_x11_create_widget(SphX11Kind kind, void* parent_ptr, const char* text,
                                           int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    sph_x11_ensure_display();
    SphX11Widget* parent = (SphX11Widget*)parent_ptr;
    SphX11Widget* node = (SphX11Widget*)calloc(1, sizeof(SphX11Widget));
    if (!node) return NULL;

    node->kind = kind;
    node->id = id;
    node->x = x;
    node->y = y;
    node->w = w > 0 ? w : 100;
    node->h = h > 0 ? h : 28;
    node->open = 1;
    node->visible = (kind != SPH_X11_WINDOW) ? 1 : 0;
    node->progress_max = 100;
    node->list_selected = -1;
    node->parent = parent;
    node->text = sph_x11_strdup(text ? text : "");

#if defined(SPH_HAS_X11)
    if (g_x11_dpy) {
        Window xparent = (parent && parent->xwin) ? parent->xwin : RootWindow(g_x11_dpy, g_x11_screen);
        unsigned long fg = BlackPixel(g_x11_dpy, g_x11_screen);
        unsigned long bg = (kind == SPH_X11_WINDOW) ? 0x1E1E1EUL : 0xF0F0F0UL;
        node->xwin = XCreateSimpleWindow(g_x11_dpy, xparent, x, y, (unsigned)node->w, (unsigned)node->h,
                                         kind == SPH_X11_WINDOW ? 0 : 1, fg, bg);
        if (node->xwin) {
            node->xgc = XCreateGC(g_x11_dpy, node->xwin, 0, NULL);
            long mask = ExposureMask | StructureNotifyMask | KeyPressMask | ButtonPressMask |
                        ButtonReleaseMask | PointerMotionMask | LeaveWindowMask;
            XSelectInput(g_x11_dpy, node->xwin, mask);
            if (kind == SPH_X11_WINDOW) {
                XStoreName(g_x11_dpy, node->xwin, node->text);
                if (g_atom_wm_delete != None) {
                    XSetWMProtocols(g_x11_dpy, node->xwin, &g_atom_wm_delete, 1);
                }
            } else if (parent && parent->xwin) {
                XMapWindow(g_x11_dpy, node->xwin);
            }
            XFlush(g_x11_dpy);
        }
    }
#endif

    sph_x11_register_widget(node);
    return node;
}

// ============================================================================
// Window Management API
// ============================================================================

void* sph_gui_window_create(const char* title, int32_t width, int32_t height) {
    return (void*)sph_x11_create_widget(SPH_X11_WINDOW, NULL, title ? title : "SantientPHP Window",
                                        100, 100, width > 0 ? width : 640, height > 0 ? height : 480, 0);
}

void sph_gui_window_show(void* hwnd, int32_t show) {
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (!w) return;
    w->visible = show ? 1 : 0;
#if defined(SPH_HAS_X11)
    if (g_x11_dpy && w->xwin) {
        if (show) {
            XMapRaised(g_x11_dpy, w->xwin);
            XMapSubwindows(g_x11_dpy, w->xwin);
        } else {
            XUnmapWindow(g_x11_dpy, w->xwin);
        }
        XFlush(g_x11_dpy);
    }
#endif
}

void sph_gui_window_set_title(void* hwnd, const char* title) {
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (!w) return;
    free(w->text);
    w->text = sph_x11_strdup(title ? title : "");
#if defined(SPH_HAS_X11)
    if (g_x11_dpy && w->xwin) {
        XStoreName(g_x11_dpy, w->xwin, w->text);
        XFlush(g_x11_dpy);
    }
#endif
}

void sph_gui_window_get_size(void* hwnd, int32_t* out_w, int32_t* out_h) {
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (!w) return;
#if defined(SPH_HAS_X11)
    if (g_x11_dpy && w->xwin) {
        XWindowAttributes attrs;
        if (XGetWindowAttributes(g_x11_dpy, w->xwin, &attrs)) {
            w->w = attrs.width;
            w->h = attrs.height;
        }
    }
#endif
    if (out_w) *out_w = w->w;
    if (out_h) *out_h = w->h;
}

void sph_gui_window_set_size(void* hwnd, int32_t width, int32_t height) {
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (!w || width <= 0 || height <= 0) return;
    w->w = width;
    w->h = height;
#if defined(SPH_HAS_X11)
    if (g_x11_dpy && w->xwin) {
        XResizeWindow(g_x11_dpy, w->xwin, (unsigned)width, (unsigned)height);
        XFlush(g_x11_dpy);
    }
#endif
}

void sph_gui_window_close(void* hwnd) {
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (!w) return;
    w->open = 0;
#if defined(SPH_HAS_X11)
    if (g_x11_dpy && w->xwin) {
        XUnmapWindow(g_x11_dpy, w->xwin);
        XFlush(g_x11_dpy);
    }
#endif
}

int32_t sph_gui_window_is_open(void* hwnd) {
    SphX11Widget* w = (SphX11Widget*)hwnd;
    return (w && w->open) ? 1 : 0;
}

void sph_gui_window_destroy(void* hwnd) {
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (!w) return;
    w->open = 0;
    sph_x11_unregister_widget(w);
#if defined(SPH_HAS_X11)
    if (g_x11_dpy) {
#if defined(SPH_HAS_GLX)
        if (w->glx_ctx) {
            if (glXGetCurrentContext() == w->glx_ctx) {
                glXMakeCurrent(g_x11_dpy, None, NULL);
            }
            glXDestroyContext(g_x11_dpy, w->glx_ctx);
            w->glx_ctx = NULL;
        }
#endif
        if (w->xgc) XFreeGC(g_x11_dpy, w->xgc);
        if (w->xwin) XDestroyWindow(g_x11_dpy, w->xwin);
        XFlush(g_x11_dpy);
    }
#endif
    for (int32_t i = 0; i < w->list_count; ++i) {
        free(w->list_items[i]);
    }
    free(w->text);
    free(w);
}

void sph_gui_window_toggle_fullscreen(void* hwnd) {
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (!w) return;
#if defined(SPH_HAS_X11)
    if (g_x11_dpy && w->xwin && g_atom_wm_state != None && g_atom_fullscreen != None) {
        XEvent xev;
        memset(&xev, 0, sizeof(xev));
        xev.type = ClientMessage;
        xev.xclient.window = w->xwin;
        xev.xclient.message_type = g_atom_wm_state;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 2; // _NET_WM_STATE_TOGGLE
        xev.xclient.data.l[1] = (long)g_atom_fullscreen;
        XSendEvent(g_x11_dpy, DefaultRootWindow(g_x11_dpy), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &xev);
        XFlush(g_x11_dpy);
    }
#endif
}

// ============================================================================
// Native Widget Primitives
// ============================================================================

void* sph_gui_button_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    return (void*)sph_x11_create_widget(SPH_X11_BUTTON, parent_hwnd, text, x, y, w, h, id);
}

int32_t sph_gui_button_is_pushed(void* ctrl_hwnd) {
    SphX11Widget* w = (SphX11Widget*)ctrl_hwnd;
    return (w && w->pushed) ? 1 : 0;
}

void* sph_gui_label_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    return (void*)sph_x11_create_widget(SPH_X11_LABEL, parent_hwnd, text, x, y, w, h, id);
}

void* sph_gui_textbox_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id, int32_t multiline) {
    SphX11Widget* box = sph_x11_create_widget(SPH_X11_TEXTBOX, parent_hwnd, text, x, y, w, h, id);
    if (box) box->multiline = multiline;
    return (void*)box;
}

void* sph_gui_checkbox_create(void* parent_hwnd, const char* text, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    return (void*)sph_x11_create_widget(SPH_X11_CHECKBOX, parent_hwnd, text, x, y, w, h, id);
}

int32_t sph_gui_checkbox_is_checked(void* ctrl_hwnd) {
    SphX11Widget* w = (SphX11Widget*)ctrl_hwnd;
    return (w && w->checked) ? 1 : 0;
}

void sph_gui_checkbox_set_checked(void* ctrl_hwnd, int32_t checked) {
    SphX11Widget* w = (SphX11Widget*)ctrl_hwnd;
    if (!w) return;
    w->checked = checked ? 1 : 0;
#if defined(SPH_HAS_X11)
    sph_x11_draw_widget(w);
    if (g_x11_dpy) XFlush(g_x11_dpy);
#endif
}

void* sph_gui_progressbar_create(void* parent_hwnd, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    return (void*)sph_x11_create_widget(SPH_X11_PROGRESSBAR, parent_hwnd, "", x, y, w, h, id);
}

void sph_gui_progressbar_set_pos(void* ctrl_hwnd, int32_t pos) {
    SphX11Widget* w = (SphX11Widget*)ctrl_hwnd;
    if (!w) return;
    w->progress_pos = pos;
#if defined(SPH_HAS_X11)
    sph_x11_draw_widget(w);
    if (g_x11_dpy) XFlush(g_x11_dpy);
#endif
}

void sph_gui_progressbar_set_range(void* ctrl_hwnd, int32_t min_val, int32_t max_val) {
    SphX11Widget* w = (SphX11Widget*)ctrl_hwnd;
    if (!w) return;
    w->progress_min = min_val;
    w->progress_max = max_val > min_val ? max_val : min_val + 1;
#if defined(SPH_HAS_X11)
    sph_x11_draw_widget(w);
    if (g_x11_dpy) XFlush(g_x11_dpy);
#endif
}

void* sph_gui_listview_create(void* parent_hwnd, int32_t x, int32_t y, int32_t w, int32_t h, int32_t id) {
    return (void*)sph_x11_create_widget(SPH_X11_LISTVIEW, parent_hwnd, "", x, y, w, h, id);
}

int32_t sph_gui_listview_add_item(void* ctrl_hwnd, const char* text) {
    SphX11Widget* w = (SphX11Widget*)ctrl_hwnd;
    if (!w || w->list_count >= SPH_X11_MAX_ITEMS) return -1;
    int32_t idx = w->list_count;
    w->list_items[w->list_count++] = sph_x11_strdup(text ? text : "");
#if defined(SPH_HAS_X11)
    sph_x11_draw_widget(w);
    if (g_x11_dpy) XFlush(g_x11_dpy);
#endif
    return idx;
}

int32_t sph_gui_listview_get_selected(void* ctrl_hwnd) {
    SphX11Widget* w = (SphX11Widget*)ctrl_hwnd;
    return w ? w->list_selected : -1;
}

const char* sph_gui_get_text(void* ctrl_hwnd) {
    SphX11Widget* w = (SphX11Widget*)ctrl_hwnd;
    g_x11_text_ret[0] = '\0';
    if (w && w->text) {
        strncpy(g_x11_text_ret, w->text, sizeof(g_x11_text_ret) - 1);
        g_x11_text_ret[sizeof(g_x11_text_ret) - 1] = '\0';
    }
    return g_x11_text_ret;
}

void sph_gui_set_text(void* ctrl_hwnd, const char* text) {
    SphX11Widget* w = (SphX11Widget*)ctrl_hwnd;
    if (!w) return;
    free(w->text);
    w->text = sph_x11_strdup(text ? text : "");
#if defined(SPH_HAS_X11)
    if (g_x11_dpy && w->xwin) {
        if (w->kind == SPH_X11_WINDOW) {
            XStoreName(g_x11_dpy, w->xwin, w->text);
        } else {
            sph_x11_draw_widget(w);
        }
        XFlush(g_x11_dpy);
    }
#endif
}

// ============================================================================
// Dialogs, Clipboard, Cursor, Hidden Process
// ============================================================================

int32_t sph_gui_messagebox(void* parent_hwnd, const char* text, const char* title, int32_t utype) {
    (void)parent_hwnd;
    const char* msg = text ? text : "";
    const char* cap = title ? title : "Message";
    int32_t kind = utype & 0xF;

#if defined(SPH_HAS_X11)
    sph_x11_ensure_display();
    if (g_x11_dpy) {
        Window dlg = XCreateSimpleWindow(
            g_x11_dpy, RootWindow(g_x11_dpy, g_x11_screen), 200, 200, 380, 140, 1, 0x000000UL, 0xF4F4F4UL);
        if (dlg) {
            XStoreName(g_x11_dpy, dlg, cap);
            XSetWMProtocols(g_x11_dpy, dlg, &g_atom_wm_delete, 1);
            XSelectInput(g_x11_dpy, dlg, ExposureMask | ButtonPressMask | KeyPressMask);
            GC gc = XCreateGC(g_x11_dpy, dlg, 0, NULL);
            XMapRaised(g_x11_dpy, dlg);

            int32_t result = 1; // IDOK
            int done = 0;
            while (!done) {
                XEvent ev;
                XNextEvent(g_x11_dpy, &ev);
                if (ev.type == Expose) {
                    XSetForeground(g_x11_dpy, gc, 0x111111UL);
                    XDrawString(g_x11_dpy, dlg, gc, 20, 44, msg, (int)strlen(msg));
                    XSetForeground(g_x11_dpy, gc, 0xDDDDDDUL);
                    XFillRectangle(g_x11_dpy, dlg, gc, 150, 92, 80, 28);
                    XSetForeground(g_x11_dpy, gc, 0x111111UL);
                    XDrawRectangle(g_x11_dpy, dlg, gc, 150, 92, 80, 28);
                    const char* btnLabel = (kind == 4 || kind == 3) ? "Yes" : "OK";
                    XDrawString(g_x11_dpy, dlg, gc, 180, 111, btnLabel, (int)strlen(btnLabel));
                } else if (ev.type == ButtonPress) {
                    result = (kind == 4 || kind == 3) ? 6 : 1;
                    done = 1;
                } else if (ev.type == KeyPress) {
                    KeySym ks = XLookupKeysym(&ev.xkey, 0);
                    if (ks == XK_Escape) {
                        result = (kind == 4 || kind == 3) ? 7 : 2;
                        done = 1;
                    } else if (ks == XK_Return || ks == XK_space) {
                        result = (kind == 4 || kind == 3) ? 6 : 1;
                        done = 1;
                    }
                } else if (ev.type == ClientMessage) {
                    result = 2;
                    done = 1;
                }
            }
            XFreeGC(g_x11_dpy, gc);
            XDestroyWindow(g_x11_dpy, dlg);
            XFlush(g_x11_dpy);
            return result;
        }
    }
#endif
    fprintf(stderr, "[%s] %s\n", cap, msg);
    return (kind == 4 || kind == 3) ? 6 : 1;
}

static int sph_x11_try_dialog_cmd(const char* argv_tool, const char* title, int save_mode, char* out, size_t out_cap) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return 0;
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(pipefd[1]);
        if (strcmp(argv_tool, "zenity") == 0) {
            if (save_mode) {
                execlp("zenity", "zenity", "--file-selection", "--save", "--confirm-overwrite",
                       "--title", title ? title : "Save File", (char*)NULL);
            } else {
                execlp("zenity", "zenity", "--file-selection",
                       "--title", title ? title : "Open File", (char*)NULL);
            }
        } else {
            execlp("kdialog", "kdialog", save_mode ? "--getsavefilename" : "--getopenfilename",
                   ".", "--title", title ? title : "Select File", (char*)NULL);
        }
        _exit(127);
    }
    close(pipefd[1]);
    size_t pos = 0;
    ssize_t n;
    while (pos + 1 < out_cap && (n = read(pipefd[0], out + pos, out_cap - 1 - pos)) > 0) {
        pos += (size_t)n;
    }
    out[pos] = '\0';
    close(pipefd[0]);
    while (pos > 0 && (out[pos - 1] == '\n' || out[pos - 1] == '\r')) {
        out[--pos] = '\0';
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0 && pos > 0);
}

const char* sph_gui_file_picker_open(void* parent_hwnd, const char* title, const char* filter) {
    (void)parent_hwnd;
    (void)filter;
    g_x11_picker_ret[0] = '\0';
    if (!sph_x11_try_dialog_cmd("zenity", title, 0, g_x11_picker_ret, sizeof(g_x11_picker_ret))) {
        sph_x11_try_dialog_cmd("kdialog", title, 0, g_x11_picker_ret, sizeof(g_x11_picker_ret));
    }
    return g_x11_picker_ret;
}

const char* sph_gui_file_picker_save(void* parent_hwnd, const char* title, const char* filter) {
    (void)parent_hwnd;
    (void)filter;
    g_x11_picker_ret[0] = '\0';
    if (!sph_x11_try_dialog_cmd("zenity", title, 1, g_x11_picker_ret, sizeof(g_x11_picker_ret))) {
        sph_x11_try_dialog_cmd("kdialog", title, 1, g_x11_picker_ret, sizeof(g_x11_picker_ret));
    }
    return g_x11_picker_ret;
}

const char* sph_gui_clipboard_get_text(void) {
#if defined(SPH_HAS_X11)
    sph_x11_ensure_display();
    if (g_x11_dpy && g_atom_clipboard != None && g_x11_clip_owner) {
        Window owner = XGetSelectionOwner(g_x11_dpy, g_atom_clipboard);
        if (owner != None && owner != g_x11_clip_owner) {
            Atom prop = XInternAtom(g_x11_dpy, "SPH_CLIP_DATA", False);
            XConvertSelection(g_x11_dpy, g_atom_clipboard, g_atom_utf8 ? g_atom_utf8 : XA_STRING,
                              prop, g_x11_clip_owner, CurrentTime);
            XFlush(g_x11_dpy);
            XEvent ev;
            for (int i = 0; i < 20; ++i) {
                if (XCheckTypedWindowEvent(g_x11_dpy, g_x11_clip_owner, SelectionNotify, &ev)) {
                    if (ev.xselection.property != None) {
                        Atom actual;
                        int fmt;
                        unsigned long nitems, bytes_after;
                        unsigned char* data = NULL;
                        if (XGetWindowProperty(g_x11_dpy, g_x11_clip_owner, prop, 0, 16384, True,
                                               AnyPropertyType, &actual, &fmt, &nitems, &bytes_after, &data) == Success &&
                            data) {
                            size_t copy = nitems < sizeof(g_x11_clipboard_text) - 1 ? nitems : sizeof(g_x11_clipboard_text) - 1;
                            memcpy(g_x11_clipboard_text, data, copy);
                            g_x11_clipboard_text[copy] = '\0';
                            XFree(data);
                        }
                    }
                    break;
                }
                usleep(2000);
            }
        }
    }
#endif
    return g_x11_clipboard_text;
}

void sph_gui_clipboard_set_text(const char* text) {
    if (!text) return;
    strncpy(g_x11_clipboard_text, text, sizeof(g_x11_clipboard_text) - 1);
    g_x11_clipboard_text[sizeof(g_x11_clipboard_text) - 1] = '\0';
#if defined(SPH_HAS_X11)
    sph_x11_ensure_display();
    if (g_x11_dpy && g_atom_clipboard != None && g_x11_clip_owner) {
        XSetSelectionOwner(g_x11_dpy, g_atom_clipboard, g_x11_clip_owner, CurrentTime);
        XFlush(g_x11_dpy);
    }
#endif
}

void sph_gui_set_cursor(int32_t cursor_type) {
#if defined(SPH_HAS_X11)
    sph_x11_ensure_display();
    if (!g_x11_dpy) return;
    unsigned int shape = XC_left_ptr;
    switch (cursor_type) {
        case 1: shape = XC_xterm; break;
        case 2: shape = XC_watch; break;
        case 3: shape = XC_crosshair; break;
        case 4: shape = XC_hand2; break;
        default: shape = XC_left_ptr; break;
    }
    Cursor cur = XCreateFontCursor(g_x11_dpy, shape);
    pthread_mutex_lock(&g_x11_lock);
    for (int32_t i = 0; i < g_x11_widget_count; ++i) {
        if (g_x11_widgets[i] && g_x11_widgets[i]->kind == SPH_X11_WINDOW && g_x11_widgets[i]->xwin) {
            XDefineCursor(g_x11_dpy, g_x11_widgets[i]->xwin, cur);
        }
    }
    pthread_mutex_unlock(&g_x11_lock);
    XFreeCursor(g_x11_dpy, cur);
    XFlush(g_x11_dpy);
#else
    (void)cursor_type;
#endif
}

void sph_gui_register_event_handler(int32_t id, sph_gui_event_callback callback, void* user_data) {
    for (int32_t i = 0; i < SPH_X11_MAX_HANDLERS; ++i) {
        if (!g_x11_handlers[i].used || g_x11_handlers[i].id == id) {
            g_x11_handlers[i].used = 1;
            g_x11_handlers[i].id = id;
            g_x11_handlers[i].cb = callback;
            g_x11_handlers[i].user_data = user_data;
            return;
        }
    }
}

void sph_gui_set_wndproc_hook(void* hwnd, void* hook_fn) {
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (w) {
        w->hook_cb = (sph_gui_event_callback)hook_fn;
    }
}

int32_t sph_gui_exec_hidden(const char* cmd) {
    if (!cmd || !*cmd) return -1;
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }
    if (pid < 0) return -1;
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// ============================================================================
// Canvas Present & GLX Bridges Used by gui_canvas.c
// ============================================================================

static void sph_x11_canvas_present(sph_canvas* canvas) {
#if defined(SPH_HAS_X11)
    if (!canvas || !canvas->pixels || !canvas->hwnd || !g_x11_dpy) return;
    SphX11Widget* w = (SphX11Widget*)canvas->hwnd;
    if (!w->xwin || !w->xgc) return;
    w->canvas = canvas;

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(g_x11_dpy, w->xwin, &attrs) || attrs.width <= 0 || attrs.height <= 0) {
        return;
    }

    Visual* vis = DefaultVisual(g_x11_dpy, g_x11_screen);
    int depth = DefaultDepth(g_x11_dpy, g_x11_screen);
    if (depth < 24) return;

    XImage* img = XCreateImage(g_x11_dpy, vis, (unsigned)depth, ZPixmap, 0,
                               (char*)canvas->pixels, (unsigned)canvas->width, (unsigned)canvas->height, 32, 0);
    if (img) {
        int dst_x = (attrs.width > canvas->width) ? (attrs.width - canvas->width) / 2 : 0;
        int dst_y = (attrs.height > canvas->height) ? (attrs.height - canvas->height) / 2 : 0;
        int blit_w = attrs.width < canvas->width ? attrs.width : canvas->width;
        int blit_h = attrs.height < canvas->height ? attrs.height : canvas->height;
        XPutImage(g_x11_dpy, w->xwin, w->xgc, img, 0, 0, dst_x, dst_y, (unsigned)blit_w, (unsigned)blit_h);
        img->data = NULL;
        XDestroyImage(img);
        XFlush(g_x11_dpy);
    }
#else
    (void)canvas;
#endif
}

static int sph_x11_get_window_size(void* hwnd, int32_t* out_w, int32_t* out_h) {
    if (!hwnd) return 0;
    sph_gui_window_get_size(hwnd, out_w, out_h);
    return 1;
}

static void* sph_x11_gl_context_create(void* hwnd) {
#if defined(SPH_HAS_X11) && defined(SPH_HAS_GLX)
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (!w || !g_x11_dpy || !w->xwin) return NULL;
    int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    XVisualInfo* vi = glXChooseVisual(g_x11_dpy, g_x11_screen, attribs);
    if (!vi) return NULL;
    GLXContext ctx = glXCreateContext(g_x11_dpy, vi, NULL, GL_TRUE);
    XFree(vi);
    if (ctx) {
        w->glx_ctx = ctx;
        glXMakeCurrent(g_x11_dpy, w->xwin, ctx);
    }
    return (void*)ctx;
#else
    (void)hwnd;
    return NULL;
#endif
}

static void sph_x11_gl_swap_buffers(void* hwnd) {
#if defined(SPH_HAS_X11) && defined(SPH_HAS_GLX)
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (w && g_x11_dpy && w->xwin) {
        glXSwapBuffers(g_x11_dpy, w->xwin);
    }
#else
    (void)hwnd;
#endif
}

static void sph_x11_gl_make_current(void* hwnd, void* gl_ctx) {
#if defined(SPH_HAS_X11) && defined(SPH_HAS_GLX)
    SphX11Widget* w = (SphX11Widget*)hwnd;
    if (!g_x11_dpy) return;
    if (w && w->xwin && gl_ctx) {
        glXMakeCurrent(g_x11_dpy, w->xwin, (GLXContext)gl_ctx);
    } else {
        glXMakeCurrent(g_x11_dpy, None, NULL);
    }
#else
    (void)hwnd;
    (void)gl_ctx;
#endif
}

// ============================================================================
// X11 + Epoll Pump & Wait Helpers Used by gui.c
// ============================================================================

static int32_t sph_x11_pump_events(volatile int32_t* running_flag) {
#if defined(SPH_HAS_X11)
    if (!g_x11_dpy) return running_flag ? *running_flag : 1;
    while (XPending(g_x11_dpy) > 0) {
        XEvent ev;
        XNextEvent(g_x11_dpy, &ev);
        if (ev.type == SelectionRequest && ev.xselectionrequest.owner == g_x11_clip_owner) {
            XSelectionEvent reply;
            memset(&reply, 0, sizeof(reply));
            reply.type = SelectionNotify;
            reply.requestor = ev.xselectionrequest.requestor;
            reply.selection = ev.xselectionrequest.selection;
            reply.target = ev.xselectionrequest.target;
            reply.time = ev.xselectionrequest.time;
            reply.property = None;
            if (ev.xselectionrequest.target == g_atom_targets) {
                Atom targets[2] = { g_atom_utf8 ? g_atom_utf8 : XA_STRING, XA_STRING };
                XChangeProperty(g_x11_dpy, reply.requestor, ev.xselectionrequest.property, XA_ATOM, 32,
                                PropModeReplace, (unsigned char*)targets, 2);
                reply.property = ev.xselectionrequest.property;
            } else if (ev.xselectionrequest.target == XA_STRING || ev.xselectionrequest.target == g_atom_utf8) {
                XChangeProperty(g_x11_dpy, reply.requestor, ev.xselectionrequest.property,
                                ev.xselectionrequest.target, 8, PropModeReplace,
                                (unsigned char*)g_x11_clipboard_text, (int)strlen(g_x11_clipboard_text));
                reply.property = ev.xselectionrequest.property;
            }
            XSendEvent(g_x11_dpy, reply.requestor, False, 0, (XEvent*)&reply);
            continue;
        }

        SphX11Widget* target = sph_x11_find_by_xwin(ev.xany.window);
        if (!target) continue;

        switch (ev.type) {
            case Expose:
                if (ev.xexpose.count == 0) {
                    sph_x11_draw_widget(target);
                    sph_x11_dispatch_event(target->parent ? target->parent : target, 6, target->id, 0, 0);
                }
                break;
            case ConfigureNotify:
                target->w = ev.xconfigure.width;
                target->h = ev.xconfigure.height;
                sph_x11_dispatch_event(target, 2, target->id, target->w, target->h);
                break;
            case ButtonPress:
                if (target->kind == SPH_X11_BUTTON) {
                    target->pushed = 1;
                    sph_x11_draw_widget(target);
                } else if (target->kind == SPH_X11_CHECKBOX) {
                    target->checked = !target->checked;
                    sph_x11_draw_widget(target);
                    sph_x11_dispatch_event(target->parent ? target->parent : target, 1, target->id, target->checked, 0);
                } else if (target->kind == SPH_X11_LISTVIEW) {
                    int32_t idx = (ev.xbutton.y - 4) / 20;
                    if (idx >= 0 && idx < target->list_count) {
                        target->list_selected = idx;
                        sph_x11_draw_widget(target);
                        sph_x11_dispatch_event(target->parent ? target->parent : target, 1, target->id, idx, 0);
                    }
                }
                sph_x11_dispatch_event(target->parent ? target->parent : target, 4, target->id, ev.xbutton.x, ev.xbutton.y);
                break;
            case ButtonRelease:
                if (target->kind == SPH_X11_BUTTON && target->pushed) {
                    target->pushed = 0;
                    sph_x11_draw_widget(target);
                    sph_x11_dispatch_event(target->parent ? target->parent : target, 1, target->id, 0, 0);
                }
                break;
            case LeaveNotify:
                if (target->kind == SPH_X11_BUTTON && target->pushed) {
                    target->pushed = 0;
                    sph_x11_draw_widget(target);
                }
                break;
            case KeyPress: {
                char buf[16];
                KeySym ks = NoSymbol;
                int len = XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &ks, NULL);
                if (target->kind == SPH_X11_TEXTBOX && len > 0) {
                    size_t cur_len = target->text ? strlen(target->text) : 0;
                    if (ks == XK_BackSpace && cur_len > 0) {
                        target->text[cur_len - 1] = '\0';
                        sph_x11_draw_widget(target);
                        sph_x11_dispatch_event(target->parent ? target->parent : target, 7, target->id, 0, 0);
                    } else if ((unsigned char)buf[0] >= 32) {
                        char* grown = (char*)realloc(target->text, cur_len + (size_t)len + 1);
                        if (grown) {
                            memcpy(grown + cur_len, buf, (size_t)len);
                            grown[cur_len + (size_t)len] = '\0';
                            target->text = grown;
                            sph_x11_draw_widget(target);
                            sph_x11_dispatch_event(target->parent ? target->parent : target, 7, target->id, 0, 0);
                        }
                    }
                }
                sph_x11_dispatch_event(target->parent ? target->parent : target, 3, target->id, (int64_t)ks, 0);
                break;
            }
            case ClientMessage:
                if (g_atom_wm_delete != None && (Atom)ev.xclient.data.l[0] == g_atom_wm_delete) {
                    target->open = 0;
                    sph_x11_dispatch_event(target, 5, target->id, 0, 0);
                    if (running_flag) *running_flag = 0;
                }
                break;
            default:
                break;
        }
    }
#endif
    if (g_x11_wake_fd >= 0) {
        uint64_t drain = 0;
        ssize_t n = read(g_x11_wake_fd, &drain, sizeof(drain));
        (void)n;
    }
    return running_flag ? *running_flag : 1;
}

static void sph_x11_wait_events(int timeout_ms) {
    sph_x11_ensure_epoll();
    if (g_x11_epoll_fd >= 0) {
        struct epoll_event events[4];
        int nfds = epoll_wait(g_x11_epoll_fd, events, 4, timeout_ms);
        (void)nfds;
    } else {
        usleep((useconds_t)(timeout_ms > 0 ? timeout_ms * 1000 : 1000));
    }
}

#endif // __linux__ || (__unix__ && !__APPLE__)

