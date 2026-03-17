#pragma once
#include "umbra.h"

static inline struct umbra_builder* build_srcover(void) {
    struct umbra_builder *bb = umbra_builder();
    umbra_val  ix     = umbra_lane(bb);
    umbra_val  rgba[4];
    umbra_load_u8x4(bb, (umbra_ptr){0}, ix, rgba);
    umbra_val inv255 = umbra_imm_f32(bb, 1.0f/255.0f);
    umbra_val sr     = umbra_mul_f32(bb, umbra_cvt_f32_i32(bb, rgba[0]), inv255),
              sg     = umbra_mul_f32(bb, umbra_cvt_f32_i32(bb, rgba[1]), inv255),
              sb     = umbra_mul_f32(bb, umbra_cvt_f32_i32(bb, rgba[2]), inv255),
              sa     = umbra_mul_f32(bb, umbra_cvt_f32_i32(bb, rgba[3]), inv255),
              dr     = umbra_load_f16(bb, (umbra_ptr){1}, ix),
              dg     = umbra_load_f16(bb, (umbra_ptr){2}, ix),
              db     = umbra_load_f16(bb, (umbra_ptr){3}, ix),
              da     = umbra_load_f16(bb, (umbra_ptr){4}, ix),
              one    = umbra_imm_f32(bb, 1.0f),
              inv_a  = umbra_sub_f32(bb, one, sa),
              rout   = umbra_add_f32(bb, sr, umbra_mul_f32(bb, dr, inv_a)),
              gout   = umbra_add_f32(bb, sg, umbra_mul_f32(bb, dg, inv_a)),
              bout   = umbra_add_f32(bb, sb, umbra_mul_f32(bb, db, inv_a)),
              aout   = umbra_add_f32(bb, sa, umbra_mul_f32(bb, da, inv_a));
    umbra_store_f16(bb, (umbra_ptr){1}, ix, rout);
    umbra_store_f16(bb, (umbra_ptr){2}, ix, gout);
    umbra_store_f16(bb, (umbra_ptr){3}, ix, bout);
    umbra_store_f16(bb, (umbra_ptr){4}, ix, aout);
    return bb;
}
