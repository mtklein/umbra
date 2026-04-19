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

// TODO: unsound when c straddles zero.  The {1/c.hi, 1/c.lo} recip here
// assumes c is sign-consistent (all positive or all negative); if c.lo < 0 <
// c.hi the true set {a/x : x in c} is unbounded, e.g. [2,3] / [-1,1] =
// (-inf, -2] U [2, +inf), but this code produces a narrow finite interval
// like [-3, 3] that does NOT enclose the truth.  Downstream this causes
// unsound tile culling in umbra_sdf_draw under perspective transforms (see
// TODO near umbra_sdf_draw in draw.c): a horizon-crossing tile can have its
// f.lo come out positive when the true f.lo is deeply negative, and the
// `f.lo < 0` predicate drops a tile that should have drawn -> visible holes.
//
// Two candidate fixes once we actually need perspective-sdf support:
//
//  (1) Return (-inf, +inf) from this op when c straddles zero, AND plug the
//      NaN leak in umbra_interval_mul_f32 so infinities survive downstream:
//      0 * (+-inf) = NaN in IEEE, and NaN < 0 = false would silently break
//      soundness again.  Fix by appending sel(isnan(lo), -inf, lo) and
//      sel(isnan(hi), +inf, hi) after every interval op (or at least mul and
//      sqrt).  Cheap since these ops run in the bounds program once per tile,
//      not per pixel.  Makes the library self-consistent against any future
//      NaN source, not just this one.
//
//  (2) Clamp at the divide: route the zero-straddle case through a saturating
//      div returning +-FLT_MAX sentinels.  Avoids NaN without touching mul.
//      Leaks precision though -- FLT_MAX * small is finite and misleadingly
//      tight, whereas inf * small is inf and stays honest.
//
// Prefer (1); (2) is a local hack.  Either way, update umbra_sdf_draw's
// perspective-sniff fallback (see draw.c) once this lands so we cull tiles
// instead of full-rect dispatch.
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
