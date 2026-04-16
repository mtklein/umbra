#include "../include/umbra_interval.h"

static _Bool exact(umbra_interval a) { return a.lo.id == a.hi.id; }

// TODO: use umbra_interval_exact() to build the {r, r} returns in the exact
// peepholes below, and in umbra_interval_uniform().  Currently each writes
// (umbra_interval){r, r} or {val, val} inline.  Calling umbra_interval_exact()
// would require passing the float value rather than the umbra_val32, so this
// may need a new helper like umbra_interval_from_val32(umbra_val32 v) that
// returns (umbra_interval){v, v}.

umbra_interval umbra_interval_exact(struct umbra_builder *b, float v) {
    umbra_val32 const val = umbra_imm_f32(b, v);
    return (umbra_interval){val, val};
}

umbra_interval umbra_interval_uniform(struct umbra_builder *b,
                                      umbra_ptr32 ptr, int slot) {
    umbra_val32 const val = umbra_uniform_32(b, ptr, slot);
    return (umbra_interval){val, val};
}

umbra_interval umbra_interval_add_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        umbra_val32 const r = umbra_add_f32(b, a.lo, c.lo);
        return (umbra_interval){r, r};
    }
    return (umbra_interval){umbra_add_f32(b, a.lo, c.lo),
                            umbra_add_f32(b, a.hi, c.hi)};
}

umbra_interval umbra_interval_sub_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        umbra_val32 const r = umbra_sub_f32(b, a.lo, c.lo);
        return (umbra_interval){r, r};
    }
    return (umbra_interval){umbra_sub_f32(b, a.lo, c.hi),
                            umbra_sub_f32(b, a.hi, c.lo)};
}

umbra_interval umbra_interval_mul_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        umbra_val32 const r = umbra_mul_f32(b, a.lo, c.lo);
        return (umbra_interval){r, r};
    }
    if (a.lo.id == c.lo.id && a.hi.id == c.hi.id) {
        umbra_val32 const ll = umbra_mul_f32(b, a.lo, a.lo),
                          hh = umbra_mul_f32(b, a.hi, a.hi);
        return (umbra_interval){umbra_imm_f32(b, 0.0f),
                                umbra_max_f32(b, ll, hh)};
    }
    umbra_val32 const ll = umbra_mul_f32(b, a.lo, c.lo),
                      lh = umbra_mul_f32(b, a.lo, c.hi),
                      hl = umbra_mul_f32(b, a.hi, c.lo),
                      hh = umbra_mul_f32(b, a.hi, c.hi);
    return (umbra_interval){
        umbra_min_f32(b, umbra_min_f32(b, ll, lh), umbra_min_f32(b, hl, hh)),
        umbra_max_f32(b, umbra_max_f32(b, ll, lh), umbra_max_f32(b, hl, hh)),
    };
}

umbra_interval umbra_interval_div_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        umbra_val32 const r = umbra_div_f32(b, a.lo, c.lo);
        return (umbra_interval){r, r};
    }
    umbra_interval const recip = {umbra_div_f32(b, umbra_imm_f32(b, 1.0f), c.hi),
                                  umbra_div_f32(b, umbra_imm_f32(b, 1.0f), c.lo)};
    return umbra_interval_mul_f32(b, a, recip);
}

umbra_interval umbra_interval_min_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        umbra_val32 const r = umbra_min_f32(b, a.lo, c.lo);
        return (umbra_interval){r, r};
    }
    return (umbra_interval){umbra_min_f32(b, a.lo, c.lo),
                            umbra_min_f32(b, a.hi, c.hi)};
}

umbra_interval umbra_interval_max_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        umbra_val32 const r = umbra_max_f32(b, a.lo, c.lo);
        return (umbra_interval){r, r};
    }
    return (umbra_interval){umbra_max_f32(b, a.lo, c.lo),
                            umbra_max_f32(b, a.hi, c.hi)};
}

umbra_interval umbra_interval_sqrt_f32(struct umbra_builder *b, umbra_interval a) {
    if (exact(a)) {
        umbra_val32 const r = umbra_sqrt_f32(b, a.lo);
        return (umbra_interval){r, r};
    }
    return (umbra_interval){umbra_sqrt_f32(b, a.lo),
                            umbra_sqrt_f32(b, a.hi)};
}

umbra_interval umbra_interval_abs_f32(struct umbra_builder *b, umbra_interval a) {
    if (exact(a)) {
        umbra_val32 const r = umbra_abs_f32(b, a.lo);
        return (umbra_interval){r, r};
    }
    umbra_val32 const zero   = umbra_imm_f32(b, 0.0f),
                      neg_lo = umbra_sub_f32(b, zero, a.lo);
    return (umbra_interval){zero, umbra_max_f32(b, neg_lo, a.hi)};
}
