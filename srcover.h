#pragma once
#include "umbra.h"

static inline struct umbra_basic_block* build_srcover(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32  ix     = umbra_lane(bb);
    umbra_i16  rgba[4];
    umbra_load_8x4(bb, (umbra_ptr){0}, ix, rgba);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);
    umbra_half sr     = umbra_mul_half(bb, umbra_half_from_i16(bb, rgba[0]), inv255),
               sg     = umbra_mul_half(bb, umbra_half_from_i16(bb, rgba[1]), inv255),
               sb     = umbra_mul_half(bb, umbra_half_from_i16(bb, rgba[2]), inv255),
               sa     = umbra_mul_half(bb, umbra_half_from_i16(bb, rgba[3]), inv255),
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
