#pragma once
#include "umbra.h"

static inline struct umbra_basic_block* build_srcover(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32  ix     = umbra_lane(bb),
               src    = umbra_load_32(bb, (umbra_ptr){0}, ix),
               mask8  = umbra_imm_32(bb, 0xff);
    umbra_half inv255 = umbra_imm_half(bb, 0x1C04);
    umbra_v32  ri     = umbra_and_32(bb, src, mask8);
    umbra_half sr     = umbra_mul_half(bb, umbra_half_from_i32(bb, ri), inv255);
    umbra_v32  sh8    = umbra_imm_32(bb, 8),
               gi     = umbra_and_32(bb, umbra_shr_u32(bb, src, sh8), mask8);
    umbra_half sg     = umbra_mul_half(bb, umbra_half_from_i32(bb, gi), inv255);
    umbra_v32  sh16   = umbra_imm_32(bb, 16),
               bi     = umbra_and_32(bb, umbra_shr_u32(bb, src, sh16), mask8);
    umbra_half sb     = umbra_mul_half(bb, umbra_half_from_i32(bb, bi), inv255);
    umbra_v32  sh24   = umbra_imm_32(bb, 24),
               ai     = umbra_and_32(bb, umbra_shr_u32(bb, src, sh24), mask8);
    umbra_half sa     = umbra_mul_half(bb, umbra_half_from_i32(bb, ai), inv255),
               dr     = umbra_load_half(bb, (umbra_ptr){1}, ix),
               dg     = umbra_load_half(bb, (umbra_ptr){2}, ix),
               db     = umbra_load_half(bb, (umbra_ptr){3}, ix),
               da     = umbra_load_half(bb, (umbra_ptr){4}, ix),
               one    = umbra_imm_half(bb, 0x3c00),
               inv_a  = umbra_sub_half(bb, one, sa),
               rout   = umbra_add_half(bb, sr, umbra_mul_half(bb, dr, inv_a)),
               gout   = umbra_add_half(bb, sg, umbra_mul_half(bb, dg, inv_a)),
               bout   = umbra_add_half(bb, sb, umbra_mul_half(bb, db, inv_a)),
               aout   = umbra_add_half(bb, sa, umbra_mul_half(bb, da, inv_a));
    umbra_store_half(bb, (umbra_ptr){1}, ix, rout);
    umbra_store_half(bb, (umbra_ptr){2}, ix, gout);
    umbra_store_half(bb, (umbra_ptr){3}, ix, bout);
    umbra_store_half(bb, (umbra_ptr){4}, ix, aout);
    return bb;
}
