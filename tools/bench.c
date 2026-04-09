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

// Pilot + min-of-K timing.
//
// 1. Pilot: time one draw. Doubles as warmup; the cold-start pilot may be
//    slower than steady-state but min-of-K below filters that out as long
//    as at least one of the K samples lands in steady-state.
// 2. Calibrate iters so each timed sample is ~target_secs long.
// 3. Take up to `samples` samples; bail early once we've spent the wall
//    budget so a single very expensive draw can't multiply by samples.
// 4. Report the fastest sample as ns/px.
//
// Min is robust to one-sided positive jitter (preemption, thermal blips,
// GPU contention) — the noise we actually see in practice — and gives a
// stable point estimate for regression detection across commits.
static double bench(draw_fn draw, void *ctx, struct umbra_backend *be,
                    int w, int h, int samples, double target_secs) {
    double const pilot_start = now();
    draw(ctx);
    be->flush(be);
    double const t_one = now() - pilot_start;

    int iters = (int)(target_secs / (t_one > 1e-9 ? t_one : 1e-9));
    if (iters < 1) { iters = 1; }

    double const wall_budget    = (double)samples * target_secs;
    double       best           = 0;
    double       total_elapsed  = 0;
    for (int k = 0; k < samples; k++) {
        double const start = now();
        for (int it = 0; it < iters; it++) { draw(ctx); }
        be->flush(be);
        double const dt = now() - start;
        if (k == 0 || dt < best) { best = dt; }
        total_elapsed += dt;
        if (total_elapsed >= wall_budget) { break; }
    }
    return best / ((double)iters * (double)w * (double)h) * 1e9;
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
    int         W         = 4096;
    struct umbra_fmt const *fmt_ov = NULL;
    int         be_mask   = 0xf;
    int         samples   = 5;
    int         target_ms = 15;
    char const *match     = NULL;

    for (int i = 1; i < argc; i++) {
        if (streq(argv[i], "--help") || streq(argv[i], "-h")) { usage(); return 0; }
        else if (streq(argv[i], "--fmt")     && i+1 < argc) {
            static struct umbra_fmt parsed;
            parsed = parse_fmt(argv[++i]);
            fmt_ov = &parsed;
        }
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

    printf("%-40s", "");
    for (int bi = 0; bi < nb; bi++) {
        if (be_mask & (1 << bi)) { printf(" %12s", be_names[bi]); }
    }
    printf("\n");

    for (int si = 0; si < ns; si++) {
        struct slide *s = slide_get(si);
        if (!s->draw) { continue; }
        if (match && !strstr(s->title, match)) { continue; }

        struct umbra_fmt fmt = fmt_ov ? *fmt_ov : umbra_fmt_8888;
        size_t bpp = fmt.bpp;
        size_t planes = (size_t)fmt.planes;
        void *buf = calloc((size_t)(W * H) * planes, bpp);

        printf("%-40s", s->title);
        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi)) || !bes[bi]) {
                if (be_mask & (1 << bi)) { printf(" %12s", "-"); }
                continue;
            }
            s->prepare(s, bes[bi], fmt);
            struct slide_draw_ctx sctx = {.s=s, .buf=buf, .frame=0, .w=W, .h=H};
            char tmp[32];
            sprintf(tmp, "%5.2f ns/px",
                    bench(slide_draw, &sctx, bes[bi], W, H, samples, target_secs));
            printf(" %12s", tmp);
        }
        printf("\n");

        free(buf);
    }

    if (!match || strstr("slug accumulator (1 curve)", match)) {
        struct slug_curves        sc = slug_extract("Slug", (float)H * 0.3125f);
        struct slug_acc_layout    al;
        struct umbra_builder     *bld = slug_build_acc(&al);
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
            if (be_mask & (1 << bi)) { printf(" %12s", be_names[bi]); }
        }
        printf("\n%-40s", "");

        for (int bi = 0; bi < nb; bi++) {
            if (!(be_mask & (1 << bi)) || !progs[bi]) {
                if (be_mask & (1 << bi)) { printf(" %12s", "-"); }
                continue;
            }
            struct prog_draw_ctx pctx = {.p=progs[bi], .bufs=abuf, .w=W, .h=H};
            char tmp[32];
            sprintf(tmp, "%5.2f ns/px",
                    bench(prog_draw, &pctx, bes[bi], W, H, samples, target_secs));
            printf(" %12s", tmp);
        }
        printf("\n");

        for (int bi = 0; bi < nb; bi++) {
            if (progs[bi]) { progs[bi]->free(progs[bi]); }
        }
        free(wind);
        free(al.uniforms);
        slug_free(&sc);
    }

    slides_cleanup();
    for (int bi = 0; bi < nb; bi++) {
        if (bes[bi]) { bes[bi]->free(bes[bi]); }
    }
    return 0;
}
