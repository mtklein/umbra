#include "../include/umbra_interval.h"

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

// TODO: unsound on zero-straddling divisors.  The {1/c.hi, 1/c.lo} recip
// here assumes c is sign-consistent (all positive or all negative).  If
// c.lo < 0 < c.hi the true set {a/x : x in c} is unbounded -- for example
// [2, 3] / [-1, 1] = (-inf, -2] U [2, +inf) -- but this op returns a narrow
// finite interval like [-3, 3] that does NOT enclose the truth.  Any caller
// that could end up here with a zero-straddling divisor gets silently wrong
// bounds.
//
// No consumer today needs it: the SDF bounds program is affine-only
// (the struct umbra_affine path in src/draw.c emits no divide), and the other
// interval ops don't introduce division.  But the op is part
// of our public interval library, so figure out the right behavior before
// a caller shows up that would be hurt.  Candidates: return (-inf, +inf)
// and harden every other interval op against NaN from later 0*inf; or a
// saturating +-FLT_MAX at the divide; or simply make it a precondition
// that the divisor be sign-consistent.
umbra_interval umbra_interval_div_f32(struct umbra_builder *b,
                                      umbra_interval a, umbra_interval c) {
    if (exact(a) && exact(c)) {
        return umbra_interval_exact(umbra_div_f32(b, a.lo, c.lo));
    }
    umbra_interval const recip = {umbra_div_f32(b, umbra_imm_f32(b, 1.0f), c.hi),
                                  umbra_div_f32(b, umbra_imm_f32(b, 1.0f), c.lo)};
    return umbra_interval_mul_f32(b, a, recip);
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
