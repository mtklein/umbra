#define _POSIX_C_SOURCE 200809L
#include "../slides/slide.h"
#include "../slides/slug.h"
#include "../include/umbra.h"
#include "../include/umbra_draw.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

typedef void (*draw_fn)(void *ctx);

struct slide_draw_ctx {
    struct slide *s;
    void         *buf;
    int           frame;
    int           w, h, :32;
};
static void slide_draw(void *vctx) {
    struct slide_draw_ctx *c = vctx;
    c->s->draw(c->s, c->frame++, 0, 0, c->w, c->h, c->buf);
}

struct prog_draw_ctx {
    struct umbra_program *p;
    struct umbra_buf     *bufs;
    int                   w, h;
};
static void prog_draw(void *vctx) {
    struct prog_draw_ctx *c = vctx;
    c->p->queue(c->p, 0, 0, c->w, c->h, c->bufs);
}

// Doubling pilot + min-of-K timing.
//
// 1. Pilot: time iters=1, then 2, 4, 8, ..., until one round takes at
//    least target_secs/2. Each pilot round is `iters` draws + one flush
//    so the per-iter draw cost dominates the time once iters is large
//    enough — that's what makes the calibration robust to per-flush
//    fixed cost (cmdbuf submit, waitUntilCompleted, autorelease). A
//    naive "time one draw, divide" pilot was conflating draw with flush
//    and getting iters wildly wrong on cheap GPU benches: metal's flush
//    is more expensive than vulkan's, so metal looked slower than
//    vulkan, which is impossible since vulkan is MoltenVK on top of
//    metal on Apple Silicon.
// 2. Scale the converged iters so each timed sample is ~target_secs:
//    target_iters = pilot_iters * target_secs / pilot_time.
// 3. Take up to `samples` samples at the calibrated iters and report
//    the min as ns/px. Bail early once total wall exceeds the budget so
//    a single very expensive draw can't multiply by samples. Min is
//    robust to one-sided positive jitter (preemption, thermal blips,
//    GPU contention) — the noise we actually see in practice — and
//    gives a stable point estimate for regression detection across
//    commits.
static double bench(draw_fn draw, void *ctx, struct umbra_backend *be,
                    int w, int h, int samples, double target_secs,
                    double *out_gpu_ns_px) {
    int    iters   = 1;
    double t_pilot = 0;
    for (int p = 0; p < 20; p++) {
        double const start = now();
        for (int it = 0; it < iters; it++) { draw(ctx); }
        be->flush(be);
        t_pilot = now() - start;
        if (t_pilot >= target_secs / 2) { break; }
        iters *= 2;
    }

    int const calibrated = (int)((double)iters * target_secs / t_pilot);
    if (calibrated > iters) { iters = calibrated; }

    double const wall_budget    = (double)samples * target_secs;
    double       best           = 0;
    double       best_gpu       = 0;
    double       total_elapsed  = 0;
    for (int k = 0; k < samples; k++) {
        double const gpu0  = be->gpu_time ? be->gpu_time(be) : 0;
        double const start = now();
        for (int it = 0; it < iters; it++) { draw(ctx); }
        be->flush(be);
        double const dt     = now() - start;
        double const gpu_dt = be->gpu_time ? be->gpu_time(be) - gpu0 : 0;
        if (k == 0 || dt < best)         { best     = dt; }
        if (k == 0 || gpu_dt < best_gpu) { best_gpu = gpu_dt; }
        total_elapsed += dt;
        if (total_elapsed >= wall_budget) { break; }
    }
    double const px = (double)iters * (double)w * (double)h;
    if (out_gpu_ns_px) {
        *out_gpu_ns_px = be->gpu_time ? best_gpu / px * 1e9 : -1;
    }
    return best / px * 1e9;
}

static _Bool streq(char const *a, char const *b) { return strcmp(a, b) == 0; }

static void usage(void) {
    fprintf(stderr,
        "Usage: bench [options]\n"
        "  --fmt FORMAT     destination format: 8888, 565, 1010102, fp16, fp16_planar\n"
        "  --backend NAME   run only: interp, jit, metal, vulkan\n"
        "  --match SUBSTR   run only slides whose title contains SUBSTR\n"
        "  --samples N      timing samples per measurement, take min (default 5)\n"
        "  --target-ms N    target ms per sample (default 15)\n"
        "  --width N        canvas width in pixels (default 4096)\n"
        "  --help           show this help\n");
}

static struct umbra_fmt parse_fmt(char const *s) {
    if (streq(s, "8888"))        return umbra_fmt_8888;
    if (streq(s, "565"))         return umbra_fmt_565;
    if (streq(s, "1010102"))     return umbra_fmt_1010102;
    if (streq(s, "fp16"))        return umbra_fmt_fp16;
    if (streq(s, "fp16_planar")) return umbra_fmt_fp16_planar;
    fprintf(stderr, "unknown format: %s\n", s);
    usage();
    exit(1);
}

int main(int argc, char *argv[]) {
    int              W         = 4096;
    struct umbra_fmt fmt       = umbra_fmt_8888;
    int              be_mask   = 0xf;
    int              samples   = 5;
    int              target_ms = 15;
    char const      *match     = NULL;

    for (int i = 1; i < argc; i++) {
        if (streq(argv[i], "--help") || streq(argv[i], "-h")) { usage(); return 0; }
        else if (streq(argv[i], "--fmt")     && i+1 < argc) { fmt = parse_fmt(argv[++i]); }
        else if (streq(argv[i], "--backend") && i+1 < argc) {
            char const *b = argv[++i];
            be_mask = streq(b, "interp") ? 1 : streq(b, "jit") ? 2 :
                      streq(b, "metal")  ? 4 : streq(b, "vulkan") ? 8 : 0;
            if (!be_mask) { fprintf(stderr, "unknown backend: %s\n", b); usage(); return 1; }
        }
        else if (streq(argv[i], "--match")     && i+1 < argc) { match = argv[++i]; }
        else if (streq(argv[i], "--samples")   && i+1 < argc) { samples = atoi(argv[++i]); }
        else if (streq(argv[i], "--target-ms") && i+1 < argc) { target_ms = atoi(argv[++i]); }
        else if (streq(argv[i], "--width")     && i+1 < argc) { W = atoi(argv[++i]); }
        else { W = atoi(argv[i]); }
    }
    if (samples < 1)   { samples   = 1; }
    if (target_ms < 1) { target_ms = 1; }
    double const target_secs = (double)target_ms * 1e-3;

    int const H = 480;
    slides_init(W, H);

    int ns = slide_count() - 1;

    char const *be_names[] = {"interp", "jit", "metal", "vulkan"};
    struct umbra_backend *bes[] = {
        umbra_backend_interp(), umbra_backend_jit(),
        umbra_backend_metal(),  umbra_backend_vulkan(),
    };
    int const nb = (int)(sizeof bes / sizeof *bes);

    _Bool any_anomaly = 0;

    printf("%-40s", "ns/px");
    for (int bi = 0; bi < nb; bi++) {
        if (!(be_mask & (1 << bi))) { continue; }
        char hdr[32];
        if (bes[bi] && bes[bi]->gpu_time) {
            sprintf(hdr, "%s gpu%%", be_names[bi]);
        } else {
            sprintf(hdr, "%s", be_names[bi]);
        }
        printf("| %-13s", hdr);
    }
    printf("\n");

    for (int si = 0; si < ns; si++) {
        struct slide *s = slide_get(si);
        if (!s->draw) { continue; }
        if (match && !strstr(s->title, match)) { continue; }

        size_t const bpp    = fmt.bpp;
        size_t const planes = (size_t)fmt.planes;
        void        *buf    = calloc((size_t)(W * H) * planes, bpp);

        double ns_px[4] = {-1, -1, -1, -1};
        double gpu[4]   = {-1, -1, -1, -1};
        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi)) || !bes[bi]) { continue; }
            s->prepare(s, bes[bi], fmt);
            struct slide_draw_ctx sctx = {.s=s, .buf=buf, .frame=0, .w=W, .h=H};
            ns_px[bi] = bench(slide_draw, &sctx, bes[bi], W, H, samples, target_secs,
                              &gpu[bi]);
        }

        printf("%-40s", s->title);
        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi))) { continue; }
            if (ns_px[bi] < 0) { printf("  %-13s", "-"); continue; }
            char tmp[32];
            if (gpu[bi] >= 0 && ns_px[bi] > 0) {
                int pct = (int)(gpu[bi] / ns_px[bi] * 100 + 0.5);
                sprintf(tmp, "%.2f  %d", ns_px[bi], pct);
            } else {
                sprintf(tmp, "%.2f", ns_px[bi]);
            }
            printf("  %-13s", tmp);
        }
        // Anomaly markers: interp should never beat jit, and vulkan should
        // never beat metal on Apple Silicon (vulkan is MoltenVK on top of
        // metal). Either case is a measurement bug or a real perf
        // regression worth investigating. Compare rounded-to-printed
        // precision so visually-tied numbers don't fire on float noise.
        int const ri = ns_px[0] >= 0 ? (int)(ns_px[0] * 100 + 0.5) : -1;
        int const rj = ns_px[1] >= 0 ? (int)(ns_px[1] * 100 + 0.5) : -1;
        int const rm = ns_px[2] >= 0 ? (int)(ns_px[2] * 100 + 0.5) : -1;
        int const rv = ns_px[3] >= 0 ? (int)(ns_px[3] * 100 + 0.5) : -1;
        _Bool anomaly = 0;
        if (ri >= 0 && rj >= 0 && ri < rj) { anomaly = 1; }
        if (rv >= 0 && rm >= 0 && rv < rm) { anomaly = 1; }
        if (anomaly) { printf(" !"); any_anomaly = 1; }
        printf("\n");

        free(buf);
    }

    if (!match || strstr("slug accumulator (1 curve)", match)) {
        struct slug_curves       sc  = slug_extract("Slug", (float)H * 0.3125f);
        struct slug_acc_layout   al;
        struct umbra_builder    *bld = slug_build_acc(&al);
        struct umbra_basic_block *bb = umbra_basic_block(bld);
        umbra_builder_free(bld);

        struct umbra_program *progs[4];
        for (int bi = 0; bi < nb; bi++) {
            progs[bi] = bes[bi] ? bes[bi]->compile(bes[bi], bb) : NULL;
        }
        umbra_basic_block_free(bb);

        float *wind = calloc((size_t)(W * H), 4);
        float  mat[11] = {0};
        slide_perspective_matrix(mat, 0.0f, W, H, (int)sc.w, (int)sc.h);
        mat[9] = sc.w;
        mat[10] = sc.h;
        umbra_uniforms_fill_f32(al.uniforms, al.mat, mat, 11);
        umbra_uniforms_fill_ptr(al.uniforms, al.curves_off,
                      (struct umbra_buf){.ptr=sc.data, .sz=(size_t)(sc.count * 6 * 4)});
        float j0;
        { int32_t z = 0; __builtin_memcpy(&j0, &z, 4); }
        umbra_uniforms_fill_f32(al.uniforms, al.loop_off, &j0, 1);
        struct umbra_buf abuf[] = {
            (struct umbra_buf){.ptr=al.uniforms, .sz=al.uni.size, .read_only=1},
            {.ptr=wind, .sz=(size_t)(W * H * 4)},
        };

        printf("\n%-40s", "slug accumulator (1 curve)");
        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi))) { continue; }
            char hdr[32];
            if (bes[bi] && bes[bi]->gpu_time) {
                sprintf(hdr, "%s gpu%%", be_names[bi]);
            } else {
                sprintf(hdr, "%s", be_names[bi]);
            }
            printf("| %-13s", hdr);
        }
        printf("\n%-40s", "");

        double ns_px[4] = {-1, -1, -1, -1};
        double gpu[4]   = {-1, -1, -1, -1};
        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi)) || !progs[bi]) { continue; }
            struct prog_draw_ctx pctx = {.p=progs[bi], .bufs=abuf, .w=W, .h=H};
            ns_px[bi] = bench(prog_draw, &pctx, bes[bi], W, H, samples, target_secs,
                              &gpu[bi]);
        }
        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi))) { continue; }
            if (ns_px[bi] < 0) { printf("  %-13s", "-"); continue; }
            char tmp[32];
            if (gpu[bi] >= 0 && ns_px[bi] > 0) {
                int pct = (int)(gpu[bi] / ns_px[bi] * 100 + 0.5);
                sprintf(tmp, "%.2f  %d", ns_px[bi], pct);
            } else {
                sprintf(tmp, "%.2f", ns_px[bi]);
            }
            printf("  %-13s", tmp);
        }
        int const ri = ns_px[0] >= 0 ? (int)(ns_px[0] * 100 + 0.5) : -1;
        int const rj = ns_px[1] >= 0 ? (int)(ns_px[1] * 100 + 0.5) : -1;
        int const rm = ns_px[2] >= 0 ? (int)(ns_px[2] * 100 + 0.5) : -1;
        int const rv = ns_px[3] >= 0 ? (int)(ns_px[3] * 100 + 0.5) : -1;
        _Bool anomaly = 0;
        if (ri >= 0 && rj >= 0 && ri < rj) { anomaly = 1; }
        if (rv >= 0 && rm >= 0 && rv < rm) { anomaly = 1; }
        if (anomaly) { printf(" !"); any_anomaly = 1; }
        printf("\n");

        for (int bi = 0; bi < nb; bi++) {
            if (progs[bi]) { progs[bi]->free(progs[bi]); }
        }
        free(wind);
        free(al.uniforms);
        slug_free(&sc);
    }

    // Compile-time benchmarks: time builder → basic_block → compile → free
    // per slide.
    if (!match || strstr("compile", match)) {
        printf("\n%-40s", "compile (µs)");
        for (int bi = 0; bi < nb; bi++) {
            if (be_mask & (1 << bi)) { printf("| %-13s", be_names[bi]); }
        }
        printf("\n");

        for (int si = 0; si < ns; si++) {
            struct slide *s = slide_get(si);
            if (!s->get_builder) { continue; }
            if (match && !strstr(s->title, match) && !strstr("compile", match)) {
                continue;
            }

            double us_call[4] = {-1, -1, -1, -1};
            for (int bi = 0; bi < nb; bi++) {
                if (!(be_mask & (1 << bi)) || !bes[bi]) { continue; }
                    // Warm up: single compile to populate caches.
                    {
                        struct umbra_builder *b = s->get_builder(s, fmt);
                        if (!b) { continue; }
                        struct umbra_basic_block *bb = umbra_basic_block(b);
                        umbra_builder_free(b);
                        struct umbra_program *p = bes[bi]->compile(bes[bi], bb);
                        p->free(p);
                        umbra_basic_block_free(bb);
                    }

                    int    iters   = 1;
                    double t_pilot = 0;
                    for (int pi = 0; pi < 20; pi++) {
                        double const start = now();
                        for (int it = 0; it < iters; it++) {
                            struct umbra_builder *b = s->get_builder(s, fmt);
                            struct umbra_basic_block *bb = umbra_basic_block(b);
                            umbra_builder_free(b);
                            struct umbra_program *p = bes[bi]->compile(bes[bi], bb);
                            p->free(p);
                            umbra_basic_block_free(bb);
                        }
                        t_pilot = now() - start;
                        if (t_pilot >= target_secs / 2) { break; }
                        iters *= 2;
                    }
                    int const cal = (int)((double)iters * target_secs / t_pilot);
                    if (cal > iters) { iters = cal; }

                    double best = 0;
                    double total = 0;
                    for (int k = 0; k < samples; k++) {
                        double const start = now();
                        for (int it = 0; it < iters; it++) {
                            struct umbra_builder *b = s->get_builder(s, fmt);
                            struct umbra_basic_block *bb = umbra_basic_block(b);
                            umbra_builder_free(b);
                            struct umbra_program *p = bes[bi]->compile(bes[bi], bb);
                            p->free(p);
                            umbra_basic_block_free(bb);
                        }
                        double const dt = now() - start;
                        if (k == 0 || dt < best) { best = dt; }
                        total += dt;
                        if (total >= (double)samples * target_secs) { break; }
                    }
                    us_call[bi] = best / (double)iters * 1e6;
                }
            printf("%-40s", s->title);
            for (int bi = 0; bi < nb; bi++) {
                if (!(be_mask & (1 << bi))) { continue; }
                if (us_call[bi] < 0) { printf("  %-13s", "-"); continue; }
                char tmp[32];
                sprintf(tmp, "%.1f", us_call[bi]);
                printf("  %-13s", tmp);
            }
            printf("\n");
        }
    }

    slides_cleanup();
    for (int bi = 0; bi < nb; bi++) {
        if (bes[bi]) { bes[bi]->free(bes[bi]); }
    }
    return any_anomaly ? 1 : 0;
}
