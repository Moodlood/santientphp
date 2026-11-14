// ============================================================================
// SantientPHP (SPH) - Native GUI Subsystem Aggregate & Dispatch Queue
// ============================================================================

#include "gui.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gui_win32.c"
#endif

#include "gui_canvas.c"
#include "gui_webview.c"

// ============================================================================
// Thread-Safe UI Dispatch Queue (Gui::post_to_main)
// ============================================================================

#define SPH_DISPATCH_QUEUE_CAPACITY 256

typedef struct sph_dispatch_task {
    sph_gui_task_fn fn;
    void* arg;
} sph_dispatch_task;

static sph_dispatch_task g_dispatch_queue[SPH_DISPATCH_QUEUE_CAPACITY];
static volatile int32_t g_dispatch_head = 0;
static volatile int32_t g_dispatch_tail = 0;
static volatile int32_t g_gui_running = 1;

#if defined(_WIN32)
static CRITICAL_SECTION g_dispatch_cs;
static int g_dispatch_cs_init = 0;

static void ensure_dispatch_cs(void) {
    if (!g_dispatch_cs_init) {
        InitializeCriticalSection(&g_dispatch_cs);
        g_dispatch_cs_init = 1;
    }
}
#endif

void sph_gui_post_to_main(sph_gui_task_fn fn, void* arg) {
    if (!fn) return;

#if defined(_WIN32)
    ensure_dispatch_cs();
    EnterCriticalSection(&g_dispatch_cs);

    int32_t next_head = (g_dispatch_head + 1) % SPH_DISPATCH_QUEUE_CAPACITY;
    if (next_head != g_dispatch_tail) {
        g_dispatch_queue[g_dispatch_head].fn = fn;
        g_dispatch_queue[g_dispatch_head].arg = arg;
        g_dispatch_head = next_head;
    }

    LeaveCriticalSection(&g_dispatch_cs);
#else
    int32_t next_head = (g_dispatch_head + 1) % SPH_DISPATCH_QUEUE_CAPACITY;
    if (next_head != g_dispatch_tail) {
        g_dispatch_queue[g_dispatch_head].fn = fn;
        g_dispatch_queue[g_dispatch_head].arg = arg;
        g_dispatch_head = next_head;
    }
#endif
}

void sph_gui_dispatch_posted_tasks(void) {
#if defined(_WIN32)
    ensure_dispatch_cs();
    EnterCriticalSection(&g_dispatch_cs);

    while (g_dispatch_tail != g_dispatch_head) {
        sph_dispatch_task task = g_dispatch_queue[g_dispatch_tail];
        g_dispatch_tail = (g_dispatch_tail + 1) % SPH_DISPATCH_QUEUE_CAPACITY;

        LeaveCriticalSection(&g_dispatch_cs);
        if (task.fn) {
            task.fn(task.arg);
        }
        EnterCriticalSection(&g_dispatch_cs);
    }

    LeaveCriticalSection(&g_dispatch_cs);
#else
    while (g_dispatch_tail != g_dispatch_head) {
        sph_dispatch_task task = g_dispatch_queue[g_dispatch_tail];
        g_dispatch_tail = (g_dispatch_tail + 1) % SPH_DISPATCH_QUEUE_CAPACITY;
        if (task.fn) {
            task.fn(task.arg);
        }
    }
#endif
}

// ============================================================================
// Non-Blocking Message Pump & Unified Event Loop
// ============================================================================

int32_t sph_gui_pump_events(void) {
#if defined(_WIN32)
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_gui_running = 0;
            return 0; // Exiting
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    sph_gui_dispatch_posted_tasks();
    return g_gui_running;
#else
    sph_gui_dispatch_posted_tasks();
    return g_gui_running;
#endif
}

void sph_gui_quit(void) {
    g_gui_running = 0;
#if defined(_WIN32)
    PostQuitMessage(0);
#endif
}

int32_t sph_gui_is_running(void) {
    return g_gui_running;
}

// External forward declarations from async runtime if available
extern void sph_event_loop_init(void);
extern void* sph_event_loop_pop_fiber(void);
extern void sph_event_loop_yield(void);

void sph_gui_run_event_loop(void) {
    g_gui_running = 1;
#if defined(_WIN32)
    while (g_gui_running) {
        // 1. Non-blocking message pump
        if (!sph_gui_pump_events()) {
            break;
        }

        // 2. Dispatch worker thread posted tasks
        sph_gui_dispatch_posted_tasks();

        // 3. Wait briefly or process fiber coroutines
        MsgWaitForMultipleObjectsEx(0, NULL, 10, QS_ALLINPUT, MWMO_ALERTABLE);
    }
#endif
}

