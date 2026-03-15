#include "slides/slides.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__wasm__)

int main(void) { return 0; }

#else

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
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

enum { FMT_8888, FMT_565, FMT_FP16, FMT_FP16P, FMT_1010102, NUM_FMTS };
static char const     *fmt_name[]  = {"8888",           "565",           "fp16",           "fp16p",                "1010102"};
static umbra_load_fn   fmt_load[]  = {umbra_load_8888,  umbra_load_565,  umbra_load_fp16,  umbra_load_fp16_planar,  umbra_load_1010102};
static umbra_store_fn  fmt_store[] = {umbra_store_8888, umbra_store_565, umbra_store_fp16, umbra_store_fp16_planar, umbra_store_1010102};
static int             fmt_bpp[]   = {4,                2,               8,                2,                       4};

static void build_slide_fmt(slide const *s, int fmt) {
    free_backends();
    umbra_load_fn  load  = s->load  ? fmt_load[fmt]  : NULL;
    umbra_store_fn store = fmt_store[fmt];

    struct umbra_basic_block *bb = umbra_draw_build(
        s->shader, s->coverage, s->blend, load, store);
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

static void build_perspective_matrix(float out[11], float t, int sw, int sh, int bw, int bh) {
    float cx = (float)sw * 0.5f, cy = (float)sh * 0.5f;
    float bx = (float)bw * 0.5f, by = (float)bh * 0.5f;
    float angle = t * 0.3f;
    float tilt  = sinf(t * 0.7f) * 0.0008f;
    float sc    = 1.0f + 0.2f * sinf(t * 0.5f);
    float ca = cosf(angle), sa = sinf(angle);
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

static void update_title(SDL_Window *w, slide const *s, int bi, int fi, double fps) {
    char title[256];
    SDL_snprintf(title, sizeof title, "%s  [%s/%s]  %.0f fps",
                 s->title, backend_name[bi], fmt_name[fi], fps);
    SDL_SetWindowTitle(w, title);
}

enum { LUT_N = 64 };
static __fp16 linear_lut[LUT_N * 4];
static __fp16 radial_lut[LUT_N * 4];

static void build_luts(void) {
    __fp16 const linear_stops[][4] = {
        {(__fp16)1.2f, (__fp16)0.0f, (__fp16)0.0f, (__fp16)1.0f},
        {(__fp16)1.0f, (__fp16)0.8f, (__fp16)0.0f, (__fp16)1.0f},
        {(__fp16)0.0f, (__fp16)1.2f, (__fp16)0.0f, (__fp16)1.0f},
        {(__fp16)0.0f, (__fp16)0.8f, (__fp16)1.2f, (__fp16)1.0f},
        {(__fp16)0.0f, (__fp16)0.0f, (__fp16)1.2f, (__fp16)1.0f},
        {(__fp16)0.8f, (__fp16)0.0f, (__fp16)1.0f, (__fp16)1.0f},
    };
    umbra_gradient_lut_even(linear_lut, LUT_N, 6, linear_stops);

    __fp16 const radial_stops[][4] = {
        {(__fp16)1.5f, (__fp16)1.5f, (__fp16)1.2f, (__fp16)1.0f},
        {(__fp16)1.2f, (__fp16)0.8f, (__fp16)0.0f, (__fp16)1.0f},
        {(__fp16)0.8f, (__fp16)0.0f, (__fp16)0.2f, (__fp16)1.0f},
        {(__fp16)0.05f, (__fp16)0.0f, (__fp16)0.15f, (__fp16)1.0f},
    };
    umbra_gradient_lut_even(radial_lut, LUT_N, 4, radial_stops);
}

static void fill_bg_row(void *dst, int n, uint32_t bg, int fmt, int stride) {
    uint8_t r = (uint8_t)(bg >>  0);
    uint8_t g = (uint8_t)(bg >>  8);
    uint8_t b = (uint8_t)(bg >> 16);
    uint8_t a = (uint8_t)(bg >> 24);
    switch (fmt) {
    case FMT_8888:
        for (int i = 0; i < n; i++) { ((uint32_t*)dst)[i] = bg; }
        break;
    case FMT_565: {
        uint16_t px = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        for (int i = 0; i < n; i++) { ((uint16_t*)dst)[i] = px; }
    } break;
    case FMT_FP16: {
        __fp16 hc[4] = {(__fp16)(r/255.0f), (__fp16)(g/255.0f),
                        (__fp16)(b/255.0f), (__fp16)(a/255.0f)};
        for (int i = 0; i < n; i++) { __builtin_memcpy((__fp16*)dst + i*4, hc, 8); }
    } break;
    case FMT_FP16P: {
        __fp16 hr = (__fp16)(r/255.0f), hg = (__fp16)(g/255.0f);
        __fp16 hb = (__fp16)(b/255.0f), ha = (__fp16)(a/255.0f);
        __fp16 *p = dst;
        for (int i = 0; i < n; i++) {
            p[i]            = hr;
            p[i + stride]   = hg;
            p[i + stride*2] = hb;
            p[i + stride*3] = ha;
        }
    } break;
    case FMT_1010102: {
        uint32_t px = ((uint32_t)(r * 1023 / 255))
                    | ((uint32_t)(g * 1023 / 255) << 10)
                    | ((uint32_t)(b * 1023 / 255) << 20)
                    | ((uint32_t)(a * 3 / 255) << 30);
        for (int i = 0; i < n; i++) { ((uint32_t*)dst)[i] = px; }
    } break;
    }
}

static void readback_row(uint32_t *dst, void const *src, int n, int fmt, int stride) {
    switch (fmt) {
    case FMT_8888:
        __builtin_memcpy(dst, src, (size_t)(n * 4));
        break;
    case FMT_565: {
        uint16_t const *s = src;
        for (int i = 0; i < n; i++) {
            uint32_t ri = ((s[i] >> 11) & 0x1fu) * 255 / 31;
            uint32_t gi = ((s[i] >>  5) & 0x3fu) * 255 / 63;
            uint32_t bi = ( s[i]        & 0x1fu) * 255 / 31;
            dst[i] = ri | (gi << 8) | (bi << 16) | 0xff000000u;
        }
    } break;
    case FMT_FP16: {
        __fp16 const *s = src;
        for (int i = 0; i < n; i++) {
            float rf = (float)s[i*4+0], gf = (float)s[i*4+1];
            float bf = (float)s[i*4+2], af = (float)s[i*4+3];
            if (rf < 0) { rf = 0; } if (rf > 1) { rf = 1; }
            if (gf < 0) { gf = 0; } if (gf > 1) { gf = 1; }
            if (bf < 0) { bf = 0; } if (bf > 1) { bf = 1; }
            if (af < 0) { af = 0; } if (af > 1) { af = 1; }
            dst[i] = (uint32_t)(rf*255+.5f) | ((uint32_t)(gf*255+.5f) << 8)
                   | ((uint32_t)(bf*255+.5f) << 16) | ((uint32_t)(af*255+.5f) << 24);
        }
    } break;
    case FMT_FP16P: {
        __fp16 const *s = src;
        for (int i = 0; i < n; i++) {
            float rf = (float)s[i],            gf = (float)s[i + stride];
            float bf = (float)s[i + stride*2], af = (float)s[i + stride*3];
            if (rf < 0) { rf = 0; } if (rf > 1) { rf = 1; }
            if (gf < 0) { gf = 0; } if (gf > 1) { gf = 1; }
            if (bf < 0) { bf = 0; } if (bf > 1) { bf = 1; }
            if (af < 0) { af = 0; } if (af > 1) { af = 1; }
            dst[i] = (uint32_t)(rf*255+.5f) | ((uint32_t)(gf*255+.5f) << 8)
                   | ((uint32_t)(bf*255+.5f) << 16) | ((uint32_t)(af*255+.5f) << 24);
        }
    } break;
    case FMT_1010102: {
        uint32_t const *s = src;
        for (int i = 0; i < n; i++) {
            uint32_t ri = ((s[i] >>  0) & 0x3ffu) * 255 / 1023;
            uint32_t gi = ((s[i] >> 10) & 0x3ffu) * 255 / 1023;
            uint32_t bi = ((s[i] >> 20) & 0x3ffu) * 255 / 1023;
            uint32_t ai = ((s[i] >> 30) & 0x3u)   * 255 / 3;
            dst[i] = ri | (gi << 8) | (bi << 16) | (ai << 24);
        }
    } break;
    }
}

static void to_hdr_row(float *dst, void const *src, int n, int fmt, int stride) {
    switch (fmt) {
    case FMT_8888: {
        uint8_t const *s = src;
        for (int i = 0; i < n; i++) {
            dst[i*4+0] = s[i*4+0] / 255.0f;
            dst[i*4+1] = s[i*4+1] / 255.0f;
            dst[i*4+2] = s[i*4+2] / 255.0f;
            dst[i*4+3] = 1.0f;
        }
    } break;
    case FMT_565: {
        uint16_t const *s = src;
        for (int i = 0; i < n; i++) {
            dst[i*4+0] = ((s[i] >> 11) & 0x1f) / 31.0f;
            dst[i*4+1] = ((s[i] >>  5) & 0x3f) / 63.0f;
            dst[i*4+2] = ( s[i]        & 0x1f) / 31.0f;
            dst[i*4+3] = 1.0f;
        }
    } break;
    case FMT_FP16: {
        __fp16 const *s = src;
        for (int i = 0; i < n*4; i++) { dst[i] = (float)s[i]; }
    } break;
    case FMT_FP16P: {
        __fp16 const *s = src;
        for (int i = 0; i < n; i++) {
            dst[i*4+0] = (float)s[i];
            dst[i*4+1] = (float)s[i + stride];
            dst[i*4+2] = (float)s[i + stride*2];
            dst[i*4+3] = (float)s[i + stride*3];
        }
    } break;
    case FMT_1010102: {
        uint32_t const *s = src;
        for (int i = 0; i < n; i++) {
            dst[i*4+0] = ((s[i] >>  0) & 0x3ff) / 1023.0f;
            dst[i*4+1] = ((s[i] >> 10) & 0x3ff) / 1023.0f;
            dst[i*4+2] = ((s[i] >> 20) & 0x3ff) / 1023.0f;
            dst[i*4+3] = ((s[i] >> 30) & 0x3)   / 3.0f;
        }
    } break;
    }
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
    build_luts();

    void *pixbuf = malloc(W * H * 8);
    int32_t planar_stride = W * H;

    int cur_slide   = 0;
    int cur_fmt     = FMT_8888;
    build_slide_fmt(&slides[cur_slide], cur_fmt);
    int cur_backend = pick_backend(1);

    uint64_t fps_start  = SDL_GetPerformanceCounter();
    int      fps_frames = 0;
    double   fps        = 0.0;

    float rect_w = 200.0f, rect_h = 150.0f;
    float rx = 100.0f, ry = 80.0f;
    float vx = 1.5f,   vy = 1.1f;

    float persp_t = 0.0f;

    _Bool want_dump = 0;

    update_title(window, &slides[cur_slide], cur_backend, cur_fmt, fps);

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
                } else if (ev.key.key == SDLK_C) {
                    cur_fmt = (cur_fmt + 1) % NUM_FMTS;
                    build_slide_fmt(&slides[cur_slide], cur_fmt);
                    cur_backend = pick_backend(cur_backend);
                } else if (ev.key.key == SDLK_S) {
                    want_dump = 1;
                } else if (ev.key.key == SDLK_ESCAPE) {
                    running = 0;
                }
                if (next != cur_slide) {
                    cur_slide = next;
                    build_slide_fmt(&slides[cur_slide], cur_fmt);
                    cur_backend = pick_backend(cur_backend);
                }
                update_title(window, &slides[cur_slide], cur_backend, cur_fmt, fps);
            }
        }
        if (!running) { break; }

        slide const *s = &slides[cur_slide];
        run_fn run = run_fns[cur_backend];
        void  *ctx = backends[cur_backend];

        int bpp    = fmt_bpp[cur_fmt];
        _Bool planar = (cur_fmt == FMT_FP16P);

        #define ROW(y) (planar ? (void*)((__fp16*)pixbuf + (y) * W) \
                               : (void*)((uint8_t*)pixbuf + (y) * W * bpp))
        long row_sz = planar ? (long)(W * H * 4) * 2 : (long)(W * bpp);

        for (int y = 0; y < H; y++) {
            fill_bg_row(ROW(y), W, s->bg, cur_fmt, planar_stride);
        }

        int32_t x0 = 0;

        if (s->coverage == umbra_coverage_bitmap_matrix) {
            persp_t += 0.016f;
            float mat[11];
            build_perspective_matrix(mat, persp_t, W, H, bitmap_cov.w, bitmap_cov.h);
            __fp16 hc[4];
            for (int i = 0; i < 4; i++) { hc[i] = (__fp16)s->color[i]; }
            for (int y = 0; y < H; y++) {
                int32_t yval = y;
                umbra_buf buf[] = {
                    { ROW(y),          row_sz              },
                    { &x0,           -4                   },
                    { &yval,         -4                   },
                    { hc,            -8                   },
                    { bitmap_cov.data, -(W * H * 2)       },
                    { mat,           -(int)(sizeof mat)    },
                    { &planar_stride, -4                   },
                };
                run(ctx, W, buf);
            }
        } else if (s->coverage == umbra_coverage_bitmap || s->coverage == umbra_coverage_sdf) {
            text_cov *tc = (s->coverage == umbra_coverage_bitmap) ? &bitmap_cov : &sdf_cov;
            __fp16 hc[4];
            for (int i = 0; i < 4; i++) { hc[i] = (__fp16)s->color[i]; }
            for (int y = 0; y < H; y++) {
                int32_t yval = y;
                umbra_buf buf[] = {
                    { ROW(y),           row_sz    },
                    { &x0,             -4         },
                    { &yval,           -4         },
                    { hc,              -8         },
                    { tc->data + y * W, -(W * 2)  },
                    { NULL,             0         },
                    { &planar_stride,  -4         },
                };
                run(ctx, W, buf);
            }
        } else if (s->shader != umbra_shader_solid) {
            __fp16 hc[8];
            for (int i = 0; i < 8; i++) { hc[i] = (__fp16)s->color[i]; }

            _Bool is_lut = (s->shader == umbra_shader_linear_grad ||
                            s->shader == umbra_shader_radial_grad);
            __fp16 *p3   = is_lut ? (s->shader == umbra_shader_linear_grad ? linear_lut
                                                                           : radial_lut)
                                  : hc;
            int     p3sz = is_lut ? (int)(LUT_N * 4 * 2) : 16;
            float   gp[4] = {s->grad[0], s->grad[1], s->grad[2], s->grad[3]};

            for (int y = 0; y < H; y++) {
                int32_t yval = y;
                umbra_buf buf[] = {
                    {ROW(y), row_sz},
                    {&x0,  -4},
                    {&yval, -4},
                    {p3,   -p3sz},
                    {NULL,  0},
                    {gp,   -(int)sizeof gp},
                    {&planar_stride, -4},
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

            __fp16 hc[4];
            for (int i = 0; i < 4; i++) { hc[i] = (__fp16)s->color[i]; }

            for (int y = 0; y < H; y++) {
                int32_t yval = y;
                umbra_buf buf[] = {
                    { ROW(y), row_sz },
                    { &x0,   -4      },
                    { &yval, -4      },
                    { hc,    -8      },
                    { rect,  -16     },
                    { NULL,   0      },
                    { &planar_stride, -4 },
                };
                run(ctx, W, buf);
            }
        }
        #undef ROW

        void *tex_pixels;
        int   tex_pitch;
        if (!SDL_LockTexture(texture, NULL, &tex_pixels, &tex_pitch)) {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
            break;
        }

        uint8_t *rows = (uint8_t*)tex_pixels;
        for (int y = 0; y < H; y++) {
            void *src = planar ? (void*)((__fp16*)pixbuf + y * W)
                               : (void*)((uint8_t*)pixbuf + y * W * bpp);
            readback_row((uint32_t*)(rows + y * tex_pitch), src, W, cur_fmt, planar_stride);
        }

        if (want_dump) {
            float *fdata = malloc((size_t)(W * H) * 4 * sizeof(float));
            for (int y = 0; y < H; y++) {
                void *src = planar ? (void*)((__fp16*)pixbuf + y * W)
                                   : (void*)((uint8_t*)pixbuf + y * W * bpp);
                to_hdr_row(fdata + y * W * 4, src, W, cur_fmt, planar_stride);
            }
            stbi_write_hdr("dump.hdr", W, H, 4, fdata);
            SDL_Log("saved dump.hdr (%s)", fmt_name[cur_fmt]);
            free(fdata);
            want_dump = 0;
        }

        SDL_UnlockTexture(texture);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        fps_frames++;
        uint64_t now = SDL_GetPerformanceCounter();
        uint64_t freq = SDL_GetPerformanceFrequency();
        double elapsed = (double)(now - fps_start) / (double)freq;
        if (elapsed >= 0.5) {
            fps = (double)fps_frames / elapsed;
            fps_frames = 0;
            fps_start  = now;
            update_title(window, &slides[cur_slide], cur_backend, cur_fmt, fps);
        }
    }

    free(pixbuf);
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
