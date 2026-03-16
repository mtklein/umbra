#pragma once
#include "umbra.h"

static inline struct umbra_basic_block* build_srcover(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32  ix     = umbra_lane(bb);
    umbra_v32  rgba[4];
    umbra_load8x4(bb, (umbra_ptr){0}, ix, rgba);
    umbra_v32 inv255 = umbra_fimm(bb, 1.0f/255.0f);
    umbra_v32 sr     = umbra_fmul(bb, umbra_itof(bb, rgba[0]), inv255),
              sg     = umbra_fmul(bb, umbra_itof(bb, rgba[1]), inv255),
              sb     = umbra_fmul(bb, umbra_itof(bb, rgba[2]), inv255),
              sa     = umbra_fmul(bb, umbra_itof(bb, rgba[3]), inv255),
              dr     = umbra_load_f16(bb, (umbra_ptr){1}, ix),
              dg     = umbra_load_f16(bb, (umbra_ptr){2}, ix),
              db     = umbra_load_f16(bb, (umbra_ptr){3}, ix),
              da     = umbra_load_f16(bb, (umbra_ptr){4}, ix),
              one    = umbra_fimm(bb, 1.0f),
              inv_a  = umbra_fsub(bb, one, sa),
              rout   = umbra_fadd(bb, sr, umbra_fmul(bb, dr, inv_a)),
              gout   = umbra_fadd(bb, sg, umbra_fmul(bb, dg, inv_a)),
              bout   = umbra_fadd(bb, sb, umbra_fmul(bb, db, inv_a)),
              aout   = umbra_fadd(bb, sa, umbra_fmul(bb, da, inv_a));
    umbra_store_f16(bb, (umbra_ptr){1}, ix, rout);
    umbra_store_f16(bb, (umbra_ptr){2}, ix, gout);
    umbra_store_f16(bb, (umbra_ptr){3}, ix, bout);
    umbra_store_f16(bb, (umbra_ptr){4}, ix, aout);
    return bb;
}
