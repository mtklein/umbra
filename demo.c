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
#include <SDL3/SDL.h>
#pragma clang diagnostic pop
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "stb/stb_image_write.h"
#pragma clang diagnostic pop

typedef struct umbra_builder BB;

enum { NUM_BACKENDS = 4 };
static char const *backend_name[NUM_BACKENDS] = {
    "interp", "jit", "codegen", "metal",
};

typedef void (*run_fn)(void*, int, umbra_buf[]);

static void run_interp(void *ctx, int n,
                       umbra_buf buf[]) {
    umbra_interpreter_run(ctx, n, buf);
}
static void run_jit(void *ctx, int n,
                    umbra_buf buf[]) {
    umbra_jit_run(ctx, n, buf);
}
static void run_codegen(void *ctx, int n,
                        umbra_buf buf[]) {
    umbra_codegen_run(ctx, n, buf);
}
static void run_metal(void *ctx, int n,
                      umbra_buf buf[]) {
    umbra_metal_run(ctx, n, buf);
}

static run_fn const run_fns[NUM_BACKENDS] = {
    run_interp, run_jit, run_codegen, run_metal,
};

static struct umbra_interpreter *interp;
static struct umbra_jit         *jit;
static struct umbra_codegen     *codegen;
static struct umbra_metal       *mtl;

static void             *backends[NUM_BACKENDS];
static umbra_draw_layout draw_layout;

static void free_backends(void) {
    if (interp) {
        umbra_interpreter_free(interp);
        interp = NULL;
    }
    if (jit) {
        umbra_jit_free(jit);
        jit = NULL;
    }
    if (codegen) {
        umbra_codegen_free(codegen);
        codegen = NULL;
    }
    if (mtl) {
        umbra_metal_free(mtl);
        mtl = NULL;
    }
    for (int i = 0; i < NUM_BACKENDS; i++) {
        backends[i] = NULL;
    }
}

enum {
    FMT_8888, FMT_565, FMT_FP16,
    FMT_FP16P, FMT_1010102, NUM_FMTS,
};
static char const *fmt_name[] = {
    "8888", "565", "fp16", "fp16p", "1010102",
};
static umbra_load_fn fmt_load[] = {
    umbra_load_8888,  umbra_load_565,
    umbra_load_fp16,  umbra_load_fp16_planar,
    umbra_load_1010102,
};
static umbra_store_fn fmt_store[] = {
    umbra_store_8888, umbra_store_565,
    umbra_store_fp16, umbra_store_fp16_planar,
    umbra_store_1010102,
};
static int fmt_bpp[] = {4, 2, 8, 2, 4};

typedef struct {
    struct umbra_interpreter *interp;
    struct umbra_jit         *jit;
    void                     *ctx;
    run_fn                    run;
    int                       uni_len;
    int                       pad_;
} pipe;

static pipe fill_pipe, readback_pipe, hdr_pipe;

static void free_pipe(pipe *p) {
    if (p->interp) {
        umbra_interpreter_free(p->interp);
    }
    if (p->jit) { umbra_jit_free(p->jit); }
    *p = (pipe){0};
}

static void finish_pipe(pipe *p, BB *b) {
    p->uni_len = umbra_uni_len(b);
    struct umbra_basic_block *bb = umbra_basic_block(b);
    umbra_builder_free(b);
    p->interp  = umbra_interpreter(bb);
    p->jit     = umbra_jit(bb);
    p->ctx = p->jit
        ? (void*)p->jit : (void*)p->interp;
    p->run = p->jit ? run_jit : run_interp;
    umbra_basic_block_free(bb);
}

static void build_fill(int fmt) {
    free_pipe(&fill_pipe);
    BB *bb = umbra_builder();
    umbra_val ix = umbra_lane(bb);
    int fi = umbra_reserve(bb, 4);
    umbra_color c = {
        umbra_load_i32(bb, (umbra_ptr){1},
                     umbra_imm_i32(bb, fi)),
        umbra_load_i32(bb, (umbra_ptr){1},
                     umbra_imm_i32(bb, fi+1)),
        umbra_load_i32(bb, (umbra_ptr){1},
                     umbra_imm_i32(bb, fi+2)),
        umbra_load_i32(bb, (umbra_ptr){1},
                     umbra_imm_i32(bb, fi+3)),
    };
    fmt_store[fmt](bb, (umbra_ptr){0}, ix, c);
    finish_pipe(&fill_pipe, bb);
}

static void build_readback(int fmt) {
    free_pipe(&readback_pipe);
    BB *bb = umbra_builder();
    umbra_val ix = umbra_lane(bb);
    umbra_color c =
        fmt_load[fmt](bb, (umbra_ptr){0}, ix);
    umbra_store_8888(bb, (umbra_ptr){2}, ix, c);
    finish_pipe(&readback_pipe, bb);
}

static void build_hdr(int fmt) {
    free_pipe(&hdr_pipe);
    BB *bb = umbra_builder();
    umbra_val ix = umbra_lane(bb);
    umbra_color c =
        fmt_load[fmt](bb, (umbra_ptr){0}, ix);
    umbra_val ix4 = umbra_shl_i32(bb, ix,
                        umbra_imm_i32(bb, 2));
    umbra_store_i32(bb, (umbra_ptr){2},
        umbra_add_i32(bb, ix4,
                   umbra_imm_i32(bb, 0)), c.r);
    umbra_store_i32(bb, (umbra_ptr){2},
        umbra_add_i32(bb, ix4,
                   umbra_imm_i32(bb, 1)), c.g);
    umbra_store_i32(bb, (umbra_ptr){2},
        umbra_add_i32(bb, ix4,
                   umbra_imm_i32(bb, 2)), c.b);
    umbra_store_i32(bb, (umbra_ptr){2},
        umbra_add_i32(bb, ix4,
                   umbra_imm_i32(bb, 3)), c.a);
    finish_pipe(&hdr_pipe, bb);
}

static void build_pipes(int fmt) {
    build_fill(fmt);
    build_readback(fmt);
    build_hdr(fmt);
}

static void free_pipes(void) {
    free_pipe(&fill_pipe);
    free_pipe(&readback_pipe);
    free_pipe(&hdr_pipe);
}

static void uni_i32(char *u, int off, int32_t v) {
    __builtin_memcpy(u+off, &v, 4);
}
static void uni_f32(char *u, int off,
                    float const *v, int n) {
    __builtin_memcpy(u+off, v, (unsigned long)n*4);
}
static void uni_ptr(char *u, int off,
                    void *p, long sz) {
    __builtin_memcpy(u+off,   &p,  8);
    __builtin_memcpy(u+off+8, &sz, 8);
}

static void build_slide_fmt(slide const *s,
                            int fmt) {
    free_backends();
    umbra_load_fn  load =
        s->load ? fmt_load[fmt] : NULL;
    umbra_store_fn store = fmt_store[fmt];

    BB *b = umbra_draw_build(
        s->shader, s->coverage, s->blend,
        load, store, &draw_layout);
    struct umbra_basic_block *bb = umbra_basic_block(b);
    umbra_builder_free(b);

    interp  = umbra_interpreter(bb);
    jit     = umbra_jit(bb);
    codegen = umbra_codegen(bb);
    mtl     = umbra_metal(bb);
    umbra_basic_block_free(bb);

    backends[0] = interp;
    backends[1] = jit;
    backends[2] = codegen;
    backends[3] = mtl;

    build_pipes(fmt);
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

static void build_perspective_matrix(
        float out[11], float t,
        int sw, int sh, int bw, int bh) {
    float cx = (float)sw * 0.5f;
    float cy = (float)sh * 0.5f;
    float bx = (float)bw * 0.5f;
    float by = (float)bh * 0.5f;
    float angle = t * 0.3f;
    float tilt  = sinf(t * 0.7f) * 0.0008f;
    float sc    = 1.0f + 0.2f * sinf(t * 0.5f);
    float ca = cosf(angle), sa = sinf(angle);
    float w0 = 1.0f - tilt * cx;

    out[0] = ca * sc;
    out[1] = sa * sc;
    out[2] = -cx*ca*sc - cy*sa*sc + bx*w0;
    out[3] = -sa * sc;
    out[4] = ca * sc;
    out[5] = cx*sa*sc - cy*ca*sc + by*w0;
    out[6] = tilt;
    out[7] = 0.0f;
    out[8] = w0;
    out[9]  = (float)bw;
    out[10] = (float)bh;
}

static void update_title(SDL_Window *w,
                         slide const *s,
                         int bi, int fi,
                         double fps) {
    char title[256];
    SDL_snprintf(title, sizeof title,
        "%s  [%s/%s]  %.0f fps",
        s->title, backend_name[bi],
        fmt_name[fi], fps);
    SDL_SetWindowTitle(w, title);
}

enum { LUT_N = 64 };
static float linear_lut[LUT_N * 4];
static float radial_lut[LUT_N * 4];

static void build_luts(void) {
    float const linear_stops[][4] = {
        {1.2f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.8f, 0.0f, 1.0f},
        {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f},
        {0.0f, 0.0f, 1.2f, 1.0f},
        {0.8f, 0.0f, 1.0f, 1.0f},
    };
    umbra_gradient_lut_even(linear_lut, LUT_N,
                            6, linear_stops);

    float const radial_stops[][4] = {
        {1.5f, 1.5f, 1.2f, 1.0f},
        {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f},
        {0.05f, 0.0f, 0.15f, 1.0f},
    };
    umbra_gradient_lut_even(radial_lut, LUT_N,
                            4, radial_stops);
}

static void fill_bg_row(void *dst, int n,
                        uint32_t bg,
                        long row_sz,
                        int32_t stride) {
    float hc[4] = {
        (float)( bg        & 0xffu) / 255.0f,
        (float)((bg >>  8) & 0xffu) / 255.0f,
        (float)((bg >> 16) & 0xffu) / 255.0f,
        (float)((bg >> 24) & 0xffu) / 255.0f,
    };
    long long uni_[4] = {0};
    char *uni = (char*)uni_;
    uni_f32(uni, 0, hc, 4);
    if (fill_pipe.uni_len > 16) {
        uni_i32(uni, 16, stride);
    }
    umbra_buf buf[] = {
        { dst,  row_sz },
        { uni, -(long)fill_pipe.uni_len },
    };
    fill_pipe.run(fill_pipe.ctx, n, buf);
}

static void readback_row(uint32_t *dst,
                         void *src, int n,
                         long src_sz,
                         int32_t stride) {
    long long uni_[2] = {0};
    char *uni = (char*)uni_;
    if (readback_pipe.uni_len > 0) {
        uni_i32(uni, 0, stride);
    }
    umbra_buf buf[] = {
        { src,  -src_sz },
        { uni,  -(long)readback_pipe.uni_len },
        { dst,  (long)(n * 4) },
    };
    readback_pipe.run(readback_pipe.ctx, n, buf);
}

static void to_hdr_row(float *dst, void *src,
                       int n, long src_sz,
                       int32_t stride) {
    long long uni_[2] = {0};
    char *uni = (char*)uni_;
    if (hdr_pipe.uni_len > 0) {
        uni_i32(uni, 0, stride);
    }
    umbra_buf buf[] = {
        { src,  -src_sz },
        { uni,  -(long)hdr_pipe.uni_len },
        { dst,  (long)(n * 16) },
    };
    hdr_pipe.run(hdr_pipe.ctx, n, buf);
}

int main(void) {
    enum { W = 640, H = 480 };

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s",
                SDL_GetError());
        return 1;
    }

    SDL_Window *window =
        SDL_CreateWindow("umbra demo", W, H, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s",
                SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s",
                SDL_GetError());
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, W, H);
    if (!texture) {
        SDL_Log("SDL_CreateTexture failed: %s",
                SDL_GetError());
        return 1;
    }

    text_cov bitmap_cov =
        text_rasterize(W, H, 72.0f, 0);
    text_cov sdf_cov =
        text_rasterize(W, H, 72.0f, 1);
    build_luts();

    void *pixbuf = malloc(W * H * 8);
    int32_t planar_stride = W * H;

    int cur_slide   = 0;
    int cur_fmt     = FMT_8888;
    build_slide_fmt(&slides[cur_slide], cur_fmt);
    int cur_backend = pick_backend(1);

    uint64_t fps_start  =
        SDL_GetPerformanceCounter();
    int      fps_frames = 0;
    double   fps        = 0.0;

    float rect_w = 200.0f, rect_h = 150.0f;
    float rx = 100.0f, ry = 80.0f;
    float vx = 1.5f,   vy = 1.1f;

    float persp_t = 0.0f;

    _Bool want_dump = 0;

    update_title(window, &slides[cur_slide],
                 cur_backend, cur_fmt, fps);

    _Bool running = 1;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = 0;
            } else if (ev.type ==
                           SDL_EVENT_KEY_DOWN) {
                int next = cur_slide;
                if (ev.key.key == SDLK_RIGHT
                 || ev.key.key == SDLK_SPACE) {
                    next = (cur_slide + 1)
                         % SLIDE_COUNT;
                } else if (ev.key.key ==
                               SDLK_LEFT) {
                    next = (cur_slide
                            + SLIDE_COUNT - 1)
                         % SLIDE_COUNT;
                } else if (ev.key.key == SDLK_B) {
                    cur_backend =
                        next_backend(cur_backend);
                } else if (ev.key.key == SDLK_C) {
                    cur_fmt =
                        (cur_fmt + 1) % NUM_FMTS;
                    build_slide_fmt(
                        &slides[cur_slide],
                        cur_fmt);
                    cur_backend =
                        pick_backend(cur_backend);
                } else if (ev.key.key == SDLK_S) {
                    want_dump = 1;
                } else if (ev.key.key ==
                               SDLK_ESCAPE) {
                    running = 0;
                }
                if (next != cur_slide) {
                    cur_slide = next;
                    build_slide_fmt(
                        &slides[cur_slide],
                        cur_fmt);
                    cur_backend =
                        pick_backend(cur_backend);
                }
                update_title(window,
                    &slides[cur_slide],
                    cur_backend, cur_fmt, fps);
            }
        }
        if (!running) { break; }

        slide const *s = &slides[cur_slide];
        run_fn run = run_fns[cur_backend];
        void  *ctx = backends[cur_backend];

        int bpp = fmt_bpp[cur_fmt];
        _Bool planar = (cur_fmt == FMT_FP16P);

        #define ROW(y) (planar               \
            ? (void*)((__fp16*)pixbuf         \
                      + (y) * W)             \
            : (void*)((uint8_t*)pixbuf       \
                      + (y) * W * bpp))
        long row_sz = planar
            ? (long)(W * H * 4) * 2
            : (long)(W * bpp);

        for (int y = 0; y < H; y++) {
            fill_bg_row(ROW(y), W, s->bg,
                        row_sz, planar_stride);
        }

        int uni_len = draw_layout.uni_len;
        int ps = planar
            ? (s->load ? 2 : 1) : 0;

        if (cur_backend == 3 && mtl) {
            umbra_metal_begin_batch(mtl);
        }

        if (s->coverage ==
                umbra_coverage_bitmap_matrix) {
            persp_t += 0.016f;
            float mat[11];
            build_perspective_matrix(mat, persp_t,
                W, H,
                bitmap_cov.w, bitmap_cov.h);
            float hc[4];
            for (int i = 0; i < 4; i++) {
                hc[i] = s->color[i];
            }
            for (int y = 0; y < H; y++) {
                long long uni_[12] = {0};
                char *uni = (char*)uni_;
                uni_i32(uni,
                    draw_layout.x0, 0);
                uni_i32(uni,
                    draw_layout.y, y);
                uni_f32(uni,
                    draw_layout.shader, hc, 4);
                uni_f32(uni,
                    draw_layout.coverage,
                    mat, 11);
                uni_ptr(uni,
                    (draw_layout.coverage
                     + 11*4 + 7) & ~7,
                    bitmap_cov.data,
                    (long)(W * H * 2));
                for (int i = 0; i < ps; i++) {
                    uni_i32(uni,
                        uni_len - (ps-i) * 4,
                        planar_stride);
                }
                umbra_buf buf[] = {
                    { ROW(y), row_sz },
                    { uni, -(long)uni_len },
                };
                run(ctx, W, buf);
            }
        } else if (
                s->coverage ==
                    umbra_coverage_bitmap
             || s->coverage ==
                    umbra_coverage_sdf) {
            text_cov *tc =
                (s->coverage ==
                     umbra_coverage_bitmap)
                    ? &bitmap_cov : &sdf_cov;
            float hc[4];
            for (int i = 0; i < 4; i++) {
                hc[i] = s->color[i];
            }
            for (int y = 0; y < H; y++) {
                long long uni_[6] = {0};
                char *uni = (char*)uni_;
                uni_i32(uni,
                    draw_layout.x0, 0);
                uni_i32(uni,
                    draw_layout.y, y);
                uni_f32(uni,
                    draw_layout.shader, hc, 4);
                uni_ptr(uni,
                    draw_layout.coverage,
                    tc->data + y * W,
                    (long)(W * 2));
                for (int i = 0; i < ps; i++) {
                    uni_i32(uni,
                        uni_len - (ps-i) * 4,
                        planar_stride);
                }
                umbra_buf buf[] = {
                    { ROW(y), row_sz },
                    { uni, -(long)uni_len },
                };
                run(ctx, W, buf);
            }
        } else if (s->shader !=
                       umbra_shader_solid) {
            float hc[8];
            for (int i = 0; i < 8; i++) {
                hc[i] = s->color[i];
            }

            _Bool is_lut =
                (s->shader ==
                     umbra_shader_linear_grad
              || s->shader ==
                     umbra_shader_radial_grad);
            float gp[4] = {
                s->grad[0], s->grad[1],
                s->grad[2], s->grad[3],
            };

            for (int y = 0; y < H; y++) {
                long long uni_[8] = {0};
                char *uni = (char*)uni_;
                uni_i32(uni,
                    draw_layout.x0, 0);
                uni_i32(uni,
                    draw_layout.y, y);
                if (is_lut) {
                    float *lut =
                        (s->shader ==
                         umbra_shader_linear_grad)
                            ? linear_lut
                            : radial_lut;
                    uni_f32(uni,
                        draw_layout.shader,
                        gp, 4);
                    uni_ptr(uni,
                        (draw_layout.shader
                         + 16 + 7) & ~7,
                        lut,
                        (long)(LUT_N * 4 * 4));
                } else {
                    uni_f32(uni,
                        draw_layout.shader,
                        gp, 3);
                    uni_f32(uni,
                        draw_layout.shader + 12,
                        hc, 8);
                }
                for (int i = 0; i < ps; i++) {
                    uni_i32(uni,
                        uni_len - (ps-i) * 4,
                        planar_stride);
                }
                umbra_buf buf[] = {
                    { ROW(y), row_sz },
                    { uni, -(long)uni_len },
                };
                run(ctx, W, buf);
            }
        } else {
            rx += vx;
            ry += vy;
            if (rx < 0.0f) {
                rx = 0.0f;
                vx = -vx;
            }
            if (rx + rect_w > (float)W) {
                rx = (float)W - rect_w;
                vx = -vx;
            }
            if (ry < 0.0f) {
                ry = 0.0f;
                vy = -vy;
            }
            if (ry + rect_h > (float)H) {
                ry = (float)H - rect_h;
                vy = -vy;
            }

            float rect[4] = {
                rx, ry, rx + rect_w, ry + rect_h,
            };

            float hc[4];
            for (int i = 0; i < 4; i++) {
                hc[i] = s->color[i];
            }

            for (int y = 0; y < H; y++) {
                long long uni_[6] = {0};
                char *uni = (char*)uni_;
                uni_i32(uni,
                    draw_layout.x0, 0);
                uni_i32(uni,
                    draw_layout.y, y);
                uni_f32(uni,
                    draw_layout.shader, hc, 4);
                if (s->coverage) {
                    uni_f32(uni,
                        draw_layout.coverage,
                        rect, 4);
                }
                for (int i = 0; i < ps; i++) {
                    uni_i32(uni,
                        uni_len - (ps-i) * 4,
                        planar_stride);
                }
                umbra_buf buf[] = {
                    { ROW(y), row_sz },
                    { uni, -(long)uni_len },
                };
                run(ctx, W, buf);
            }
        }

        if (cur_backend == 3 && mtl) {
            umbra_metal_flush(mtl);
        }
        #undef ROW

        void *tex_pixels;
        int   tex_pitch;
        if (!SDL_LockTexture(texture, NULL,
                &tex_pixels, &tex_pitch)) {
            SDL_Log("SDL_LockTexture failed: %s",
                    SDL_GetError());
            break;
        }

        uint8_t *rows = (uint8_t*)tex_pixels;
        for (int y = 0; y < H; y++) {
            void *src = planar
                ? (void*)((__fp16*)pixbuf
                          + y * W)
                : (void*)((uint8_t*)pixbuf
                          + y * W * bpp);
            readback_row(
                (uint32_t*)(rows + y*tex_pitch),
                src, W, row_sz, planar_stride);
        }

        if (want_dump) {
            float *fdata = malloc(
                (size_t)(W*H) * 4 * sizeof(float));
            for (int y = 0; y < H; y++) {
                void *src = planar
                    ? (void*)((__fp16*)pixbuf
                              + y * W)
                    : (void*)((uint8_t*)pixbuf
                              + y * W * bpp);
                to_hdr_row(fdata + y * W * 4,
                           src, W, row_sz,
                           planar_stride);
            }
            stbi_write_hdr("dump.hdr",
                           W, H, 4, fdata);
            SDL_Log("saved dump.hdr (%s)",
                    fmt_name[cur_fmt]);
            free(fdata);
            want_dump = 0;
        }

        SDL_UnlockTexture(texture);
        SDL_RenderTexture(renderer,
                          texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        fps_frames++;
        uint64_t now =
            SDL_GetPerformanceCounter();
        uint64_t freq =
            SDL_GetPerformanceFrequency();
        double elapsed =
            (double)(now - fps_start)
            / (double)freq;
        if (elapsed >= 0.5) {
            fps = (double)fps_frames / elapsed;
            fps_frames = 0;
            fps_start  = now;
            update_title(window,
                &slides[cur_slide],
                cur_backend, cur_fmt, fps);
        }
    }

    free(pixbuf);
    text_cov_free(&bitmap_cov);
    text_cov_free(&sdf_cov);
    free_backends();
    free_pipes();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#endif
