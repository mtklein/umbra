#include "slides/slide.h"
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

typedef struct umbra_builder builder;

enum { NUM_BACKENDS = 4 };
static char const *backend_name[NUM_BACKENDS] = {
    "interp", "jit", "codegen", "metal",
};

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

static slide_run_fn const run_fns[NUM_BACKENDS] = {
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
    slide_run_fn              run;
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

static void finish_pipe(pipe *p, builder *builder) {
    p->uni_len = umbra_uni_len(builder);
    struct umbra_basic_block *bb =
        umbra_basic_block(builder);
    umbra_builder_free(builder);
    p->interp  = umbra_interpreter(bb);
    p->jit     = umbra_jit(bb);
    p->ctx = p->jit
        ? (void*)p->jit : (void*)p->interp;
    p->run = p->jit ? run_jit : run_interp;
    umbra_basic_block_free(bb);
}

static void build_fill(int fmt) {
    free_pipe(&fill_pipe);
    builder *builder = umbra_builder();
    umbra_val ix = umbra_iota(builder);
    int fi = umbra_reserve(builder, 4);
    umbra_color c = {
        umbra_load_i32(builder, (umbra_ptr){1},
                     umbra_imm_i32(builder, fi)),
        umbra_load_i32(builder, (umbra_ptr){1},
                     umbra_imm_i32(builder, fi+1)),
        umbra_load_i32(builder, (umbra_ptr){1},
                     umbra_imm_i32(builder, fi+2)),
        umbra_load_i32(builder, (umbra_ptr){1},
                     umbra_imm_i32(builder, fi+3)),
    };
    fmt_store[fmt](builder, (umbra_ptr){0}, ix, c);
    finish_pipe(&fill_pipe, builder);
}

static void build_readback(int fmt) {
    free_pipe(&readback_pipe);
    builder *builder = umbra_builder();
    umbra_val ix = umbra_iota(builder);
    umbra_color c =
        fmt_load[fmt](builder, (umbra_ptr){0}, ix);
    umbra_store_8888(builder, (umbra_ptr){2}, ix, c);
    finish_pipe(&readback_pipe, builder);
}

static void build_hdr(int fmt) {
    free_pipe(&hdr_pipe);
    builder *builder = umbra_builder();
    umbra_val ix = umbra_iota(builder);
    umbra_color c =
        fmt_load[fmt](builder, (umbra_ptr){0}, ix);
    umbra_val ix4 = umbra_shl_i32(builder, ix,
                        umbra_imm_i32(builder, 2));
    umbra_store_i32(builder, (umbra_ptr){2},
        umbra_add_i32(builder, ix4,
                   umbra_imm_i32(builder, 0)), c.r);
    umbra_store_i32(builder, (umbra_ptr){2},
        umbra_add_i32(builder, ix4,
                   umbra_imm_i32(builder, 1)), c.g);
    umbra_store_i32(builder, (umbra_ptr){2},
        umbra_add_i32(builder, ix4,
                   umbra_imm_i32(builder, 2)), c.b);
    umbra_store_i32(builder, (umbra_ptr){2},
        umbra_add_i32(builder, ix4,
                   umbra_imm_i32(builder, 3)), c.a);
    finish_pipe(&hdr_pipe, builder);
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

static void build_slide_fmt(slide *s, int fmt) {
    free_backends();
    umbra_load_fn  load =
        s->load ? fmt_load[fmt] : NULL;
    umbra_store_fn store = fmt_store[fmt];

    builder *builder = umbra_draw_build(
        s->shader, s->coverage, s->blend,
        load, store, &draw_layout);
    struct umbra_basic_block *bb =
        umbra_basic_block(builder);
    umbra_builder_free(builder);

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

static void update_title(SDL_Window *w,
                         slide *s,
                         int bi, int fi,
                         double fps) {
    char title[256];
    SDL_snprintf(title, sizeof title,
        "%s  [%s/%s]  %.0f fps",
        s->title, backend_name[bi],
        fmt_name[fi], fps);
    SDL_SetWindowTitle(w, title);
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
    slide_uni_f32(uni, 0, hc, 4);
    if (fill_pipe.uni_len > 16) {
        slide_uni_i32(uni, 16, stride);
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
        slide_uni_i32(uni, 0, stride);
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
        slide_uni_i32(uni, 0, stride);
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

    slides_init(W, H);

    void *pixbuf = malloc(W * H * 8);
    int32_t planar_stride = W * H;

    int cur_slide   = slide_count() - 1;
    int cur_fmt     = FMT_8888;
    build_slide_fmt(slide_get(cur_slide), cur_fmt);
    int cur_backend = pick_backend(1);

    uint64_t fps_start  =
        SDL_GetPerformanceCounter();
    int      fps_frames = 0;
    double   fps        = 0.0;

    _Bool want_dump = 0;

    update_title(window, slide_get(cur_slide),
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
                         % slide_count();
                } else if (ev.key.key ==
                               SDLK_LEFT) {
                    next = (cur_slide
                            + slide_count() - 1)
                         % slide_count();
                } else if (ev.key.key == SDLK_B) {
                    cur_backend =
                        next_backend(cur_backend);
                } else if (ev.key.key == SDLK_C) {
                    cur_fmt =
                        (cur_fmt + 1) % NUM_FMTS;
                    build_slide_fmt(
                        slide_get(cur_slide),
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
                        slide_get(cur_slide),
                        cur_fmt);
                    cur_backend =
                        pick_backend(cur_backend);
                }
                update_title(window,
                    slide_get(cur_slide),
                    cur_backend, cur_fmt, fps);
            }
        }
        if (!running) { break; }

        slide *s = slide_get(cur_slide);
        slide_run_fn run = run_fns[cur_backend];
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

        int ps = planar
            ? (s->load ? 2 : 1) : 0;

        if (s->animate) { s->animate(s, 0.016f); }

        if (cur_backend == 3 && mtl) {
            umbra_metal_begin_batch(mtl);
        }

        for (int y = 0; y < H; y++) {
            s->render_row(s, y, W,
                ROW(y), row_sz,
                &draw_layout, ps, planar_stride,
                ctx, run);
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
                slide_get(cur_slide),
                cur_backend, cur_fmt, fps);
        }
    }

    free(pixbuf);
    free_backends();
    free_pipes();
    slides_cleanup();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#endif
