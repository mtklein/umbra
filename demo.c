#include "slides/slides.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(__wasm__)

int main(void) { return 0; }

#else

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop

enum { NUM_BACKENDS = 4 };
static char const *backend_name[NUM_BACKENDS] = {"interp", "jit", "codegen", "metal"};

typedef void (*run_fn)(void*, int, umbra_buf[]);

static void run_interp (void *ctx, int n, umbra_buf buf[]) { umbra_interpreter_run(ctx, n, buf); }
static void run_jit    (void *ctx, int n, umbra_buf buf[]) { umbra_jit_run(ctx, n, buf); }
static void run_codegen(void *ctx, int n, umbra_buf buf[]) { umbra_codegen_run(ctx, n, buf); }
static void run_metal  (void *ctx, int n, umbra_buf buf[]) { umbra_metal_run(ctx, n, buf); }

static run_fn const run_fns[NUM_BACKENDS] = {run_interp, run_jit, run_codegen, run_metal};

static struct umbra_interpreter *interp;
static struct umbra_jit         *jit;
static struct umbra_codegen     *codegen;
static struct umbra_metal       *mtl;

static void *backends[NUM_BACKENDS];

static void free_backends(void) {
    if (interp)  { umbra_interpreter_free(interp); interp  = NULL; }
    if (jit)     { umbra_jit_free(jit);            jit     = NULL; }
    if (codegen) { umbra_codegen_free(codegen);    codegen = NULL; }
    if (mtl)     { umbra_metal_free(mtl);          mtl     = NULL; }
    for (int i = 0; i < NUM_BACKENDS; i++) { backends[i] = NULL; }
}

static void build_slide(slide const *s) {
    free_backends();

    struct umbra_basic_block *bb = umbra_draw_build(
        s->shader, s->coverage, s->blend, s->load, s->store);
    umbra_basic_block_optimize(bb);

    interp  = umbra_interpreter(bb);
    jit     = umbra_jit(bb);
    codegen = umbra_codegen(bb);
    mtl     = umbra_metal(bb);
    umbra_basic_block_free(bb);

    backends[0] = interp;
    backends[1] = jit;
    backends[2] = codegen;
    backends[3] = mtl;
}

static int pick_backend(int cur) {
    if (backends[cur]) { return cur; }
    for (int i = 1; i < NUM_BACKENDS; i++) {
        int b = (cur + i) % NUM_BACKENDS;
        if (backends[b]) { return b; }
    }
    return 0;
}
static int next_backend(int cur) {
    for (int i = 1; i <= NUM_BACKENDS; i++) {
        int b = (cur + i) % NUM_BACKENDS;
        if (backends[b]) { return b; }
    }
    return cur;
}

// Build a 3x3 inverse projective matrix for mapping screen→bitmap coords.
// Animates a gentle perspective tilt around the center of the screen.
static void build_perspective_matrix(float out[11], float t, int sw, int sh, int bw, int bh) {
    float cx = (float)sw * 0.5f, cy = (float)sh * 0.5f;
    float bx = (float)bw * 0.5f, by = (float)bh * 0.5f;

    // Animated rotation and perspective tilt.
    float angle = t * 0.3f;
    float tilt  = sinf(t * 0.7f) * 0.0008f;
    float sc    = 1.0f + 0.2f * sinf(t * 0.5f);
    float ca = cosf(angle), sa = sinf(angle);

    // Forward: translate to center, scale+rotate, apply perspective, translate to bitmap.
    // We directly build the inverse: bitmap→screen, then invert.
    // Simpler: build screen→bitmap directly.
    //   1. Translate screen origin to center: x' = x - cx, y' = y - cy
    //   2. Apply perspective divisor: w = 1 + tilt*x' (simple 1D perspective)
    //   3. Scale and rotate: x'' = (ca*x'/w + sa*y'/w) * sc, y'' = (-sa*x'/w + ca*y'/w) * sc
    //   4. Translate to bitmap center: x''' = x'' + bx, y''' = y'' + by
    //
    // As a projective matrix [x_out, y_out, w_out] = M * [x, y, 1]:
    //   Row 2 (w): [tilt, 0, 1 - tilt*cx]
    //   Row 0 (x): [ca*sc, sa*sc, -cx*ca*sc - cy*sa*sc + bx*(1 - tilt*cx)]
    //   Row 1 (y): [-sa*sc, ca*sc, cx*sa*sc - cy*ca*sc + by*(1 - tilt*cx)]
    // But we want screen→bitmap, so the division happens on output.

    float w0 = 1.0f - tilt * cx;

    out[0] = ca * sc;
    out[1] = sa * sc;
    out[2] = -cx * ca * sc - cy * sa * sc + bx * w0;
    out[3] = -sa * sc;
    out[4] = ca * sc;
    out[5] = cx * sa * sc - cy * ca * sc + by * w0;
    out[6] = tilt;
    out[7] = 0.0f;
    out[8] = w0;
    out[9]  = (float)bw;
    out[10] = (float)bh;
}

static void update_title(SDL_Window *w, slide const *s, int bi, double fps) {
    char title[256];
    SDL_snprintf(title, sizeof title, "%s  [%s]  %.0f fps", s->title, backend_name[bi], fps);
    SDL_SetWindowTitle(w, title);
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

    text_cov bitmap_cov = text_rasterize(W, H, 72.0f, 0);
    text_cov sdf_cov    = text_rasterize(W, H, 72.0f, 1);

    int cur_slide = 0;
    build_slide(&slides[cur_slide]);
    int cur_backend = pick_backend(1);

    // FPS tracking.
    uint64_t fps_start  = SDL_GetPerformanceCounter();
    int      fps_frames = 0;
    double   fps        = 0.0;

    // Rect animation state.
    float rect_w = 200.0f, rect_h = 150.0f;
    float rx = 100.0f, ry = 80.0f;
    float vx = 1.5f,   vy = 1.1f;

    // Perspective animation time.
    float persp_t = 0.0f;

    update_title(window, &slides[cur_slide], cur_backend, fps);

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
                } else if (ev.key.key == SDLK_B) {
                    cur_backend = next_backend(cur_backend);
                    update_title(window, &slides[cur_slide], cur_backend, fps);
                } else if (ev.key.key == SDLK_ESCAPE) {
                    running = 0;
                }
                if (next != cur_slide) {
                    cur_slide = next;
                    build_slide(&slides[cur_slide]);
                    cur_backend = pick_backend(cur_backend);
                    update_title(window, &slides[cur_slide], cur_backend, fps);
                }
            }
        }
        if (!running) { break; }

        slide const *s = &slides[cur_slide];
        run_fn run = run_fns[cur_backend];
        void  *ctx = backends[cur_backend];

        __fp16 color[4] = {
            (__fp16)s->color[0],
            (__fp16)s->color[1],
            (__fp16)s->color[2],
            (__fp16)s->color[3],
        };

        void *tex_pixels;
        int   tex_pitch;
        if (!SDL_LockTexture(texture, NULL, &tex_pixels, &tex_pitch)) {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
            break;
        }

        uint8_t *rows = (uint8_t*)tex_pixels;
        for (int y = 0; y < H; y++) {
            uint32_t *row = (uint32_t*)(rows + y * tex_pitch);
            for (int x = 0; x < W; x++) {
                row[x] = s->bg;
            }
        }

        int32_t x0 = 0;

        if (s->coverage == umbra_coverage_bitmap_matrix) {
            persp_t += 0.016f;
            float mat[11];
            build_perspective_matrix(mat, persp_t, W, H, bitmap_cov.w, bitmap_cov.h);
            for (int y = 0; y < H; y++) {
                int32_t yval = y;
                uint32_t *row = (uint32_t*)(rows + y * tex_pitch);
                umbra_buf buf[] = {
                    { row,            W * 4               },
                    { &x0,           -4                   },
                    { &yval,         -4                   },
                    { color,         -8                   },
                    { bitmap_cov.data, -(W * H * 2)       },
                    { mat,           -(int)(sizeof mat)    },
                };
                run(ctx, W, buf);
            }
        } else if (s->coverage == umbra_coverage_bitmap || s->coverage == umbra_coverage_sdf) {
            text_cov *tc = (s->coverage == umbra_coverage_bitmap) ? &bitmap_cov : &sdf_cov;
            for (int y = 0; y < H; y++) {
                int32_t yval = y;
                uint32_t *row = (uint32_t*)(rows + y * tex_pitch);
                umbra_buf buf[] = {
                    { row,              W * 4     },
                    { &x0,             -4         },
                    { &yval,           -4         },
                    { color,           -8         },
                    { tc->data + y * W, -(W * 2)  },
                };
                run(ctx, W, buf);
            }
        } else {
            rx += vx;
            ry += vy;
            if (rx < 0.0f)              { rx = 0.0f;              vx = -vx; }
            if (rx + rect_w > (float)W) { rx = (float)W - rect_w; vx = -vx; }
            if (ry < 0.0f)              { ry = 0.0f;              vy = -vy; }
            if (ry + rect_h > (float)H) { ry = (float)H - rect_h; vy = -vy; }

            float rect[4] = { rx, ry, rx + rect_w, ry + rect_h };

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
                run(ctx, W, buf);
            }
        }

        SDL_UnlockTexture(texture);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        // Update FPS every ~0.5 seconds.
        fps_frames++;
        uint64_t now = SDL_GetPerformanceCounter();
        uint64_t freq = SDL_GetPerformanceFrequency();
        double elapsed = (double)(now - fps_start) / (double)freq;
        if (elapsed >= 0.5) {
            fps = (double)fps_frames / elapsed;
            fps_frames = 0;
            fps_start  = now;
            update_title(window, &slides[cur_slide], cur_backend, fps);
        }
    }

    text_cov_free(&bitmap_cov);
    text_cov_free(&sdf_cov);
    free_backends();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#endif
