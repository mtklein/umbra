#pragma once
#include "../include/umbra_draw.h"

// 8-bit and SDF bitmap coverage, ctx is an umbra_buf*.
umbra_val32 coverage_bitmap(void *umbra_buf, struct umbra_builder*,
                            umbra_val32 x, umbra_val32 y);
umbra_val32 coverage_sdf   (void *umbra_buf, struct umbra_builder*,
                            umbra_val32 x, umbra_val32 y);
