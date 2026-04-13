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
                    double *out_gpu_ns_px, struct umbra_backend_stats *out_stats) {
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
    struct umbra_backend_stats best_stats = {0};
    for (int k = 0; k < samples; k++) {
        struct umbra_backend_stats const s0 = be->stats(be);
        double const start = now();
        for (int it = 0; it < iters; it++) { draw(ctx); }
        be->flush(be);
        double const dt = now() - start;
        struct umbra_backend_stats const s1 = be->stats(be);
        double const gpu_dt = s1.gpu_sec - s0.gpu_sec;
        if (k == 0 || dt < best) {
            best = dt;
            best_stats = (struct umbra_backend_stats){
                .gpu_sec    = gpu_dt,
                .encode_sec = s1.encode_sec - s0.encode_sec,
                .submit_sec = s1.submit_sec - s0.submit_sec,
                .dispatches = s1.dispatches - s0.dispatches,
                .submits    = s1.submits    - s0.submits,
                .upload_bytes = s1.upload_bytes - s0.upload_bytes,
                .uniform_ring_rotations = s1.uniform_ring_rotations
                                        - s0.uniform_ring_rotations,
            };
        }
        if (k == 0 || gpu_dt < best_gpu) { best_gpu = gpu_dt; }
        total_elapsed += dt;
        if (total_elapsed >= wall_budget) { break; }
    }
    double const px = (double)iters * (double)w * (double)h;
    if (out_gpu_ns_px) {
        *out_gpu_ns_px = best_gpu > 0 ? best_gpu / px * 1e9 : -1;
    }
    if (out_stats) { *out_stats = best_stats; }
    return best / px * 1e9;
}

static _Bool streq(char const *a, char const *b) { return strcmp(a, b) == 0; }

static void usage(void) {
    fprintf(stderr,
        "Usage: bench [options]\n"
        "  --fmt FORMAT     destination format: 8888, 565, 1010102, fp16, fp16_planar\n"
        "  --backend NAME   run only: interp, jit, metal, vulkan, wgpu\n"
        "  --match SUBSTR   run only slides whose title contains SUBSTR\n"
        "  --samples N      timing samples per measurement, take min (default 5)\n"
        "  --target-ms N    target ms per sample (default 15)\n"
        "  --width N        canvas width in pixels (default 4096)\n"
        "  --help           show this help\n");
}

static struct umbra_fmt parse_fmt(char const *s) {
    if (streq(s, "8888"))        { return umbra_fmt_8888; }
    if (streq(s, "565"))         { return umbra_fmt_565; }
    if (streq(s, "1010102"))     { return umbra_fmt_1010102; }
    if (streq(s, "fp16"))        { return umbra_fmt_fp16; }
    if (streq(s, "fp16_planar")) { return umbra_fmt_fp16_planar; }
    fprintf(stderr, "unknown format: %s\n", s);
    usage();
    exit(1);
}

// Display order: M(metal), V(vulkan), W(wgpu), J(jit), I(interp).
// Expected ranking: fastest to slowest, left to right.
enum { ND = 5, TITLE_W = 36, VAL_W = 7, CPU_W = 5, GAP = 2 };
static int const disp_idx[ND] = {2, 3, 4, 1, 0};
static char const *const disp_name[ND] = {"metal", "vulkan", "wgpu", "jit", "interp"};
static _Bool const disp_gpu[ND] = {1, 1, 1, 0, 0};

static void print_ns_header(int be_mask) {
    printf("%-*s", TITLE_W, "ns/px");
    _Bool first = 1;
    for (int d = 0; d < ND; d++) {
        if (!(be_mask & (1 << disp_idx[d]))) { continue; }
        if (!first) { printf("%*s", GAP, ""); }
        first = 0;
        if (disp_gpu[d]) { printf("%*s%*s", VAL_W, disp_name[d], CPU_W, "cpu%"); }
        else             { printf("%*s", VAL_W, disp_name[d]); }
    }
    printf("\n");
}

static void print_compile_header(int be_mask) {
    printf("\n%-*s ", TITLE_W, "compile \xc2\xb5s");
    _Bool first = 1;
    for (int d = 0; d < ND; d++) {
        if (!(be_mask & (1 << disp_idx[d]))) { continue; }
        if (!first) { printf("%*s", GAP, ""); }
        first = 0;
        if (disp_gpu[d]) { printf("%*s%*s", VAL_W, disp_name[d], CPU_W, ""); }
        else             { printf("%*s", VAL_W, disp_name[d]); }
    }
    printf("\n");
}

static _Bool print_row(char const *title, double ns_px[5], double gpu[5],
                       int be_mask) {
    printf("%-*s", TITLE_W, title);
    _Bool first = 1;
    for (int d = 0; d < ND; d++) {
        int bi = disp_idx[d];
        if (!(be_mask & (1 << bi))) { continue; }
        if (!first) { printf("%*s", GAP, ""); }
        first = 0;
        if (ns_px[bi] < 0) {
            printf("%*s", VAL_W + (disp_gpu[d] ? CPU_W : 0), "-");
            continue;
        }
        printf("%*.2f", VAL_W, ns_px[bi]);
        if (disp_gpu[d]) {
            if (gpu[bi] >= 0 && ns_px[bi] > 0) {
                int pct = 100 - (int)(gpu[bi] / ns_px[bi] * 100 + 0.5);
                printf("%*d", CPU_W, pct);
            } else {
                printf("%*s", CPU_W, "");
            }
        }
    }

    _Bool anomaly = 0;
    for (int d = 0; d + 1 < ND; d++) {
        int a = disp_idx[d], b = disp_idx[d + 1];
        if (!(be_mask & (1 << a)) || !(be_mask & (1 << b))) { continue; }
        if (ns_px[a] < 0 || ns_px[b] < 0) { continue; }
        int ra = (int)(ns_px[a] * 100 + 0.5);
        int rb = (int)(ns_px[b] * 100 + 0.5);
        if (ra > rb) { anomaly = 1; }
    }
    if (anomaly) { printf(" !"); }
    printf("\n");
    return anomaly;
}

int main(int argc, char *argv[]) {
    int              W         = 4096;
    struct umbra_fmt fmt       = umbra_fmt_fp16;
    int              be_mask   = 0x1f;
    int              samples   = 5;
    int              target_ms = 15;
    char const      *match     = NULL;
    _Bool            verbose   = 0;

    for (int i = 1; i < argc; i++) {
        if (streq(argv[i], "--help") || streq(argv[i], "-h")) { usage(); return 0; }
        else if (streq(argv[i], "--verbose") || streq(argv[i], "-v")) { verbose = 1; }
        else if (streq(argv[i], "--fmt")     && i+1 < argc) { fmt = parse_fmt(argv[++i]); }
        else if (streq(argv[i], "--backend") && i+1 < argc) {
            char const *b = argv[++i];
            int bit = streq(b, "interp") ? 1  : streq(b, "jit")    ? 2 :
                      streq(b, "metal")  ? 4  : streq(b, "vulkan") ? 8 :
                      streq(b, "wgpu")   ? 16 : 0;
            if (!bit) { fprintf(stderr, "unknown backend: %s\n", b); usage(); return 1; }
            if (be_mask == 0x1f) { be_mask = 0; }
            be_mask |= bit;
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

    struct umbra_backend *bes[] = {
        umbra_backend_interp(), umbra_backend_jit(),
        umbra_backend_metal(),  umbra_backend_vulkan(),
        umbra_backend_wgpu(),
    };
    int const nb = (int)(sizeof bes / sizeof *bes);

    _Bool any_anomaly = 0;

    print_ns_header(be_mask);

    for (int si = 0; si < ns; si++) {
        struct slide *s = slide_get(si);
        if (!s->draw) { continue; }
        if (match && !strstr(s->title, match)) { continue; }

        size_t const bpp    = fmt.bpp;
        size_t const planes = (size_t)fmt.planes;
        void        *buf    = calloc((size_t)(W * H) * planes, bpp);

        double ns_px[5] = {-1, -1, -1, -1, -1};
        double gpu[5]   = {-1, -1, -1, -1, -1};
        struct umbra_backend_stats bstats[5] = {{0}};
        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi)) || !bes[bi]) { continue; }
            s->prepare(s, bes[bi], fmt);
            struct slide_draw_ctx sctx = {.s=s, .buf=buf, .frame=0, .w=W, .h=H};
            ns_px[bi] = bench(slide_draw, &sctx, bes[bi], W, H, samples, target_secs,
                              &gpu[bi], &bstats[bi]);
        }
        any_anomaly |= print_row(s->title, ns_px, gpu, be_mask);
        if (verbose) {
            for (int d = 0; d < ND; d++) {
                int bi = disp_idx[d];
                if (!(be_mask & (1 << bi)) || ns_px[bi] < 0) { continue; }
                struct umbra_backend_stats const *st = &bstats[bi];
                double const us_per = st->dispatches
                    ? st->gpu_sec / st->dispatches * 1e6 : 0;
                fprintf(stderr, "  %s: %.3fms gpu"
                                " \xc3\xb7 %d dispatches"
                                " = %.0f \xc2\xb5s/dispatch\n",
                        disp_name[d], st->gpu_sec * 1e3,
                        st->dispatches, us_per);
            }
        }
        free(buf);
    }

    if (!match || strstr("slug accumulator (1 curve)", match)) {
        struct slug_curves       sc  = slug_extract("Slug", (float)H * 0.3125f);
        struct slug_acc_layout   al;
        struct umbra_builder    *bld = slug_build_acc(&al);
        struct umbra_flat_ir *bb = umbra_flat_ir(bld);
        umbra_builder_free(bld);

        struct umbra_program *progs[5];
        for (int bi = 0; bi < nb; bi++) {
            progs[bi] = bes[bi] ? bes[bi]->compile(bes[bi], bb) : NULL;
        }
        umbra_flat_ir_free(bb);

        float *wind = calloc((size_t)(W * H), 4);
        struct umbra_matrix mat;
        slide_perspective_matrix(&mat, 0.0f, W, H, (int)sc.w, (int)sc.h);
        umbra_uniforms_fill_f32(al.uniforms, al.mat, &mat.sx, 9);
        float const wh[2] = {sc.w, sc.h};
        umbra_uniforms_fill_f32(al.uniforms, al.wh, wh, 2);
        umbra_uniforms_fill_ptr(al.uniforms, al.curves_off,
                      (struct umbra_buf){.ptr=sc.data, .count=sc.count * 6});
        float j0;
        { int32_t z = 0; __builtin_memcpy(&j0, &z, 4); }
        umbra_uniforms_fill_f32(al.uniforms, al.loop_off, &j0, 1);
        struct umbra_buf abuf[] = {
            (struct umbra_buf){.ptr=al.uniforms, .count=al.uni.slots},
            {.ptr=wind, .count=W * H},
        };

        double ns_px[5] = {-1, -1, -1, -1, -1};
        double gpu[5]   = {-1, -1, -1, -1, -1};
        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi)) || !progs[bi]) { continue; }
            struct prog_draw_ctx pctx = {.p=progs[bi], .bufs=abuf, .w=W, .h=H};
            ns_px[bi] = bench(prog_draw, &pctx, bes[bi], W, H, samples, target_secs,
                              &gpu[bi], NULL);
        }
        any_anomaly |= print_row("Slug Accumulator (1 curve)", ns_px, gpu, be_mask);

        for (int bi = 0; bi < nb; bi++) {
            if (progs[bi]) { progs[bi]->free(progs[bi]); }
        }
        free(wind);
        free(al.uniforms);
        slug_free(&sc);
    }

    // Compile-time benchmarks.
    if (!match || strstr("compile", match)) {
        print_compile_header(be_mask);

        for (int si = 0; si < ns; si++) {
            struct slide *s = slide_get(si);
            if (!s->get_builder) { continue; }
            if (match && !strstr(s->title, match) && !strstr("compile", match)) {
                continue;
            }

            double us_call[5] = {-1, -1, -1, -1, -1};
            for (int bi = 0; bi < nb; bi++) {
                if (!(be_mask & (1 << bi)) || !bes[bi]) { continue; }
                {
                    struct umbra_builder *b = s->get_builder(s, fmt);
                    if (!b) { continue; }
                    struct umbra_flat_ir *bb2 = umbra_flat_ir(b);
                    umbra_builder_free(b);
                    struct umbra_program *p = bes[bi]->compile(bes[bi], bb2);
                    p->free(p);
                    umbra_flat_ir_free(bb2);
                }

                int    iters   = 1;
                double t_pilot = 0;
                for (int pi = 0; pi < 20; pi++) {
                    double const start = now();
                    for (int it = 0; it < iters; it++) {
                        struct umbra_builder *b = s->get_builder(s, fmt);
                        struct umbra_flat_ir *bb2 = umbra_flat_ir(b);
                        umbra_builder_free(b);
                        struct umbra_program *p = bes[bi]->compile(bes[bi], bb2);
                        p->free(p);
                        umbra_flat_ir_free(bb2);
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
                        struct umbra_flat_ir *bb2 = umbra_flat_ir(b);
                        umbra_builder_free(b);
                        struct umbra_program *p = bes[bi]->compile(bes[bi], bb2);
                        p->free(p);
                        umbra_flat_ir_free(bb2);
                    }
                    double const dt = now() - start;
                    if (k == 0 || dt < best) { best = dt; }
                    total += dt;
                    if (total >= (double)samples * target_secs) { break; }
                }
                us_call[bi] = best / (double)iters * 1e6;
            }
            printf("%-*s", TITLE_W, s->title);
            _Bool first = 1;
            for (int d = 0; d < ND; d++) {
                int bi = disp_idx[d];
                if (!(be_mask & (1 << bi))) { continue; }
                if (!first) { printf("%*s", GAP, ""); }
                first = 0;
                if (us_call[bi] < 0) {
                    printf("%*s", VAL_W + (disp_gpu[d] ? CPU_W : 0), "-");
                } else if (disp_gpu[d]) {
                    printf("%*.1f%*s", VAL_W, us_call[bi], CPU_W, "");
                } else {
                    printf("%*.1f", VAL_W, us_call[bi]);
                }
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
