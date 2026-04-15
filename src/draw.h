#pragma once

// Full definition of struct umbra_draw, the compiled bundle that the public
// include/umbra_draw.h declares only by its constructor's return type.
// Internal to the repo: src/draw.c populates it, tests may #include this
// header to assert on its fields.

#include "../include/umbra.h"
#include "interval.h"

struct umbra_draw {
    struct umbra_program    *partial_coverage;
    struct umbra_program    *full_coverage;
    struct interval_program *coverage;
    int                      uniform_offset;  // slot in buf[0] where coverage uniforms start
    int                      pad_;
};
