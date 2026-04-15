#pragma once

#include "../include/umbra.h"

// interval_program interprets umbra_flat_ir as a program of [lo,hi] intervals.
//
// The IR can read uniforms from (umbra_ptr){.ix=1},
// and it must store its result to (umbra_ptr){.ix=0}.

typedef struct {
    float lo, hi;
} interval;

_Bool interval_is_finite(interval);

// Returns NULL if the IR contains any ops we can't yet handle, or if it has
// no umbra_store_32() to (umbra_ptr){.ix=0} to serve as its output.
struct interval_program* interval_program(struct umbra_flat_ir const*);
void   interval_program_free(struct interval_program*);

interval interval_program_run(struct interval_program*, interval x, interval y
                                                      , float const *uniform, int uniforms);
