// Micro-benchmark for compiled interval evaluation via umbra_interval.h.
// Evaluates a circle-SDF's interval bounds over a grid of tiles using
// the regular backends (interp, jit).

#define _POSIX_C_SOURCE 200809L
#include "../include/umbra.h"
#include "../include/umbra_interval.h"
#include "../src/count.h"
#include "../src/flat_ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

// Uniforms: 0=cx, 1=cy, 2=r, 3=base_x, 4=base_y, 5=tile_w, 6=tile_h
// x() and y() are tile indices.  Each pixel computes one tile's bounds.
// Stores f.lo to ptr 1.
static struct umbra_flat_ir* build_circle_interval_ir(int *out_insts) {
    struct umbra_builder *b = umbra_builder();
    umbra_ptr32 const u = {.ix = 0};

    umbra_interval const cx = umbra_interval_uniform(b, u, 0),
                         cy = umbra_interval_uniform(b, u, 1),
                         r  = umbra_interval_uniform(b, u, 2);

    umbra_val32 const base_x = umbra_uniform_32(b, u, 3),
                      base_y = umbra_uniform_32(b, u, 4),
                      tile_w = umbra_uniform_32(b, u, 5),
                      tile_h = umbra_uniform_32(b, u, 6);

    umbra_val32 const xf = umbra_f32_from_i32(b, umbra_x(b)),
                      yf = umbra_f32_from_i32(b, umbra_y(b));
    umbra_val32 const x_lo = umbra_add_f32(b, base_x, umbra_mul_f32(b, xf, tile_w)),
                      y_lo = umbra_add_f32(b, base_y, umbra_mul_f32(b, yf, tile_h)),
                      x_hi = umbra_add_f32(b, x_lo, tile_w),
                      y_hi = umbra_add_f32(b, y_lo, tile_h);

    umbra_interval const x = {x_lo, x_hi},
                         y = {y_lo, y_hi};

    umbra_interval const dx = umbra_interval_sub_f32(b, x, cx),
                         dy = umbra_interval_sub_f32(b, y, cy),
                         d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, dx, dx),
                                  umbra_interval_mul_f32(b, dy, dy)),
                         d  = umbra_interval_sqrt_f32(b, d2),
                         f  = umbra_interval_sub_f32(b, d, r);

    umbra_store_32(b, (umbra_ptr32){.ix = 1}, f.lo);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    *out_insts = ir->insts;
    return ir;
}

static volatile float sink_absorb;

static void time_compiled_tiled(char const *label,
                                struct umbra_program *prog,
                                struct umbra_backend *be,
                                float const *base_uniform,
                                float base_x, float base_y,
                                float tile_w, float tile_h,
                                int xt, int yt,
                                double target_secs, int trials) {
    int const tiles = xt * yt;
    float uniforms[7] = {base_uniform[0], base_uniform[1], base_uniform[2],
                         base_x, base_y, tile_w, tile_h};
    float *lo_out = calloc((size_t)(xt * yt), sizeof(float));
    struct umbra_buf buf[] = {
        {.ptr = uniforms, .count = 7},
        {.ptr = lo_out,   .count = xt * yt, .stride = xt},
    };

    int iters = 1;
    double t_pilot = 0;
    for (int pi = 0; pi < 30; pi++) {
        double const start = now();
        for (int it = 0; it < iters; it++) {
            prog->queue(prog, 0, 0, xt, yt, buf);
            be->flush(be);
        }
        t_pilot = now() - start;
        if (t_pilot >= target_secs / 2) { break; }
        iters *= 2;
    }
    int const cal = (int)((double)iters * target_secs / t_pilot);
    if (cal > iters) { iters = cal; }

    double best = 0;
    for (int k = 0; k < trials; k++) {
        double const start = now();
        for (int it = 0; it < iters; it++) {
            prog->queue(prog, 0, 0, xt, yt, buf);
            be->flush(be);
        }
        double const dt = now() - start;
        if (k == 0 || dt < best) { best = dt; }
    }
    double const ns_tile = best / (double)(iters * tiles) * 1e9;
    printf("  %-12s %6.1f ns/tile  (%d tiles, %dx%d)\n", label, ns_tile, tiles, xt, yt);

    free(lo_out);
}

int main(void) {
    float const uniform[3] = {512.0f, 240.0f, 180.0f};

    struct { int xt, yt; float tile_w, tile_h; } grids[] = {
        {  1,   1, 1024, 480},
        {  2,   2, 512, 240},
        {  4,   4, 256, 120},
        {  8,   8, 128,  60},
        { 16,  16,  64,  30},
        { 32,  32,  32,  15},
    };

    int insts = 0;
    struct umbra_flat_ir *ir = build_circle_interval_ir(&insts);
    printf("interval_builder.h (compiled, %d IR insts)\n", insts);

    struct umbra_backend *bes[] = {
        umbra_backend_interp(),
        umbra_backend_jit(),
    };
    char const *names[] = {"interp", "jit"};

    for (int bi = 0; bi < count(bes); bi++) {
        if (!bes[bi]) { continue; }
        struct umbra_program *prog = bes[bi]->compile(bes[bi], ir);
        printf("  %s:\n", names[bi]);
        for (int gi = 0; gi < count(grids); gi++) {
            char label[32];
            snprintf(label, sizeof label, "%dx%d", grids[gi].xt, grids[gi].yt);
            time_compiled_tiled(label, prog, bes[bi], uniform,
                               0, 0, grids[gi].tile_w, grids[gi].tile_h,
                               grids[gi].xt, grids[gi].yt, 0.05, 7);
        }
        prog->free(prog);
        bes[bi]->free(bes[bi]);
    }
    umbra_flat_ir_free(ir);

    (void)sink_absorb;
    return 0;
}
