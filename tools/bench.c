#define _POSIX_C_SOURCE 200809L
#include "../slides/slide.h"
#include "../include/umbra.h"
#include "../include/umbra_draw.h"
#include "../src/count.h"
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
    double                secs;
    struct slide         *s;
    struct slide_runtime *rt;
    struct slide_bg      *bg;
    int                   w, h;
#if INTPTR_MAX != INT64_MAX
    int                   :32;
#endif
};
static void slide_draw(void *vctx) {
    struct slide_draw_ctx *c = vctx;
    slide_bg_draw(c->bg, c->s->bg, 0, 0, c->w, c->h, c->rt->dst_buf);
    slide_runtime_draw(c->rt, c->s, c->secs, 0, 0, c->w, c->h);
    c->secs += 1.0 / 60.0;
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
    draw(ctx);
    be->flush(be);

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

static void compile_all_builders(struct slide_runtime *rt, struct slide *s,
                                 struct umbra_fmt fmt,
                                 struct umbra_backend *be) {
    struct slide_builders b = slide_builders(rt, s, fmt, NULL);
    struct umbra_builder *builders[2] = {b.draw, b.bounds};
    for (int j = 0; j < 2; j++) {
        if (!builders[j]) { continue; }
        struct umbra_flat_ir *ir = umbra_flat_ir(builders[j]);
        umbra_builder_free(builders[j]);
        struct umbra_program *p = be->compile(be, ir);
        umbra_program_free(p);
        umbra_flat_ir_free(ir);
    }
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

static void print_fixed(int width, int precision, double v) {
    char buf[32];
    int const n = snprintf(buf, sizeof buf, "%.*f", precision, v);
    char const *s = buf;
    if (n >= 2 && buf[0] == '0' && buf[1] == '.') { s++; }
    printf("%*s", width, s);
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
        print_fixed(VAL_W, 2, ns_px[bi]);
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
    int const nb = count(bes);

    _Bool any_anomaly = 0;

    print_ns_header(be_mask);

    for (int si = 0; si < ns; si++) {
        struct slide *s = slide_get(si);
        if (!s->build_draw && !s->build_sdf_draw) { continue; }
        if (match && !strstr(s->title, match)) { continue; }

        size_t const bpp    = fmt.bpp;
        size_t const planes = (size_t)fmt.planes;
        void        *buf    = calloc((size_t)(W * H) * planes, bpp);

        double ns_px[5] = {-1, -1, -1, -1, -1};
        double gpu[5]   = {-1, -1, -1, -1, -1};
        struct umbra_backend_stats bstats[5] = {{0}};
        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi)) || !bes[bi]) { continue; }
            struct slide_runtime *rt = slide_runtime(s, W, H, bes[bi], fmt, NULL);
            struct slide_bg      *bg = slide_bg(bes[bi], fmt);
            rt->dst_buf = (struct umbra_buf){.ptr=buf, .count=W*H*fmt.planes, .stride=W};
            struct slide_draw_ctx sctx = {.s=s, .rt=rt, .bg=bg, .secs=0.0, .w=W, .h=H};
            ns_px[bi] = bench(slide_draw, &sctx, bes[bi], W, H, samples, target_secs,
                              &gpu[bi], &bstats[bi]);
            slide_bg_free(bg);
            slide_runtime_free(rt);
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

    // Compile-time benchmarks.
    if (!match || strstr("compile", match)) {
        print_compile_header(be_mask);

        for (int si = 0; si < ns; si++) {
            struct slide *s = slide_get(si);
            if (!s->build_draw && !s->build_sdf_draw) { continue; }
            if (match && !strstr(s->title, match) && !strstr("compile", match)) {
                continue;
            }

            double us_call[5] = {-1, -1, -1, -1, -1};
            struct slide_runtime *rt = calloc(1, sizeof *rt);
            for (int bi = 0; bi < nb; bi++) {
                if (!(be_mask & (1 << bi)) || !bes[bi]) { continue; }
                compile_all_builders(rt, s, fmt, bes[bi]);

                int    iters   = 1;
                double t_pilot = 0;
                for (int pi = 0; pi < 20; pi++) {
                    double const start = now();
                    for (int it = 0; it < iters; it++) {
                        compile_all_builders(rt, s, fmt, bes[bi]);
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
                        compile_all_builders(rt, s, fmt, bes[bi]);
                    }
                    double const dt = now() - start;
                    if (k == 0 || dt < best) { best = dt; }
                    total += dt;
                    if (total >= (double)samples * target_secs) { break; }
                }
                us_call[bi] = best / (double)iters * 1e6;
            }
            slide_runtime_free(rt);
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
                    print_fixed(VAL_W, 1, us_call[bi]);
                    printf("%*s", CPU_W, "");
                } else {
                    print_fixed(VAL_W, 1, us_call[bi]);
                }
            }
            printf("\n");
        }
    }

    slides_cleanup();
    for (int bi = 0; bi < nb; bi++) {
        umbra_backend_free(bes[bi]);
    }
    return any_anomaly ? 1 : 0;
}
