// Canvas package — SDL2 wrapper for compiled MAGI programs
// Provides sdl_* runtime call implementations that the MAGI runtime dispatches to.
// Link this file + libSDL2.a when building MAGI programs that use SDL2.
//
// Build: cc program.o magi_rt.c magi_sdl2.c -Ilib/include lib/<platform>/libSDL2.a -lpthread -ldl -lm -o program

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include "include/SDL.h"
#else
#include <SDL2/SDL.h>
#endif

// NaN-boxing (must match magi_runtime.c)
#define NANBOX_SIG   ((uint64_t)0xFFF8000000000000ULL)
#define PAYLOAD_MASK ((uint64_t)0x0000FFFFFFFFFFFFULL)
#define TAG_SHIFT    48
#define TAG_NULL   0
#define TAG_I64    2
#define TAG_STRING 3

static int64_t mk_null(void) { return (int64_t)(NANBOX_SIG | ((uint64_t)TAG_NULL << TAG_SHIFT)); }
static int64_t mk_int(int64_t n) { return (int64_t)(NANBOX_SIG | ((uint64_t)TAG_I64 << TAG_SHIFT) | ((uint64_t)n & PAYLOAD_MASK)); }
static int64_t mk_str(const char* s) { return (int64_t)(NANBOX_SIG | ((uint64_t)TAG_STRING << TAG_SHIFT) | ((uint64_t)(uintptr_t)s & PAYLOAD_MASK)); }
static int64_t as_int(int64_t v) { return (int64_t)((uint64_t)v & PAYLOAD_MASK); }
static int64_t sext48(int64_t v) { return (v << 16) >> 16; }
static int64_t get_int(int64_t v) { return sext48(as_int(v)); }
static const char* get_str(int64_t v) { return (const char*)(uintptr_t)((uint64_t)v & PAYLOAD_MASK); }

extern int64_t __magi_map_new(int32_t count, int64_t* entries);

typedef struct { SDL_Window* win; SDL_Renderer* ren; } SdlCtx;

// Called by __magi_runtime_call for "sdl_init", "sdl_clear", etc.
// The canvas package registers these by name.

int64_t canvas_sdl_dispatch(const char* name, int32_t argc, int64_t* args) {
    int64_t a = argc > 0 ? args[0] : mk_null();
    int64_t b = argc > 1 ? args[1] : mk_null();

    if (strcmp(name, "sdl_init") == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) return mk_null();
        const char* title = get_str(a);
        int w = (int)get_int(b);
        int h = argc > 2 ? (int)get_int(args[2]) : 600;
        SDL_Window* win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_SHOWN);
        if (!win) return mk_null();
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) { SDL_DestroyWindow(win); return mk_null(); }
        SdlCtx* ctx = (SdlCtx*)malloc(sizeof(SdlCtx));
        ctx->win = win; ctx->ren = ren;
        return mk_int((int64_t)(uintptr_t)ctx);
    }
    if (strcmp(name, "sdl_set_color") == 0) {
        SdlCtx* ctx = (SdlCtx*)(uintptr_t)get_int(a);
        if (ctx) SDL_SetRenderDrawColor(ctx->ren, (int)get_int(b),
            argc>2?(int)get_int(args[2]):0, argc>3?(int)get_int(args[3]):0, 255);
        return mk_null();
    }
    if (strcmp(name, "sdl_clear") == 0) {
        SdlCtx* ctx = (SdlCtx*)(uintptr_t)get_int(a);
        if (ctx) SDL_RenderClear(ctx->ren);
        return mk_null();
    }
    if (strcmp(name, "sdl_present") == 0) {
        SdlCtx* ctx = (SdlCtx*)(uintptr_t)get_int(a);
        if (ctx) SDL_RenderPresent(ctx->ren);
        return mk_null();
    }
    if (strcmp(name, "sdl_fill_rect") == 0) {
        SdlCtx* ctx = (SdlCtx*)(uintptr_t)get_int(a);
        if (ctx) {
            SDL_Rect r = {(int)get_int(b), argc>2?(int)get_int(args[2]):0,
                          argc>3?(int)get_int(args[3]):1, argc>4?(int)get_int(args[4]):1};
            SDL_RenderFillRect(ctx->ren, &r);
        }
        return mk_null();
    }
    if (strcmp(name, "sdl_draw_pixel") == 0) {
        SdlCtx* ctx = (SdlCtx*)(uintptr_t)get_int(a);
        if (ctx) SDL_RenderDrawPoint(ctx->ren, (int)get_int(b), argc>2?(int)get_int(args[2]):0);
        return mk_null();
    }
    if (strcmp(name, "sdl_draw_line") == 0) {
        SdlCtx* ctx = (SdlCtx*)(uintptr_t)get_int(a);
        if (ctx) SDL_RenderDrawLine(ctx->ren, (int)get_int(b),
            argc>2?(int)get_int(args[2]):0, argc>3?(int)get_int(args[3]):0, argc>4?(int)get_int(args[4]):0);
        return mk_null();
    }
    if (strcmp(name, "sdl_poll_event") == 0) {
        SdlCtx* ctx = (SdlCtx*)(uintptr_t)get_int(a);
        if (!ctx) return mk_null();
        SDL_Event ev;
        if (!SDL_PollEvent(&ev)) return mk_null();
        int64_t entries[4];
        entries[0] = mk_str("type");
        entries[1] = mk_int(ev.type);
        entries[2] = mk_str("scancode");
        entries[3] = mk_int(ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP ? ev.key.keysym.scancode : 0);
        return __magi_map_new(2, entries);
    }
    if (strcmp(name, "sdl_delay") == 0) {
        SDL_Delay((int)get_int(a));
        return mk_null();
    }
    if (strcmp(name, "sdl_ticks") == 0) {
        return mk_int((int64_t)SDL_GetTicks());
    }
    if (strcmp(name, "sdl_destroy") == 0) {
        SdlCtx* ctx = (SdlCtx*)(uintptr_t)get_int(a);
        if (ctx) { SDL_DestroyRenderer(ctx->ren); SDL_DestroyWindow(ctx->win); SDL_Quit(); free(ctx); }
        return mk_null();
    }
    return mk_null();
}

// Hook: the MAGI runtime calls __magi_runtime_call_hook before returning null.
// If this symbol exists, the runtime chains to it for unknown calls.
// Canvas provides SDL2 dispatch through this hook.
int64_t __magi_runtime_call_hook(const char* name, int32_t argc, int64_t* args) {
    if (strncmp(name, "sdl_", 4) == 0) {
        return canvas_sdl_dispatch(name, argc, args);
    }
    // Not an SDL call — return sentinel to indicate "not handled"
    return mk_null();
}
