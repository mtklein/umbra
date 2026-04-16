#pragma once
#include "umbra.h"

typedef struct {
    umbra_val32 lo, hi;
} umbra_interval;

static inline umbra_interval umbra_interval_exact(umbra_val32 v) {
    return (umbra_interval){v, v};
}
umbra_interval umbra_interval_add_f32 (struct umbra_builder*, umbra_interval, umbra_interval);
umbra_interval umbra_interval_sub_f32 (struct umbra_builder*, umbra_interval, umbra_interval);
umbra_interval umbra_interval_mul_f32 (struct umbra_builder*, umbra_interval, umbra_interval);
umbra_interval umbra_interval_div_f32 (struct umbra_builder*, umbra_interval, umbra_interval);
umbra_interval umbra_interval_min_f32 (struct umbra_builder*, umbra_interval, umbra_interval);
umbra_interval umbra_interval_max_f32 (struct umbra_builder*, umbra_interval, umbra_interval);
umbra_interval umbra_interval_sqrt_f32(struct umbra_builder*, umbra_interval);
umbra_interval umbra_interval_abs_f32 (struct umbra_builder*, umbra_interval);
