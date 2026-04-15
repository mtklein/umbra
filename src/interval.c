#include "assume.h"
#include "flat_ir.h"
#include "interval.h"
#include <math.h>
#include <stdlib.h>

// Self-contained snapshot: we copy the IR's instruction array in so callers
// can free the umbra_flat_ir (and the umbra_builder it came from) as soon as
// interval_program() returns.
struct interval_program {
    struct ir_inst *inst;
    interval       *v;
    int             insts, pad_;
};

static interval const MAXIMAL = {-INFINITY, +INFINITY};
static interval const EXACT_0 = {0,0};
static interval const EXACT_1 = {1,1};
static interval const MAYBE   = {0,1};

static interval interval_exact(float f) {
    return (interval){f, f};
}

_Bool interval_is_finite(interval i) {
    return isfinite(i.lo) && isfinite(i.hi);
}

static float fmin4(float a, float b, float c, float d) {
    float const ab = a < b ? a : b,
                cd = c < d ? c : d;
    return ab < cd ? ab : cd;
}
static float fmax4(float a, float b, float c, float d) {
    float const ab = a > b ? a : b,
                cd = c > d ? c : d;
    return ab > cd ? ab : cd;
}

static interval interval_add(interval a, interval b) {
    return (interval){a.lo + b.lo, a.hi + b.hi};
}
static interval interval_sub(interval a, interval b) {
    return (interval){a.lo - b.hi, a.hi - b.lo};
}
static interval interval_mul(interval a, interval b) {
    float const ll = a.lo * b.lo,
                lh = a.lo * b.hi,
                hl = a.hi * b.lo,
                hh = a.hi * b.hi;
    return (interval){fmin4(ll, lh, hl, hh), fmax4(ll, lh, hl, hh)};
}
// TODO: strict interval rounding.
//
// Every f32 op above uses round-to-nearest, which can shrink endpoints by up
// to ~0.5 ulp per op and is technically unsound for rigorous bound proofs.
// A correct implementation rounds lo toward -∞ and hi toward +∞ so the
// computed interval always envelops the true real-valued one.  Unlikely to
// bite us at dispatch-pruning scale, but here's how we'd land it when it
// becomes worthwhile — written with an eye toward eventually compiling
// interval_program to the GPU backends.
//
// APPROACHES CONSIDERED:
//
// A. fesetround(FE_DOWNWARD / FE_UPWARD).  Global rounding-mode switch.
//    Portable (C99), precise.  Unsuitable because:
//    - global mutable state → thread-unsafe without care
//    - compilers aggressively reorder across fesetround unless
//      `#pragma STDC FENV_ACCESS ON` is respected (most ignore it)
//    - GPU shader cores have fixed round-to-nearest; no runtime switch
//    - wasm has no portable fesetround
//
// B. nextafterf outward inflation (RECOMMENDED).  After each rounded op,
//    bump lo toward -∞ and hi toward +∞ by one ulp.  Over-conservative
//    by 1 ulp per op (true answer may have been exact), but:
//    - portable across CPU/GPU/wasm with no global state
//    - works in shaders as a few bit-twiddle ops on the float's u32
//      representation (bits+1 moves up, bits-1 moves down, with sign and
//      ±INF special-cased)
//    - accumulated slop over ~20 ops in a typical SDF is ~20 ulps, still
//      invisible at dispatch scale
//    - libm's nextafterf is slow; we'd want a bit-manipulation fast path
//      (~5 instructions vs libm's ~20 with special-case branches)
//
// C. AVX-512 per-op rounding-mode encoding.  Encoded in the instruction
//    itself (VRNDSCALEPS, VFMA with rm-bits).  Not applicable — our
//    x86 target is AVX2.  ARM Neon has no equivalent.
//
// D. SPV_KHR_shader_float_controls + FP rounding mode decoration.
//    Would handle SPIRV (Vulkan, wgpu).  Metal has no equivalent — it
//    would need approach B anyway.  Not worth half-coverage.
//
// E. F64 intermediate + final F32 cast.  Exact within F64, one rounding.
//    Rejected: GPU F64 support is patchy (not required in Vulkan/Metal),
//    and doubles memory traffic on CPU.
//
// PLAN (when we pick this up):
//
// 1. Add `static float next_down(float)` / `next_up(float)` bit-twiddle
//    helpers in interval.c.  Special cases: NaN → NaN, +∞ → +∞ (for
//    next_up), -∞ → -∞ (for next_down), 0 → ±FLT_TRUE_MIN.
//
// 2. Wrap the rounding-sensitive helpers (interval_add, interval_sub,
//    interval_mul, interval_div, interval_sqrt path, interval_fma,
//    interval_fms, interval_square, interval_square_add, interval_square_sub)
//    with outward inflation: `return (interval){next_down(lo), next_up(hi)};`
//    Skip helpers that don't introduce rounding: interval_abs,
//    interval_min/max (selection only), interval_lt (mask output),
//    interval_monotone(floor/ceil) on integer-valued ops.
//
// 3. Test: beef up interval_circle_sdf_contains_samples with a wider grid
//    (currently 9×9 per box; bump to e.g. 64×64) and/or add a targeted
//    test that constructs a case where round-to-nearest shrinks the
//    interval below a truth sample, proving the slop and the fix.
//
// 4. For GPU: when we add a compile_to_gpu path for interval_program, the
//    shader emission uses approach B directly — emit each op followed by
//    the 2-line `bits ± 1` next_down/next_up gadgets.  No runtime rounding
//    mode required; works on every API (Metal/Vulkan/wgpu) with the same
//    code shape.
//
// COMPLICATIONS TO HANDLE:
//
// - Preserve NaN propagation.  next_up(NaN) must be NaN (libm does this;
//    our bit-twiddle path needs to check isnan first).
// - Don't inflate past ±∞.  next_up(+INF) = +INF, not FLT_MAX.
// - Subnormals are fine with bit-twiddle but 2-3× slower on some CPUs;
//    for dispatcher-level scale it doesn't matter.
// - Backends don't need changes until we actually run interval_program on
//    the GPU — this pass still executes on CPU today.  The research is
//    for making sure the CPU-side choice doesn't box us in later.

static interval interval_div(interval a, interval b) {
    if (b.lo <= 0.0f && 0.0f <= b.hi) {
        return MAXIMAL;
    }
    interval const recip = {1.0f / b.hi, 1.0f / b.lo};
    return interval_mul(a, recip);
}
static interval interval_min(interval a, interval b) {
    return (interval){a.lo < b.lo ? a.lo : b.lo,
                      a.hi < b.hi ? a.hi : b.hi};
}
static interval interval_max(interval a, interval b) {
    return (interval){a.lo > b.lo ? a.lo : b.lo,
                      a.hi > b.hi ? a.hi : b.hi};
}
static interval interval_abs(interval a) {
    if (a.lo >= 0.0f) { return a; }
    if (a.hi <= 0.0f) { return (interval){-a.hi, -a.lo}; }
    float const m = -a.lo > a.hi ? -a.lo : a.hi;
    return (interval){0.0f, m};
}
// Tight bound for x*x.  The whole reason op_square_f32 exists: interval_mul
// treats x*x as two independent copies of x and loses the correlation, giving
// [-a², a²] for x ∈ [-a, a] instead of the tight [0, max(xl², xh²)].
static interval interval_square(interval a) {
    float const ll = a.lo * a.lo,
                hh = a.hi * a.hi;
    if (a.lo >= 0.0f) { return (interval){ll, hh}; }
    if (a.hi <= 0.0f) { return (interval){hh, ll}; }
    return (interval){0.0f, ll > hh ? ll : hh};
}
// sqrt is monotone on its domain; genuinely-negative inputs produce NaN via
// sqrtf and poison the interval, which interval_is_finite() catches.  We
// used to clamp the input to [0, ∞) because interval_mul(x, x) could dip
// slightly negative from rounding slop on `x*x + y*y` — interval_square
// kills that source, so the clamp is no longer pulling weight.  NaN
// propagation is safer than clamping anyway: NaN is contagious, whereas a
// ±INF from MAXIMAL can be "un-poisoned" by a downstream min/max/clamp.
static interval interval_monotone(interval a, float (*f)(float)) {
    return (interval){f(a.lo), f(a.hi)};
}

// fma_f32(x,y,z) = x*y + z with a single rounding — matches the hardware
// fmaf the backends emit per pixel, so the bound doesn't drift from what
// the program actually computes.  fmaf is monotone in z, so min over the
// box is at z.lo and max at z.hi; within that we enumerate all 4 (x,y)
// corners since the sign of the x-derivative depends on y and vice versa.
static interval interval_fma(interval x, interval y, interval z) {
    float const lo_ll = fmaf(x.lo, y.lo, z.lo),
                lo_lh = fmaf(x.lo, y.hi, z.lo),
                lo_hl = fmaf(x.hi, y.lo, z.lo),
                lo_hh = fmaf(x.hi, y.hi, z.lo);
    float const hi_ll = fmaf(x.lo, y.lo, z.hi),
                hi_lh = fmaf(x.lo, y.hi, z.hi),
                hi_hl = fmaf(x.hi, y.lo, z.hi),
                hi_hh = fmaf(x.hi, y.hi, z.hi);
    return (interval){fmin4(lo_ll, lo_lh, lo_hl, lo_hh),
                      fmax4(hi_ll, hi_lh, hi_hl, hi_hh)};
}
// fms_f32(x,y,z) = z - x*y, matching the interpreter's vec_fma(-x, y, z).
static interval interval_fms(interval x, interval y, interval z) {
    float const lo_ll = fmaf(-x.lo, y.lo, z.lo),
                lo_lh = fmaf(-x.lo, y.hi, z.lo),
                lo_hl = fmaf(-x.hi, y.lo, z.lo),
                lo_hh = fmaf(-x.hi, y.hi, z.lo);
    float const hi_ll = fmaf(-x.lo, y.lo, z.hi),
                hi_lh = fmaf(-x.lo, y.hi, z.hi),
                hi_hl = fmaf(-x.hi, y.lo, z.hi),
                hi_hh = fmaf(-x.hi, y.hi, z.hi);
    return (interval){fmin4(lo_ll, lo_lh, lo_hl, lo_hh),
                      fmax4(hi_ll, hi_lh, hi_hl, hi_hh)};
}
// square_add_f32(x, y) = x*x + y with a single rounding — matches fmaf(x,x,y).
// Uses the tight x*x logic: for fixed y, the function's minimum in x is at
// x=0 (if 0 lies in x's interval) or at the |x|-smaller endpoint.
static interval interval_square_add(interval x, interval y) {
    float const ll_lo = fmaf(x.lo, x.lo, y.lo),
                hh_lo = fmaf(x.hi, x.hi, y.lo);
    float const ll_hi = fmaf(x.lo, x.lo, y.hi),
                hh_hi = fmaf(x.hi, x.hi, y.hi);
    float       lo    = ll_lo < hh_lo ? ll_lo : hh_lo;
    float const hi    = ll_hi > hh_hi ? ll_hi : hh_hi;
    // x straddles 0 → min of x*x is 0, attained at x=0; fmaf(0,0,y.lo) = y.lo.
    if (x.lo <= 0.0f && 0.0f <= x.hi) { lo = y.lo; }
    return (interval){lo, hi};
}
// square_sub_f32(x, y) = y - x*x with a single rounding — matches fmaf(-x,x,y).
static interval interval_square_sub(interval x, interval y) {
    float const ll_lo = fmaf(-x.lo, x.lo, y.lo),
                hh_lo = fmaf(-x.hi, x.hi, y.lo);
    float const ll_hi = fmaf(-x.lo, x.lo, y.hi),
                hh_hi = fmaf(-x.hi, x.hi, y.hi);
    float const lo    = ll_lo < hh_lo ? ll_lo : hh_lo;
    float       hi    = ll_hi > hh_hi ? ll_hi : hh_hi;
    // x straddles 0 → max of y - x*x is y (at x=0); fmaf(0,0,y.hi) = y.hi.
    if (x.lo <= 0.0f && 0.0f <= x.hi) { hi = y.hi; }
    return (interval){lo, hi};
}

// Tri-valued compare result: {0,0} (definitely false), {1,1} (definitely
// true), or {0,1} (maybe).
static interval interval_lt(interval a, interval b) {
    if (a.hi <  b.lo) { return EXACT_1; }
    if (a.lo >= b.hi) { return EXACT_0; }
    return MAYBE;
}

// Convention: uniforms are read from umbra_ptr32{.ix=1}, and the output is
// umbra_store_32()'d to umbra_ptr32{.ix=0}.  Using distinct pointers for
// inputs and outputs keeps them obvious at a glance.
enum {
    SINK_PTR_BITS    = 0,
    UNIFORM_PTR_BITS = 1,
};

// Every op this pass knows how to bound.  Any IR op not in this set, any
// uniform read from a pointer other than UNIFORM_PTR_BITS, or any store to a
// pointer other than SINK_PTR_BITS, makes interval_program() return NULL, so
// run() can assume every op is supported and every store targets SINK.
static _Bool op_supported(struct ir_inst const *in) {
    switch ((int)in->op) {
        case op_x: case op_y: case op_join:
        case op_imm_32:

        case op_add_f32: case op_sub_f32: case op_mul_f32: case op_div_f32:
        case op_min_f32: case op_max_f32: case op_fma_f32: case op_fms_f32:
        case op_square_f32: case op_square_add_f32: case op_square_sub_f32:
        case op_sqrt_f32: case op_abs_f32:
        case op_floor_f32: case op_ceil_f32:

        case op_lt_f32:

        case op_add_f32_imm: case op_sub_f32_imm:
        case op_div_f32_imm: case op_lt_f32_imm:
            return 1;
        case op_uniform_32: return in->ptr.bits == UNIFORM_PTR_BITS;
        case op_store_32:   return in->ptr.bits == SINK_PTR_BITS;
        default:
            return 0;
    }
}

struct interval_program* interval_program(struct umbra_flat_ir const *ir) {
    _Bool has_sink_store = 0;
    for (int i = 0; i < ir->insts; i++) {
        struct ir_inst const *in = &ir->inst[i];
        if (!op_supported(in)) {
            return NULL;
        }
        if (in->op == op_store_32) {
            has_sink_store = 1;
        }
    }
    if (!has_sink_store) {
        return NULL;
    }
    size_t const bytes = (size_t)ir->insts * sizeof *ir->inst;

    struct interval_program *p = calloc(1, sizeof *p);
    p->insts = ir->insts;
    p->inst  = malloc(bytes);
    p->v     = malloc((size_t)ir->insts * sizeof *p->v);
    __builtin_memcpy(p->inst, ir->inst, bytes);
    return p;
}

void interval_program_free(struct interval_program *p) {
    if (p) {
        free(p->inst);
        free(p->v);
        free(p);
    }
}

static interval arg(struct interval_program const *p, val v) {
    return p->v[v.id];
}

interval interval_program_run(struct interval_program *p,
                              interval x, interval y,
                              float const *uniform) {
    interval last_store = MAXIMAL;

    for (int i = 0; i < p->insts; i++) {
        struct ir_inst const *in = &p->inst[i];
        interval r = MAXIMAL;

        switch ((int)in->op) {
            case op_x:    r = x; break;
            case op_y:    r = y; break;
            case op_join: r = arg(p, in->x); break;

            case op_imm_32: {
                union { int i; float f; } const u = {.i = in->imm};
                r = interval_exact(u.f);
            } break;

            case op_uniform_32: r = interval_exact(uniform[in->imm]); break;

            case op_add_f32: r = interval_add(arg(p,in->x), arg(p,in->y)); break;
            case op_sub_f32: r = interval_sub(arg(p,in->x), arg(p,in->y)); break;
            case op_mul_f32: r = interval_mul(arg(p,in->x), arg(p,in->y)); break;
            case op_div_f32: r = interval_div(arg(p,in->x), arg(p,in->y)); break;
            case op_min_f32: r = interval_min(arg(p,in->x), arg(p,in->y)); break;
            case op_max_f32: r = interval_max(arg(p,in->x), arg(p,in->y)); break;

            case op_fma_f32: r = interval_fma(arg(p,in->x), arg(p,in->y), arg(p,in->z)); break;
            case op_fms_f32: r = interval_fms(arg(p,in->x), arg(p,in->y), arg(p,in->z)); break;

            case op_square_add_f32: r = interval_square_add(arg(p,in->x), arg(p,in->y)); break;
            case op_square_sub_f32: r = interval_square_sub(arg(p,in->x), arg(p,in->y)); break;

            case op_sqrt_f32:   r = interval_monotone(arg(p,in->x), sqrtf);  break;
            case op_abs_f32:    r = interval_abs     (arg(p,in->x));         break;
            case op_square_f32: r = interval_square  (arg(p,in->x));         break;
            case op_floor_f32:  r = interval_monotone(arg(p,in->x), floorf); break;
            case op_ceil_f32:   r = interval_monotone(arg(p,in->x), ceilf);  break;

            case op_lt_f32: r = interval_lt(arg(p,in->x), arg(p,in->y)); break;

            // f32 _imm ops: x-side is a value, y-side is an f32 immediate.
            case op_add_f32_imm:
            case op_sub_f32_imm:
            case op_div_f32_imm:
            case op_lt_f32_imm: {
                union { int i; float f; } const u = {.i = in->imm};
                interval const a = arg(p,in->x),
                               b = interval_exact(u.f);
                switch ((int)in->op) {
                    case op_add_f32_imm: r = interval_add(a, b); break;
                    case op_sub_f32_imm: r = interval_sub(a, b); break;
                    case op_div_f32_imm: r = interval_div(a, b); break;
                    case op_lt_f32_imm:  r = interval_lt (a, b); break;
                    default: __builtin_unreachable();
                }
            } break;

            case op_store_32:
                // interval_program() only admits stores to SINK.
                last_store = arg(p, in->y);
                // store_32 produces no value; r stays MAXIMAL.
                break;

            // interval_program() should have returned NULL before we
            // reached an unsupported op.
            default: __builtin_unreachable();
        }
        p->v[i] = r;
    }
    return last_store;
}
