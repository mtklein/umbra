// Micro-benchmark comparing two approaches to interval evaluation:
//
//   1. interval.h  — scalar CPU interpreter (interval_program_run)
//   2. interval_builder.h — compiled program via the regular backends
//
// Both evaluate the same circle-SDF interval math on the same inputs.

#define _POSIX_C_SOURCE 200809L
#include "../include/umbra.h"
#include "../src/count.h"
#include "../src/flat_ir.h"
#include "../src/interval.h"
#include "../src/interval_builder.h"
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
    umbra_ptr32 const uniform = {.ix = 0},
                      sink    = {.ix = 1};
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

// Build the same circle SDF using interval_builder.h.
// Uniforms: 0=cx, 1=cy, 2=r, 3=x.lo, 4=x.hi, 5=y.lo, 6=y.hi
// Stores result.lo to ptr 1, result.hi to ptr 2.
static struct umbra_flat_ir* build_circle_interval_ir(int *out_insts) {
    struct umbra_builder *b = umbra_builder();
    umbra_ptr32 const u = {.ix = 0};

    umbra_interval const cx = umbra_interval_uniform(b, u, 0),
                         cy = umbra_interval_uniform(b, u, 1),
                         r  = umbra_interval_uniform(b, u, 2),
                         x  = {umbra_uniform_32(b, u, 3), umbra_uniform_32(b, u, 4)},
                         y  = {umbra_uniform_32(b, u, 5), umbra_uniform_32(b, u, 6)};

    umbra_interval const dx = umbra_interval_sub_f32(b, x, cx),
                         dy = umbra_interval_sub_f32(b, y, cy),
                         d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, dx, dx),
                                  umbra_interval_mul_f32(b, dy, dy)),
                         d  = umbra_interval_sqrt_f32(b, d2),
                         f  = umbra_interval_sub_f32(b, d, r);

    umbra_store_32(b, (umbra_ptr32){.ix = 1}, f.lo);
    umbra_store_32(b, (umbra_ptr32){.ix = 2}, f.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    *out_insts = ir->insts;
    return ir;
}

struct sample { interval x, y; };

static volatile float sink_absorb;

static void time_interp(char const *label, struct interval_program *p,
                        float const *uniform,
                        struct sample const *xy, int xys,
                        double target_secs, int trials) {
    int    iters   = 16;
    double t_pilot = 0;
    for (int pi = 0; pi < 30; pi++) {
        double const start = now();
        for (int it = 0; it < iters; it++) {
            struct sample const sa = xy[it & (xys - 1)];
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
    for (int k = 0; k < trials; k++) {
        double const start = now();
        for (int it = 0; it < iters; it++) {
            struct sample const sa = xy[it & (xys - 1)];
            interval const out = interval_program_run(p, sa.x, sa.y, uniform);
            sink_absorb += out.lo + out.hi;
        }
        double const dt = now() - start;
        if (k == 0 || dt < best) { best = dt; }
    }
    printf("  %-8s %6.1f ns/call\n", label, best / (double)iters * 1e9);
}

static void time_compiled(char const *label, struct umbra_program *prog,
                          float const *base_uniform,
                          struct sample const *xy, int xys,
                          double target_secs, int trials) {
    int    iters   = 16;
    double t_pilot = 0;
    for (int pi = 0; pi < 30; pi++) {
        double const start = now();
        for (int it = 0; it < iters; it++) {
            struct sample const sa = xy[it & (xys - 1)];
            float uniforms[7] = {base_uniform[0], base_uniform[1], base_uniform[2],
                                 sa.x.lo, sa.x.hi, sa.y.lo, sa.y.hi};
            float lo_out = 0, hi_out = 0;
            struct umbra_buf buf[] = {
                {.ptr = uniforms, .count = 7},
                {.ptr = &lo_out,  .count = 1},
                {.ptr = &hi_out,  .count = 1},
            };
            prog->queue(prog, 0, 0, 1, 1, buf);
            sink_absorb += lo_out + hi_out;
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
            struct sample const sa = xy[it & (xys - 1)];
            float uniforms[7] = {base_uniform[0], base_uniform[1], base_uniform[2],
                                 sa.x.lo, sa.x.hi, sa.y.lo, sa.y.hi};
            float lo_out = 0, hi_out = 0;
            struct umbra_buf buf[] = {
                {.ptr = uniforms, .count = 7},
                {.ptr = &lo_out,  .count = 1},
                {.ptr = &hi_out,  .count = 1},
            };
            prog->queue(prog, 0, 0, 1, 1, buf);
            sink_absorb += lo_out + hi_out;
        }
        double const dt = now() - start;
        if (k == 0 || dt < best) { best = dt; }
    }
    printf("  %-8s %6.1f ns/call\n", label, best / (double)iters * 1e9);
}

int main(void) {
    float const uniform[3] = {512.0f, 240.0f, 180.0f};

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

    // --- interval.h (scalar CPU interpreter) ---
    {
        struct umbra_flat_ir *ir = build_circle_ir();
        struct interval_program *p = interval_program(ir);
        if (!p) {
            fprintf(stderr, "interval_program() rejected the circle IR\n");
            return 1;
        }
        printf("interval.h (scalar interpreter, %d IR insts)\n", ir->insts);
        umbra_flat_ir_free(ir);
        time_interp("edge", p, uniform, edge, count(edge), 0.05, 7);
        interval_program_free(p);
    }

    // --- interval_builder.h (compiled via backends) ---
    {
        int insts = 0;
        struct umbra_flat_ir *ir = build_circle_interval_ir(&insts);
        printf("\ninterval_builder.h (compiled, %d IR insts)\n", insts);

        struct umbra_backend *bes[] = {
            umbra_backend_interp(),
            umbra_backend_jit(),
        };
        char const *names[] = {"interp", "jit"};

        for (int bi = 0; bi < count(bes); bi++) {
            if (!bes[bi]) { continue; }
            struct umbra_program *prog = bes[bi]->compile(bes[bi], ir);
            printf("  %s:\n", names[bi]);
            time_compiled("edge", prog, uniform, edge, count(edge), 0.05, 7);
            prog->free(prog);
            bes[bi]->free(bes[bi]);
        }
        umbra_flat_ir_free(ir);
    }

    return 0;
}
