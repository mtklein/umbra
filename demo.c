#include "slides/slides.h"
#include <stdint.h>
#include <string.h>

#if defined(__wasm__)

int main(void) { return 0; }

#else

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop

typedef void (*run_fn)(void*, int, umbra_buf[]);

static void run_interp(void *ctx, int n, umbra_buf buf[]) {
    umbra_interpreter_run(ctx, n, buf);
}
static void run_jit(void *ctx, int n, umbra_buf buf[]) {
    umbra_jit_run(ctx, n, buf);
}

// Pipeline state for the current slide.
static struct umbra_jit         *jit;
static struct umbra_interpreter *interp;
static run_fn                    run;
static void                     *backend;

static void build_slide(const slide *s) {
    if (jit)    { umbra_jit_free(jit);             jit    = NULL; }
    if (interp) { umbra_interpreter_free(interp);  interp = NULL; }

    struct umbra_basic_block *bb = umbra_draw_build(
        s->shader, s->coverage, s->blend, s->load, s->store);
    umbra_basic_block_optimize(bb);

    jit = umbra_jit(bb);
    if (jit) {
        run     = run_jit;
        backend = jit;
    } else {
        interp  = umbra_interpreter(bb);
        run     = run_interp;
        backend = interp;
    }
    umbra_basic_block_free(bb);
}

int main(void) {
    enum { W = 640, H = 480 };

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("umbra demo", W, H, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, W, H);
    if (!texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        return 1;
    }

    int cur_slide = 0;
    build_slide(&slides[cur_slide]);
    SDL_SetWindowTitle(window, slides[cur_slide].title);
    SDL_Log("Slide %d/%d: %s  [%s]",
            cur_slide + 1, SLIDE_COUNT, slides[cur_slide].title,
            jit ? "JIT" : "interpreter");

    // Rect animation state.
    float rect_w = 200.0f, rect_h = 150.0f;
    float rx = 100.0f, ry = 80.0f;
    float vx = 1.5f,   vy = 1.1f;

    _Bool running = 1;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = 0;
            } else if (ev.type == SDL_EVENT_KEY_DOWN) {
                int next = cur_slide;
                if (ev.key.key == SDLK_RIGHT || ev.key.key == SDLK_SPACE) {
                    next = (cur_slide + 1) % SLIDE_COUNT;
                } else if (ev.key.key == SDLK_LEFT) {
                    next = (cur_slide + SLIDE_COUNT - 1) % SLIDE_COUNT;
                } else if (ev.key.key == SDLK_ESCAPE) {
                    running = 0;
                }
                if (next != cur_slide) {
                    cur_slide = next;
                    build_slide(&slides[cur_slide]);
                    SDL_SetWindowTitle(window, slides[cur_slide].title);
                    SDL_Log("Slide %d/%d: %s  [%s]",
                            cur_slide + 1, SLIDE_COUNT, slides[cur_slide].title,
                            jit ? "JIT" : "interpreter");
                }
            }
        }
        if (!running) break;

        const slide *s = &slides[cur_slide];

        // Animate: bounce the rect around.
        rx += vx;
        ry += vy;
        if (rx < 0.0f)              { rx = 0.0f;              vx = -vx; }
        if (rx + rect_w > (float)W) { rx = (float)W - rect_w; vx = -vx; }
        if (ry < 0.0f)              { ry = 0.0f;              vy = -vy; }
        if (ry + rect_h > (float)H) { ry = (float)H - rect_h; vy = -vy; }

        float rect[4] = { rx, ry, rx + rect_w, ry + rect_h };

        __fp16 color[4] = {
            (__fp16)s->color[0],
            (__fp16)s->color[1],
            (__fp16)s->color[2],
            (__fp16)s->color[3],
        };

        // Lock texture to get a pixel buffer.
        void *tex_pixels;
        int   tex_pitch;
        if (!SDL_LockTexture(texture, NULL, &tex_pixels, &tex_pitch)) {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
            break;
        }

        // Fill background.
        uint8_t *rows = (uint8_t*)tex_pixels;
        for (int y = 0; y < H; y++) {
            uint32_t *row = (uint32_t*)(rows + y * tex_pitch);
            for (int x = 0; x < W; x++) {
                row[x] = s->bg;
            }
        }

        // Draw the rect scanline by scanline.
        int32_t x0 = 0;
        for (int y = 0; y < H; y++) {
            int32_t yval = y;
            uint32_t *row = (uint32_t*)(rows + y * tex_pitch);
            umbra_buf buf[] = {
                { row,   W * 4 },
                { &x0,  -4     },
                { &yval,-4     },
                { color,-8     },
                { rect, -16    },
            };
            run(backend, W, buf);
        }

        SDL_UnlockTexture(texture);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    // Cleanup.
    if (jit)    umbra_jit_free(jit);
    if (interp) umbra_interpreter_free(interp);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#endif
