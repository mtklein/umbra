#include "../slides/slide.h"
#include "work_group.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_image_write.h"
#pragma clang diagnostic pop

typedef struct umbra_builder builder;

enum { NUM_BACKENDS = 4 };
static char const *backend_name[NUM_BACKENDS] = {
    "interp",
    "jit",
    "metal",
    "vulkan",
};

static struct umbra_backend *bes[NUM_BACKENDS];
static struct umbra_program *programs[NUM_BACKENDS];
static umbra_draw_layout     draw_layout;

static void free_programs(void) {
    for (int i = 0; i < NUM_BACKENDS; i++) {
        if (programs[i]) { programs[i]->free(programs[i]); }
        programs[i] = NULL;
    }
}

enum {
    FMT_8888,
    FMT_565,
    FMT_FP16,
    FMT_1010102,
    NUM_FMTS,
};
static char const *fmt_name[] = {
    "8888", "565", "fp16", "1010102",
};
static umbra_fmt const fmt_enums[] = {
    umbra_fmt_8888, umbra_fmt_565, umbra_fmt_fp16,
    umbra_fmt_1010102,
};

typedef struct {
    struct umbra_program *program;
    int                   uni_len, out_ptr;
} pipe;

static pipe fill_pipe, readback_pipe, hdr_pipe;

static void free_pipe(pipe *p) {
    if (p->program) { p->program->free(p->program); }
    *p = (pipe){0};
}

static struct umbra_backend *pipe_be;

static void finish_pipe(pipe *p, builder *builder) {
    p->uni_len = umbra_uni_len(builder);
    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);
    p->program = pipe_be->compile(pipe_be, bb);
    umbra_basic_block_free(bb);
}

static void build_fill(int fmt) {
    free_pipe(&fill_pipe);
    builder    *builder = umbra_builder();
    int         fi = umbra_reserve(builder, 4);
    umbra_color c = {
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi),
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1),
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2),
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3),
    };
    umbra_store_color(builder, (umbra_ptr){1, 0}, c, fmt_enums[fmt]);
    finish_pipe(&fill_pipe, builder);
}

static void build_readback(int fmt) {
    free_pipe(&readback_pipe);
    builder    *builder = umbra_builder();
    umbra_color c = umbra_load_color(builder, (umbra_ptr){1, 0}, fmt_enums[fmt]);
    int         op = umbra_max_ptr(builder) + 1;
    umbra_store_color(builder, (umbra_ptr){op, 0}, c, umbra_fmt_8888);
    readback_pipe.out_ptr = op;
    finish_pipe(&readback_pipe, builder);
}

static void build_hdr(int fmt) {
    free_pipe(&hdr_pipe);
    builder    *builder = umbra_builder();
    umbra_color c = umbra_load_color(builder, (umbra_ptr){1, 0}, fmt_enums[fmt]);
    int         op = umbra_max_ptr(builder) + 1;
    hdr_pipe.out_ptr = op;
    umbra_store_color(builder, (umbra_ptr){op, 0}, c, umbra_fmt_fp16);
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

static int                    max_threads;
static int                    n_threads = 1;
static struct umbra_program **xtra_progs;   // [1..n_threads-1], compiled from shared backend
static struct umbra_basic_block *saved_bb;

static void free_xtra(void) {
    for (int t = 1; t < max_threads; t++) {
        if (xtra_progs[t]) { xtra_progs[t]->free(xtra_progs[t]); }
        xtra_progs[t] = NULL;
    }
}

static void rebuild_xtra(int backend) {
    free_xtra();
    if (!saved_bb || n_threads <= 1 || !bes[backend]) { return; }
    for (int t = 1; t < n_threads; t++) {
        xtra_progs[t] = bes[backend]->compile(bes[backend], saved_bb);
    }
}

static void tile_factor(int n, int *cols, int *rows) {
    int c = 1;
    for (int d = 2; d * d <= n; d++) {
        if (n % d == 0) { c = d; }
    }
    *cols = c;
    *rows = n / c;
}

static void build_slide_fmt(slide *s, int fmt) {
    free_programs();
    umbra_uniforms_free(draw_layout.uni);
    s->fmt = fmt_enums[fmt];

    builder *builder = umbra_draw_build(s->shader, s->coverage, s->blend, fmt_enums[fmt],
                                        &draw_layout);
    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);

    for (int i = 0; i < NUM_BACKENDS; i++) {
        programs[i] = bes[i] ? bes[i]->compile(bes[i], bb) : NULL;
    }
    umbra_basic_block_free(saved_bb);
    saved_bb = bb;

    build_pipes(fmt);
}

static int pick_backend(int cur) {
    if (programs[cur]) { return cur; }
    for (int i = 1; i < NUM_BACKENDS; i++) {
        int b = (cur + i) % NUM_BACKENDS;
        if (programs[b]) { return b; }
    }
    return 0;
}
static int next_backend(int cur) {
    for (int i = 1; i <= NUM_BACKENDS; i++) {
        int b = (cur + i) % NUM_BACKENDS;
        if (programs[b]) { return b; }
    }
    return cur;
}

static void update_title(SDL_Window *w, slide *s, int bi, int fi, double fps, int nt) {
    char title[256];
    int n = SDL_snprintf(title, sizeof title, "%s  [%s/%s]  %.0f fps", s->title,
                         backend_name[bi], fmt_name[fi], fps);
    if (nt > 1) {
        int tc, tr;
        tile_factor(nt, &tc, &tr);
        SDL_snprintf(title + n, sizeof title - (size_t)n, "  %dx%d", tc, tr);
    }
    SDL_SetWindowTitle(w, title);
}

static void fill_bg_row(void *dst, int n, uint32_t bg, size_t row_sz, size_t plane_gap) {
    float hc[4] = {
        (float)(bg & 0xffu) / 255.0f,
        (float)((bg >> 8) & 0xffu) / 255.0f,
        (float)((bg >> 16) & 0xffu) / 255.0f,
        (float)((bg >> 24) & 0xffu) / 255.0f,
    };
    uint64_t uni_[4] = {0};
    char    *uni = (char *)uni_;
    slide_uni_f32(uni, 0, hc, 4);
    int      ps = plane_gap ? 3 : 0;
    umbra_buf buf[5];
    buf[0] = (umbra_buf){.ptr=uni, .sz=(size_t)fill_pipe.uni_len, .read_only=1};
    buf[1] = (umbra_buf){.ptr=dst, .sz=row_sz};
    for (int i = 0; i < ps; i++) {
        buf[2 + i] = (umbra_buf){.ptr=(char *)dst + (size_t)(i + 1) * plane_gap, .sz=row_sz};
    }
    fill_pipe.program->queue(fill_pipe.program, 0, 0, n, 1, buf);
}

static void readback_row(uint32_t *dst, void *src, int n, size_t src_sz, size_t plane_gap) {
    uint64_t uni_[2] = {0};
    char    *uni = (char *)uni_;
    int      ps = plane_gap ? 3 : 0;
    int      op = readback_pipe.out_ptr;
    umbra_buf buf[6];
    buf[0] = (umbra_buf){.ptr=uni, .sz=(size_t)readback_pipe.uni_len, .read_only=1};
    buf[1] = (umbra_buf){.ptr=src, .sz=src_sz, .read_only=1};
    for (int i = 0; i < ps; i++) {
        buf[2 + i] = (umbra_buf){.ptr=(char *)src + (size_t)(i + 1) * plane_gap, .sz=src_sz};
    }
    buf[op] = (umbra_buf){.ptr=dst, .sz=(size_t)(n * 4)};
    readback_pipe.program->queue(readback_pipe.program, 0, 0, n, 1, buf);
}

static void to_hdr_row(__fp16 *dst, void *src, int n, size_t src_sz, size_t plane_gap) {
    uint64_t uni_[2] = {0};
    char    *uni = (char *)uni_;
    int      ps = plane_gap ? 3 : 0;
    int      op = hdr_pipe.out_ptr;
    umbra_buf buf[6];
    buf[0] = (umbra_buf){.ptr=uni, .sz=(size_t)hdr_pipe.uni_len, .read_only=1};
    buf[1] = (umbra_buf){.ptr=src, .sz=src_sz, .read_only=1};
    for (int i = 0; i < ps; i++) {
        buf[2 + i] = (umbra_buf){.ptr=(char *)src + (size_t)(i + 1) * plane_gap, .sz=src_sz};
    }
    buf[op] = (umbra_buf){.ptr=dst, .sz=(size_t)(n * 8)};
    hdr_pipe.program->queue(hdr_pipe.program, 0, 0, n, 1, buf);
}

typedef struct {
    slide *s;
    int    W, H, y0, y1;
    void  *buf;
    umbra_draw_layout const *lay;
    struct umbra_program    *prog;
} tile_work;

static void tile_fn(void *arg) {
    tile_work *tw = arg;
    tw->s->draw(tw->s, tw->W, tw->H, tw->y0, tw->y1, tw->buf, tw->lay, tw->prog);
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

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING, W, H);
    if (!texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        return 1;
    }

    slides_init(W, H);

    max_threads = SDL_GetNumLogicalCPUCores();
    if (max_threads < 1) { max_threads = 1; }
    struct thread_pool *pool = thread_pool(max_threads);
    xtra_progs = calloc((size_t)max_threads, sizeof *xtra_progs);

    bes[0] = umbra_backend_interp();
    bes[1] = umbra_backend_jit();
    bes[2] = umbra_backend_metal();
    bes[3] = umbra_backend_vulkan();
    pipe_be = umbra_backend_jit();
    if (!pipe_be) { pipe_be = umbra_backend_interp(); }

    void *pixbuf = malloc(W * H * 8);

    int cur_slide = slide_count() - 1;
    int cur_fmt = FMT_8888;
    build_slide_fmt(slide_get(cur_slide), cur_fmt);
    int cur_backend = pick_backend(1);

    uint64_t fps_start = SDL_GetPerformanceCounter();
    int      fps_frames = 0;
    double   fps = 0.0;

    _Bool want_dump = 0;

    update_title(window, slide_get(cur_slide), cur_backend, cur_fmt, fps, n_threads);

    _Bool running = 1;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = 0;
            } else if (ev.type == SDL_EVENT_KEY_DOWN) {
                int next = cur_slide;
                if (ev.key.key == SDLK_RIGHT || ev.key.key == SDLK_SPACE) {
                    next = (cur_slide + 1) % slide_count();
                } else if (ev.key.key == SDLK_LEFT) {
                    next = (cur_slide + slide_count() - 1) % slide_count();
                } else if (ev.key.key == SDLK_B) {
                    cur_backend = next_backend(cur_backend);
                    rebuild_xtra(cur_backend);
                } else if (ev.key.key == SDLK_C) {
                    cur_fmt = (cur_fmt + 1) % NUM_FMTS;
                    build_slide_fmt(slide_get(cur_slide), cur_fmt);
                    cur_backend = pick_backend(cur_backend);
                    rebuild_xtra(cur_backend);
                } else if (ev.key.key == SDLK_COMMA) {
                    if (n_threads > 1) { n_threads--; rebuild_xtra(cur_backend); }
                } else if (ev.key.key == SDLK_PERIOD) {
                    if (n_threads < max_threads) { n_threads++; rebuild_xtra(cur_backend); }
                } else if (ev.key.key == SDLK_S) {
                    want_dump = 1;
                } else if (ev.key.key == SDLK_ESCAPE) {
                    running = 0;
                }
                if (next != cur_slide) {
                    cur_slide = next;
                    build_slide_fmt(slide_get(cur_slide), cur_fmt);
                    cur_backend = pick_backend(cur_backend);
                    rebuild_xtra(cur_backend);
                }
                update_title(window, slide_get(cur_slide), cur_backend, cur_fmt, fps,
                             n_threads);
            }
        }
        if (!running) { break; }

        slide                *s = slide_get(cur_slide);
        struct umbra_program *b = programs[cur_backend];

        size_t bpp = umbra_fmt_size(fmt_enums[cur_fmt]);
        size_t row_bytes = (size_t)W * bpp;
        size_t plane_gap = 0;
        size_t row_sz = (size_t)W * (size_t)bpp;

        for (int y = 0; y < H; y++) {
            void *row = (void *)((uint8_t *)pixbuf + (size_t)y * row_bytes);
            fill_bg_row(row, W, s->bg, row_sz, plane_gap);
        }

        if (s->animate) { s->animate(s, 0.016f); }

        if (s->prepare) { s->prepare(s, W, H, bes[cur_backend]); }

        {
            int nt = n_threads;
            int sh = (H + nt - 1) / nt;

            tile_work *work = malloc((size_t)nt * sizeof *work);
            for (int t = 0; t < nt; t++) {
                int y0 = t * sh;
                int y1 = y0 + sh > H ? H : y0 + sh;
                struct umbra_program *tp;
                if (!bes[cur_backend]->threadsafe) { tp = b; }
                else { tp = t == 0 ? b : xtra_progs[t]; }
                work[t] = (tile_work){s, W, H, y0, y1, pixbuf, &draw_layout, tp};
            }
            if (!bes[cur_backend]->threadsafe) {
                for (int t = 0; t < nt; t++) { tile_fn(&work[t]); }
            } else {
                work_group wg = {.pool = pool};
                for (int t = 0; t < nt; t++) { work_group_add(&wg, tile_fn, &work[t]); }
                work_group_wait(&wg);
            }
            free(work);
        }

        bes[cur_backend]->flush(bes[cur_backend]);

        void *tex_pixels;
        int   tex_pitch;
        if (!SDL_LockTexture(texture, NULL, &tex_pixels, &tex_pitch)) {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
            break;
        }

        uint8_t *rows = (uint8_t *)tex_pixels;
        for (int y = 0; y < H; y++) {
            void *src = (void *)((uint8_t *)pixbuf + (size_t)y * (size_t)W * bpp);
            readback_row((uint32_t *)(rows + y * tex_pitch), src, W, row_sz, plane_gap);
        }

        if (want_dump) {
            __fp16 *hdata = malloc((size_t)(W * H) * 4 * sizeof(__fp16));
            for (int y = 0; y < H; y++) {
                void *src = (void *)((uint8_t *)pixbuf + (size_t)y * (size_t)W * bpp);
                to_hdr_row(hdata + y * W * 4, src, W, row_sz, plane_gap);
            }
            float *fdata = malloc((size_t)(W * H) * 4 * sizeof(float));
            for (int i = 0; i < W * H * 4; i++) { fdata[i] = (float)hdata[i]; }
            free(hdata);
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
        double   elapsed = (double)(now - fps_start) / (double)freq;
        if (elapsed >= 0.5) {
            fps = (double)fps_frames / elapsed;
            fps_frames = 0;
            fps_start = now;
            update_title(window, slide_get(cur_slide), cur_backend, cur_fmt, fps,
                         n_threads);
        }
    }

    free(pixbuf);
    thread_pool_free(pool);
    free_xtra();
    free(xtra_progs);
    umbra_basic_block_free(saved_bb);
    free_programs();
    umbra_uniforms_free(draw_layout.uni);
    free_pipes();
    for (int i = 0; i < NUM_BACKENDS; i++) { if (bes[i]) { bes[i]->free(bes[i]); } }
    pipe_be->free(pipe_be);
    slides_cleanup();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
