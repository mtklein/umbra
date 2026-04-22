#include "test.h"

#ifndef __wasm__

#include "../include/umbra.h"
#include "../include/umbra_draw.h"
#include "../include/umbra_interval.h"
#include <stdint.h>
#include <stdlib.h>

// Minimal CSG-style SDF with 6 uniforms, exactly the preamble shape that
// trips the ARM64 RA loop-invariant check once a cell_mat affine transform
// gets composed into the bounds program.
struct two_circle {
    float cx1, cy1, r1, cx2, cy2, r2;
};

static umbra_interval circle_sdf(struct umbra_builder *b,
                                 umbra_interval x, umbra_interval y,
                                 umbra_interval cx, umbra_interval cy,
                                 umbra_interval r) {
    umbra_interval const dx = umbra_interval_sub_f32(b, x, cx),
                         dy = umbra_interval_sub_f32(b, y, cy),
                         d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, dx, dx),
                                  umbra_interval_mul_f32(b, dy, dy)),
                         d  = umbra_interval_sqrt_f32(b, d2);
    return umbra_interval_sub_f32(b, d, r);
}

static umbra_interval two_circle_union(void *ctx, struct umbra_builder *b,
                                       umbra_interval x, umbra_interval y) {
    struct two_circle const *self = ctx;
    umbra_ptr const u = umbra_early_bind_uniforms(b, self, 6);
    umbra_interval const cx1 = umbra_interval_exact(umbra_uniform_32(b, u, 0)),
                         cy1 = umbra_interval_exact(umbra_uniform_32(b, u, 1)),
                         r1  = umbra_interval_exact(umbra_uniform_32(b, u, 2)),
                         cx2 = umbra_interval_exact(umbra_uniform_32(b, u, 3)),
                         cy2 = umbra_interval_exact(umbra_uniform_32(b, u, 4)),
                         r2  = umbra_interval_exact(umbra_uniform_32(b, u, 5));
    umbra_interval const a = circle_sdf(b, x, y, cx1, cy1, r1),
                         c = circle_sdf(b, x, y, cx2, cy2, r2);
    return umbra_interval_min_f32(b, a, c);
}

// Compile the CSG bounds IR with an affine transform composed in -- this is
// what umbra_sdf_dispatch/overview would produce per cell.  Under xsan the
// ARM64 JIT aborts at ra_assert_loop_invariant for this IR today.
TEST(test_jit_compiles_sdf_bounds_with_affine_transform) {
    struct umbra_backend *be = umbra_backend_jit();
    if (!be) { return; }  // non-JIT platform: nothing to reproduce

    struct umbra_affine cell_mat = {
        .sx = 5.0f, .kx = 0, .tx = -10.0f,
        .ky = 0,    .sy = 5.0f, .ty = -20.0f,
    };
    struct two_circle sdf = {100, 100, 50, 200, 200, 50};

    struct umbra_builder *b = umbra_builder();
    struct umbra_sdf_bounds_program *prog =
        umbra_sdf_bounds_program(b, &cell_mat, two_circle_union, &sdf);
    umbra_builder_free(b);

    prog != NULL here;

    umbra_sdf_bounds_program_free(prog);
    umbra_backend_free(be);
}

#endif
