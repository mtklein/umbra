#pragma once
#include "umbra.h"

static inline struct umbra_builder* build_srcover(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val ix = umbra_lane(b);

    umbra_val px = umbra_load_i32(b, (umbra_ptr){0}, ix),
            mask = umbra_imm_i32(b, 0xFF);
    umbra_val rgba[4] = {
        umbra_and_i32(b, px, mask),
        umbra_and_i32(b,
            umbra_shr_u32(b, px, umbra_imm_i32(b, 8)),
            mask),
        umbra_and_i32(b,
            umbra_shr_u32(b, px, umbra_imm_i32(b,16)),
            mask),
        umbra_shr_u32(b, px, umbra_imm_i32(b, 24)),
    };

    umbra_val inv255 = umbra_imm_f32(b, 1.0f/255.0f);
    umbra_val sr = umbra_mul_f32(b,
                       umbra_cvt_f32_i32(b, rgba[0]),
                       inv255),
              sg = umbra_mul_f32(b,
                       umbra_cvt_f32_i32(b, rgba[1]),
                       inv255),
              sb = umbra_mul_f32(b,
                       umbra_cvt_f32_i32(b, rgba[2]),
                       inv255),
              sa = umbra_mul_f32(b,
                       umbra_cvt_f32_i32(b, rgba[3]),
                       inv255),
              dr = umbra_widen_f16(b,
                       umbra_load_i16(b, (umbra_ptr){1},
                                      ix)),
              dg = umbra_widen_f16(b,
                       umbra_load_i16(b, (umbra_ptr){2},
                                      ix)),
              db = umbra_widen_f16(b,
                       umbra_load_i16(b, (umbra_ptr){3},
                                      ix)),
              da = umbra_widen_f16(b,
                       umbra_load_i16(b, (umbra_ptr){4},
                                      ix)),
              one   = umbra_imm_f32(b, 1.0f),
              inv_a = umbra_sub_f32(b, one, sa),
              rout  = umbra_add_f32(b, sr,
                          umbra_mul_f32(b, dr, inv_a)),
              gout  = umbra_add_f32(b, sg,
                          umbra_mul_f32(b, dg, inv_a)),
              bout  = umbra_add_f32(b, sb,
                          umbra_mul_f32(b, db, inv_a)),
              aout  = umbra_add_f32(b, sa,
                          umbra_mul_f32(b, da, inv_a));
    umbra_store_i16(b, (umbra_ptr){1}, ix,
                    umbra_narrow_f32(b, rout));
    umbra_store_i16(b, (umbra_ptr){2}, ix,
                    umbra_narrow_f32(b, gout));
    umbra_store_i16(b, (umbra_ptr){3}, ix,
                    umbra_narrow_f32(b, bout));
    umbra_store_i16(b, (umbra_ptr){4}, ix,
                    umbra_narrow_f32(b, aout));
    return b;
}
