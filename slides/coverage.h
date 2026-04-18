#pragma once
#include "../include/umbra_draw.h"

// 8-bit and SDF bitmap coverage, ctx is an umbra_buf*.  Samples at the
// dispatch pixel (x, y), 1:1 mapping between pixels and bitmap cells.
umbra_val32 coverage_bitmap(void *umbra_buf, struct umbra_builder*,
                            umbra_val32 x, umbra_val32 y);
umbra_val32 coverage_sdf   (void *umbra_buf, struct umbra_builder*,
                            umbra_val32 x, umbra_val32 y);

// Gather-based 8-bit bitmap coverage of fixed dimensions.  Unlike
// coverage_bitmap(), samples at arbitrary (x, y) coordinates -- pair with
// coverage_matrix() or similar transform combinator.  Returns 0 for samples
// outside [0, w) x [0, h).  w and h are baked at IR-build time; mutate them
// by rebuilding the program.  Pixels (.buf.ptr) mutate freely.
struct coverage_bitmap2d {
    struct umbra_buf buf;
    int              w, h;
};
umbra_val32 coverage_bitmap2d(void *coverage_bitmap2d, struct umbra_builder*,
                              umbra_val32 x, umbra_val32 y);

// Matrix-transform combinator: applies a 3x3 perspective transform to (x, y)
// and forwards to an inner coverage.  mat mutates freely between dispatches;
// inner_fn / inner_ctx are baked at IR-build time (same shape as
// umbra_supersample).
struct coverage_matrix {
    struct umbra_matrix  mat;
    int                  :32;
    umbra_coverage      *inner_fn;
    void                *inner_ctx;
};
umbra_val32 coverage_matrix(void *coverage_matrix, struct umbra_builder*,
                            umbra_val32 x, umbra_val32 y);
