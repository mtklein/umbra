#include "../include/umbra_interval.h"
#include <math.h>

static _Bool exact(umbra_interval a) { return a.lo.id == a.hi.id; }

umbra_interval umbra_interval_exact(umbra_val32 v) {
    return (umbra_interval){v, v};
}

umbra_interval umbra_interval_add_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        return umbra_interval_exact(umbra_add_f32(b, a.lo, c.lo));
    }
    return (umbra_interval){umbra_add_f32(b, a.lo, c.lo),
                            umbra_add_f32(b, a.hi, c.hi)};
}

umbra_interval umbra_interval_sub_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        return umbra_interval_exact(umbra_sub_f32(b, a.lo, c.lo));
    }
    return (umbra_interval){umbra_sub_f32(b, a.lo, c.hi),
                            umbra_sub_f32(b, a.hi, c.lo)};
}

umbra_interval umbra_interval_mul_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        return umbra_interval_exact(umbra_mul_f32(b, a.lo, c.lo));
    }
    if (a.lo.id == c.lo.id && a.hi.id == c.hi.id) {
        umbra_val32 const zero        = umbra_imm_f32(b, 0.0f),
                          ll          = umbra_mul_f32(b, a.lo, a.lo),
                          hh          = umbra_mul_f32(b, a.hi, a.hi),
                          pos         = umbra_lt_f32(b, zero, a.lo),
                          neg         = umbra_lt_f32(b, a.hi, zero),
                          consistent  = umbra_or_32 (b, pos, neg),
                          tight_lo    = umbra_min_f32(b, ll, hh);
        return (umbra_interval){umbra_sel_32(b, consistent, tight_lo, zero),
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

umbra_interval umbra_interval_fma_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c, umbra_interval d) {
    if (exact(a) && exact(c) && exact(d)) {
        return umbra_interval_exact(umbra_fma_f32(b, a.lo, c.lo, d.lo));
    }
    return umbra_interval_add_f32(b, umbra_interval_mul_f32(b, a, c), d);
}

umbra_interval umbra_interval_fms_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c, umbra_interval d) {
    if (exact(a) && exact(c) && exact(d)) {
        return umbra_interval_exact(umbra_fms_f32(b, a.lo, c.lo, d.lo));
    }
    return umbra_interval_sub_f32(b, d, umbra_interval_mul_f32(b, a, c));
}

// If c straddles or touches zero the true set {a/x : x in c} is unbounded,
// so we collapse to (-inf, +inf) rather than the misleadingly narrow
// {1/c.hi, 1/c.lo} recip that only holds when c is sign-consistent.
// Downstream ops are not hardened against NaN from later 0*inf; consumers
// must tolerate infinite bounds propagating through.
umbra_interval umbra_interval_div_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        return umbra_interval_exact(umbra_div_f32(b, a.lo, c.lo));
    }
    umbra_val32 const zero    = umbra_imm_f32(b, 0.0f),
                      one     = umbra_imm_f32(b, 1.0f),
                      neg_inf = umbra_imm_f32(b, -INFINITY),
                      pos_inf = umbra_imm_f32(b,  INFINITY),
                      lo_pos  = umbra_lt_f32 (b, zero, c.lo),
                      hi_neg  = umbra_lt_f32 (b, c.hi, zero),
                      nonstr  = umbra_or_32  (b, lo_pos, hi_neg);
    umbra_interval const recip = {umbra_div_f32(b, one, c.hi),
                                  umbra_div_f32(b, one, c.lo)};
    umbra_interval const m = umbra_interval_mul_f32(b, a, recip);
    return (umbra_interval){umbra_sel_32(b, nonstr, m.lo, neg_inf),
                            umbra_sel_32(b, nonstr, m.hi, pos_inf)};
}

umbra_interval umbra_interval_min_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        return umbra_interval_exact(umbra_min_f32(b, a.lo, c.lo));
    }
    return (umbra_interval){umbra_min_f32(b, a.lo, c.lo),
                            umbra_min_f32(b, a.hi, c.hi)};
}

umbra_interval umbra_interval_max_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        return umbra_interval_exact(umbra_max_f32(b, a.lo, c.lo));
    }
    return (umbra_interval){umbra_max_f32(b, a.lo, c.lo),
                            umbra_max_f32(b, a.hi, c.hi)};
}

umbra_interval umbra_interval_sqrt_f32(struct umbra_builder *b, umbra_interval a) {
    if (exact(a)) {
        return umbra_interval_exact(umbra_sqrt_f32(b, a.lo));
    }
    return (umbra_interval){umbra_sqrt_f32(b, a.lo),
                            umbra_sqrt_f32(b, a.hi)};
}

umbra_interval umbra_interval_abs_f32(struct umbra_builder *b, umbra_interval a) {
    if (exact(a)) {
        return umbra_interval_exact(umbra_abs_f32(b, a.lo));
    }
    umbra_val32 const zero   = umbra_imm_f32(b, 0.0f),
                      abs_lo = umbra_abs_f32(b, a.lo),
                      abs_hi = umbra_abs_f32(b, a.hi),
                      lo_pos = umbra_lt_f32 (b, zero, a.lo),
                      hi_neg = umbra_lt_f32 (b, a.hi, zero),
                      nonstr = umbra_or_32  (b, lo_pos, hi_neg),
                      tight  = umbra_min_f32(b, abs_lo, abs_hi),
                      new_lo = umbra_sel_32 (b, nonstr, tight, zero),
                      new_hi = umbra_max_f32(b, abs_lo, abs_hi);
    return (umbra_interval){new_lo, new_hi};
}
