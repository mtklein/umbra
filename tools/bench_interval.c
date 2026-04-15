// Micro-benchmark for interval_program_run.  Plan 02's quadtree dispatcher
// calls this once per tile on the descent, so its per-call cost (and per-op
// cost) sets the floor on how fine-grained we can subdivide before the
// overhead eats the savings.  Target per the plan: ~200 ns / call on a ~14-op
// circle-SDF coverage (that's about 14 ns / op).
//
// Builds a circle-SDF coverage IR mirroring slides/circle.c's circle_build_,
// wraps it in an interval_program, then times interval_program_run over a
// representative spread of (x,y) intervals:
//
//   - "far":   large intervals that straddle the circle entirely
//   - "edge":  medium intervals at the boundary (the common case during
//              quadtree descent)
//   - "tiny":  leaf-sized intervals (near-point evaluation — the dispatcher's
//              min_tile regime)

#define _POSIX_C_SOURCE 200809L
#include "../include/umbra.h"
#include "../src/flat_ir.h"
#include "../src/interval.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static struct umbra_flat_ir* build_circle_ir(void) {
    struct umbra_builder *b  = umbra_builder();
    umbra_ptr32 const sink    = {.ix = 0},
                      uniform = {.ix = 1};
    umbra_val32 const x  = umbra_x(b),
                      y  = umbra_y(b),
                      cx = umbra_uniform_32(b, uniform, 0),
                      cy = umbra_uniform_32(b, uniform, 1),
                      r  = umbra_uniform_32(b, uniform, 2),
                      dx = umbra_sub_f32(b, x, cx),
                      dy = umbra_sub_f32(b, y, cy),
                      d2 = umbra_add_f32(b, umbra_mul_f32(b, dx, dx),
                                            umbra_mul_f32(b, dy, dy)),
                      d  = umbra_sqrt_f32(b, d2),
                      a  = umbra_min_f32(b, umbra_imm_f32(b, 1.0f),
                               umbra_max_f32(b, umbra_imm_f32(b, 0.0f),
                                                umbra_sub_f32(b, r, d)));
    umbra_store_32(b, sink, a);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    return ir;
}

struct sample { interval x, y; };

// sink absorbs results so the compiler can't drop the run() call.
static float sink_absorb = 0;
static void time_samples(char const *label, struct interval_program *p,
                         float const *uniform, struct sample const *s, int n,
                         double target_secs, int samples) {
    // Pilot: grow iters until one round takes ~target_secs/2.
    int    iters   = 16;
    double t_pilot = 0;
    for (int pi = 0; pi < 30; pi++) {
        double const start = now();
        for (int it = 0; it < iters; it++) {
            struct sample const sa = s[it & (n-1)];
            interval const out = interval_program_run(p, sa.x, sa.y, uniform);
            sink_absorb += out.lo + out.hi;
        }
        t_pilot = now() - start;
        if (t_pilot >= target_secs / 2) { break; }
        iters *= 2;
    }
    int const cal = (int)((double)iters * target_secs / t_pilot);
    if (cal > iters) { iters = cal; }

    double best = 0;
    for (int k = 0; k < samples; k++) {
        double const start = now();
        for (int it = 0; it < iters; it++) {
            struct sample const sa = s[it & (n-1)];
            interval const out = interval_program_run(p, sa.x, sa.y, uniform);
            sink_absorb += out.lo + out.hi;
        }
        double const dt = now() - start;
        if (k == 0 || dt < best) { best = dt; }
    }
    double const ns_call = best / (double)iters * 1e9;
    printf("  %-8s %.1f ns/call\n", label, ns_call);
}

int main(void) {
    struct umbra_flat_ir    *ir  = build_circle_ir();
    struct interval_program *p   = interval_program(ir);
    if (!p) {
        fprintf(stderr, "interval_program() rejected the circle IR\n");
        return 1;
    }
    int const insts = ir->insts;
    umbra_flat_ir_free(ir);

    float const uniform[3] = {512.0f, 240.0f, 180.0f};

    // Power-of-two sample counts so `it & (n-1)` suffices.
    struct sample far[8] = {
        {{   0, 1024}, {  0,  480}},
        {{ 128, 1152}, { 64,  544}},
        {{ 256, 1280}, {128,  608}},
        {{ 384, 1408}, {192,  672}},
        {{-256,  768}, {  0,  480}},
        {{ 512, 1536}, {  0,  480}},
        {{   0, 2048}, {-240, 720}},
        {{ 128, 1152}, {-64,  544}},
    };
    struct sample edge[8] = {
        {{ 300,  400}, {  60,  160}},
        {{ 620,  720}, {  60,  160}},
        {{ 460,  560}, { -80,   20}},
        {{ 460,  560}, { 400,  500}},
        {{ 332,  348}, { 230,  250}},
        {{ 676,  692}, { 230,  250}},
        {{ 500,  524}, {  58,   74}},
        {{ 500,  524}, { 406,  422}},
    };
    struct sample tiny[8] = {
        {{ 512.0f, 513.0f}, { 240.0f, 241.0f}},
        {{ 400.0f, 401.0f}, { 120.0f, 121.0f}},
        {{ 332.0f, 333.0f}, { 240.0f, 241.0f}},
        {{ 100.0f, 101.0f}, { 100.0f, 101.0f}},
        {{ 700.0f, 701.0f}, { 300.0f, 301.0f}},
        {{ 692.0f, 693.0f}, { 240.0f, 241.0f}},
        {{ 512.0f, 513.0f}, {  60.0f,  61.0f}},
        {{ 512.0f, 513.0f}, { 420.0f, 421.0f}},
    };

    printf("circle-SDF coverage: %d IR insts\n", insts);
    time_samples("far",  p, uniform, far,  8, 0.05, 7);
    time_samples("edge", p, uniform, edge, 8, 0.05, 7);
    time_samples("tiny", p, uniform, tiny, 8, 0.05, 7);
    printf("  sink   %g (ignore)\n", (double)sink_absorb);

    interval_program_free(p);
    return 0;
}
