#include "../slides/slide.h"
#include "work_group.h"
#include <stdint.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_image_write.h"
#pragma clang diagnostic pop

enum { NUM_BACKENDS = 5 };
static char const *backend_name[NUM_BACKENDS] = {
    "interp",
    "jit",
    "metal",
    "vulkan",
    "wgpu",
};

static struct umbra_backend *bes[NUM_BACKENDS];

enum {
    FMT_8888,
    FMT_565,
    FMT_FP16,
    FMT_FP16_PLANAR,
    FMT_1010102,
    NUM_FMTS,
};
static struct umbra_fmt const *fmt_enums[] = {
    &umbra_fmt_8888, &umbra_fmt_565, &umbra_fmt_fp16,
    &umbra_fmt_fp16_planar, &umbra_fmt_1010102,
};

struct pipe {
    struct umbra_program          *program;
    struct umbra_uniforms_layout   uni; int :32;
    void                          *uniforms;
    int                            out_ptr, pad_;
};

static struct pipe readback_pipe, hdr_pipe;

static void free_pipe(struct pipe *p) {
    if (p->program) { p->program->free(p->program); }
    free(p->uniforms);
    *p = (struct pipe){0};
}

static struct umbra_backend *pipe_be;

static void finish_pipe(struct pipe *p, struct umbra_builder *builder,
                        struct umbra_uniforms_layout uni) {
    p->uni = uni;
    p->uniforms = umbra_uniforms_alloc(&uni);
    struct umbra_flat_ir *ir = umbra_flat_ir(builder);
    umbra_builder_free(builder);
    p->program = pipe_be->compile(pipe_be, ir);
    umbra_flat_ir_free(ir);
}

static void build_readback(int fmt) {
    free_pipe(&readback_pipe);
    struct umbra_builder *builder = umbra_builder();
    umbra_color_val32 c = fmt_enums[fmt]->load(builder, 1);
    umbra_fmt_fp16.store(builder, 2, c);
    readback_pipe.out_ptr = 2;
    finish_pipe(&readback_pipe, builder, (struct umbra_uniforms_layout){0});
}

static void build_hdr(int fmt) {
    free_pipe(&hdr_pipe);
    struct umbra_builder *builder = umbra_builder();
    umbra_color_val32 c = fmt_enums[fmt]->load(builder, 1);
    hdr_pipe.out_ptr = 2;
    umbra_fmt_fp16.store(builder, 2, c);
    finish_pipe(&hdr_pipe, builder, (struct umbra_uniforms_layout){0});
}

static void build_pipes(int fmt) {
    build_readback(fmt);
    build_hdr(fmt);
}

static void free_pipes(void) {
    free_pipe(&readback_pipe);
    free_pipe(&hdr_pipe);
}

static int                    max_threads;
static int                    n_threads = 1;
static struct umbra_program **xtra_progs;
static struct umbra_flat_ir *saved_ir;

static void free_xtra(void) {
    for (int t = 1; t < max_threads; t++) {
        if (xtra_progs[t]) { xtra_progs[t]->free(xtra_progs[t]); }
        xtra_progs[t] = NULL;
    }
}

static void rebuild_xtra(int backend) {
    free_xtra();
    if (!saved_ir || n_threads <= 1 || !bes[backend]) { return; }
    for (int t = 1; t < n_threads; t++) {
        xtra_progs[t] = bes[backend]->compile(bes[backend], saved_ir);
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

static int cur_backend;

static void build_slide_fmt(struct slide *s, int fmt) {
    if (bes[cur_backend]) {
        s->prepare(s, bes[cur_backend], *fmt_enums[fmt]);
    }
    umbra_flat_ir_free(saved_ir);
    {
        struct umbra_builder *b = NULL;
        if (s->get_builders) { s->get_builders(s, *fmt_enums[fmt], &b, 1); }
        if (b) {
            saved_ir = umbra_flat_ir(b);
            umbra_builder_free(b);
        } else {
            saved_ir = NULL;
        }
    }
    build_pipes(fmt);
}

static int pick_backend(int cur) {
    if (bes[cur]) { return cur; }
    for (int i = 1; i < NUM_BACKENDS; i++) {
        int b = (cur + i) % NUM_BACKENDS;
        if (bes[b]) { return b; }
    }
    return 0;
}
static int next_backend(int cur) {
    for (int i = 1; i <= NUM_BACKENDS; i++) {
        int b = (cur + i) % NUM_BACKENDS;
        if (bes[b]) { return b; }
    }
    return cur;
}

static void update_title(SDL_Window *w, struct slide *s, int bi, int fi, double fps, int nt) {
    char title[256];
    int n = SDL_snprintf(title, sizeof title, "%s  [%s/%s]  %.0f fps", s->title,
                         backend_name[bi], fmt_enums[fi]->name, fps);
    if (nt > 1) {
        int tc, tr;
        tile_factor(nt, &tc, &tr);
        SDL_snprintf(title + n, sizeof title - (size_t)n, "  %dx%d", tc, tr);
    }
    SDL_SetWindowTitle(w, title);
}

static void readback_row(void *dst, void *src, int n, size_t plane_gap) {
    int      ps = plane_gap ? 3 : 0;
    int      op = readback_pipe.out_ptr;
    struct umbra_buf buf[6];
    buf[0] = (struct umbra_buf){.ptr=readback_pipe.uniforms,
                                 .count=readback_pipe.uni.slots};
    buf[1] = (struct umbra_buf){.ptr=src, .count=n};
    for (int i = 0; i < ps; i++) {
        buf[2 + i] = (struct umbra_buf){.ptr=(char *)src + (size_t)(i + 1) * plane_gap,
                                         .count=n};
    }
    buf[op] = (struct umbra_buf){.ptr=dst, .count=n};
    readback_pipe.program->queue(readback_pipe.program, 0, 0, n, 1, buf);
}

static void to_hdr_row(__fp16 *dst, void *src, int n, size_t plane_gap) {
    int      ps = plane_gap ? 3 : 0;
    int      op = hdr_pipe.out_ptr;
    struct umbra_buf buf[6];
    buf[0] = (struct umbra_buf){.ptr=hdr_pipe.uniforms, .count=hdr_pipe.uni.slots};
    buf[1] = (struct umbra_buf){.ptr=src, .count=n};
    for (int i = 0; i < ps; i++) {
        buf[2 + i] = (struct umbra_buf){.ptr=(char *)src + (size_t)(i + 1) * plane_gap,
                                         .count=n};
    }
    buf[op] = (struct umbra_buf){.ptr=dst, .count=n};
    hdr_pipe.program->queue(hdr_pipe.program, 0, 0, n, 1, buf);
}

struct tile_work {
    struct slide *s;
    double        secs;
    int           l, t, r, b;
    void         *buf;
};

static void tile_fn(void *arg) {
    struct tile_work *tw = arg;
    tw->s->draw(tw->s, tw->secs, tw->l, tw->t, tw->r, tw->b, tw->buf);
}

int main(void) {
    int W = 1024, H = 768;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("umbra demo", W, H,
                                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    SDL_PropertiesID rprops = SDL_CreateProperties();
    SDL_SetPointerProperty(rprops, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetNumberProperty(rprops, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER,
                          SDL_COLORSPACE_SRGB_LINEAR);
    SDL_Renderer *renderer = SDL_CreateRendererWithProperties(rprops);
    SDL_DestroyProperties(rprops);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }

    SDL_GetWindowSizeInPixels(window, &W, &H);
    slides_init(W, H);

    max_threads = SDL_GetNumLogicalCPUCores();
    if (max_threads < 1) { max_threads = 1; }
    struct thread_pool *pool = thread_pool(max_threads);
    xtra_progs = calloc((size_t)max_threads, sizeof *xtra_progs);

    bes[0] = umbra_backend_interp();
    bes[1] = umbra_backend_jit();
    bes[2] = umbra_backend_metal();
    bes[3] = umbra_backend_vulkan();
    bes[4] = umbra_backend_wgpu();
    pipe_be = umbra_backend_jit();
    if (!pipe_be) { pipe_be = umbra_backend_interp(); }

    void *pixbuf = malloc((size_t)W * (size_t)H * 8);

    int cur_slide = slide_count() - 1;
    int cur_fmt = FMT_FP16;
    cur_backend = pick_backend(2);
    build_slide_fmt(slide_get(cur_slide), cur_fmt);

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA64_FLOAT,
                                             SDL_TEXTUREACCESS_STREAMING, W, H);
    if (!texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        return 1;
    }

    uint64_t fps_start = SDL_GetPerformanceCounter();
    uint64_t t0        = fps_start;
    int      fps_frames = 0;
    double   fps = 0.0;

    _Bool want_dump = 0;
    _Bool vsync = 0;

    update_title(window, slide_get(cur_slide), cur_backend, cur_fmt, fps, n_threads);

    _Bool running = 1;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = 0;
            } else if (ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                W = ev.window.data1;
                H = ev.window.data2;
                if (W < 1) { W = 1; }
                if (H < 1) { H = 1; }
                slides_cleanup();
                slides_init(W, H);
                free(pixbuf);
                pixbuf = malloc((size_t)W * (size_t)H * 8);
                SDL_DestroyTexture(texture);
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA64_FLOAT,
                                            SDL_TEXTUREACCESS_STREAMING, W, H);
                build_slide_fmt(slide_get(cur_slide), cur_fmt);
                rebuild_xtra(cur_backend);
            } else if (ev.type == SDL_EVENT_KEY_DOWN) {
                int next = cur_slide;
                if (ev.key.key == SDLK_RIGHT || ev.key.key == SDLK_SPACE) {
                    next = (cur_slide + 1) % slide_count();
                } else if (ev.key.key == SDLK_LEFT) {
                    next = (cur_slide + slide_count() - 1) % slide_count();
                } else if (ev.key.key == SDLK_B) {
                    cur_backend = next_backend(cur_backend);
                    struct slide *s = slide_get(cur_slide);
                    if (bes[cur_backend]) { s->prepare(s, bes[cur_backend], *fmt_enums[cur_fmt]); }
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
                } else if (ev.key.key == SDLK_V) {
                    vsync = !vsync;
                    SDL_SetRenderVSync(renderer, vsync ? 1 : 0);
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

        struct slide *s = slide_get(cur_slide);

        size_t bpp = fmt_enums[cur_fmt]->bpp;
        int    planes = fmt_enums[cur_fmt]->planes;
        size_t plane_gap = planes > 1 ? (size_t)W * (size_t)H * bpp : 0;

        if (s->prepare) { s->prepare(s, bes[cur_backend], *fmt_enums[cur_fmt]); }

        {
            int nt = n_threads;
            if (!saved_ir && nt > 1) { nt = 1; }
            int sh = (H + nt - 1) / nt;

            uint64_t const freq = SDL_GetPerformanceFrequency();
            double const secs = (double)(SDL_GetPerformanceCounter() - t0) / (double)freq;

            struct tile_work *work = malloc((size_t)nt * sizeof *work);
            for (int t = 0; t < nt; t++) {
                int y0 = t * sh;
                int y1 = y0 + sh > H ? H : y0 + sh;
                work[t] = (struct tile_work){s, secs, 0, y0, W, y1, pixbuf};
            }
            if (nt <= 1 || !xtra_progs[1]->threadsafe) {
                for (int t = 0; t < nt; t++) { tile_fn(&work[t]); }
            } else {
                struct work_group wg = {.pool = pool};
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
        if (plane_gap) {
            for (int y = 0; y < H; y++) {
                int      op = readback_pipe.out_ptr;
                struct umbra_buf rb[3];
                rb[0] = (struct umbra_buf){.ptr = readback_pipe.uniforms,
                                           .count = readback_pipe.uni.slots};
                rb[1] = (struct umbra_buf){.ptr = pixbuf,
                                           .count = W * H * planes,
                                           .stride = W};
                rb[op] = (struct umbra_buf){.ptr = rows + y * tex_pitch,
                                            .count = W};
                readback_pipe.program->queue(readback_pipe.program, 0, y, W, y + 1, rb);
            }
        } else {
            for (int y = 0; y < H; y++) {
                void *src = (void *)((uint8_t *)pixbuf + (size_t)y * (size_t)W * bpp);
                readback_row(rows + y * tex_pitch, src, W, plane_gap);
            }
        }

        if (want_dump) {
            __fp16 *hdata = malloc((size_t)(W * H) * 4 * sizeof(__fp16));
            if (plane_gap) {
                for (int y = 0; y < H; y++) {
                    int      op = hdr_pipe.out_ptr;
                    struct umbra_buf hb[3];
                    hb[0] = (struct umbra_buf){.ptr = hdr_pipe.uniforms,
                                               .count = hdr_pipe.uni.slots};
                    hb[1] = (struct umbra_buf){.ptr = pixbuf,
                                               .count = W * H * planes,
                                               .stride = W};
                    hb[op] = (struct umbra_buf){.ptr = hdata + y * W * 4,
                                                .count = W};
                    hdr_pipe.program->queue(hdr_pipe.program, 0, y, W, y + 1, hb);
                }
            } else {
                for (int y = 0; y < H; y++) {
                    void *src = (void *)((uint8_t *)pixbuf + (size_t)y * (size_t)W * bpp);
                    to_hdr_row(hdata + y * W * 4, src, W, plane_gap);
                }
            }
            float *fdata = malloc((size_t)(W * H) * 4 * sizeof(float));
            for (int i = 0; i < W * H * 4; i++) { fdata[i] = (float)hdata[i]; }
            free(hdata);
            stbi_write_hdr("dump.hdr", W, H, 4, fdata);
            SDL_Log("saved dump.hdr (%s)", fmt_enums[cur_fmt]->name);
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
    umbra_flat_ir_free(saved_ir);
    free_pipes();
    slides_cleanup();
    for (int i = 0; i < NUM_BACKENDS; i++) { if (bes[i]) { bes[i]->free(bes[i]); } }
    pipe_be->free(pipe_be);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
