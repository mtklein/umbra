Analytic filled SDF text — investigation notes
===============================================

Status: open.  Five experimental slides left in `slides/sdf.c`.  Source of
truth for what they render is the `.build` function names; titles below
are for cross-reference.

Problem
-------

The original `slide_sdf_text` renders "g" (was "Hamburgefons") by taking
the unsigned distance from a pixel to the nearest point on a flattened
polyline of the glyph and subtracting 1.5 — producing a **stroked
outline** of the glyph with a 3-pixel-wide band around the contour.  We
want the **fill** instead.

The outline → fill change is conceptually two separate pieces:

1. **Signed distance.**  Need a sign on the distance — negative inside,
   positive outside — so the existing SDF→coverage adapter's
   `clamp(-f, 0, 1)` ramp lights up the interior instead of a tube
   around the boundary.
2. **Accurate quadratic distance.**  `slug_extract` produces true
   quadratic Beziers (P0, P1, P2 per curve).  The original code ignores
   P1 and treats each curve as a line P0→P2, which looks OK for a
   thick stroke but is visibly wrong once we're trying to draw a fill
   whose boundary is the actual curve.

Both pieces need umbra IR that works inside `sdf_text_build` (called by
`umbra_sdf_draw` with interval x,y for the bounds scan and with exact
x,y for the per-pixel coverage scan).

Slides in the sequence (source order in `slides/sdf.c`)
-------------------------------------------------------

1. `slide_sdf_text_cp` — **CP markers overlay** on top of the original
   stroked fill.  Blue 1px-radius hard-edge dots at every on-curve
   point (P0 and P2 of each curve).  Magenta 1px dots at every
   off-curve control point (P1).  This was added as a debugging aid
   so we can visually correlate rendering artifacts with control
   point positions.  Build functions: `sdf_text_build` (stroke),
   `sdf_cp_markers_build` (point markers).
2. `slide_sdf_text` — unchanged original, stroked (distance − 1.5),
   parity-based sign (never reached in the stroke path, but the code
   still has the old sign logic).  Build: `sdf_text_build`.
3. `slide_sdf_text_signed` — first fill attempt: Newton distance (3
   iters from seed t=0.5 only), **signed winding sum** for sign,
   conservative bounds path (`{-dist.hi, +dist.hi}`).  Build:
   `sdf_text_signed_build`.
4. `slide_sdf_text_signed_multi` — same as 3 but **multi-seed Newton**
   from t ∈ {0, 0.5, 1}, each seed iterated 3 times, final distance
   is the min of |B(t) − pixel|² over the three converged t values.
   Build: `sdf_text_signed_multi_build`.
5. `slide_sdf_text_signed_multi_interior` — same as 4 but seeds are
   t ∈ {0.15, 0.5, 0.85} (interior only, avoiding exact endpoints).
   Build: `sdf_text_signed_multi_interior_build`.
6. `slide_sdf_text_signed_multi_safe` — same as 4 but Newton's step
   is guarded against near-zero derivative.  If `|f'(t)| < 1/65536`,
   we skip the update (`t` stays) so NaN/Inf from `0/0` can't poison
   the min.  Build: `sdf_text_signed_multi_safe_build`.

What we observed
----------------

| Slide | Distance | Sign        | Blue notches (P0/P2) | Magenta notches (P1) |
| ----- | -------- | ----------- | -------------------- | -------------------- |
| 2     | line seg | parity      | present              | present              |
| 3     | Newton×1 | signed wind | **fixed**            | present              |
| 4     | Newton×3 | signed wind | present              | **fixed**            |
| 5     | interior | signed wind | (got worse)          | —                    |
| 6     | × + guard | signed wind | present              | **fixed** (same as 4)|

Notch pattern: one small dark dip per control point.  Blue (P0/P2)
notches sit exactly on the vertex.  Magenta (P1) notches sit wherever
P1 projects onto the curve (nearest point on curve to P1).

Slide 3 → 4 swap is what's most striking: swapping single-seed
Newton for multi-seed fixes magenta but re-introduces blue.  Same
signed-winding code in both.  Guarding the Newton divide (slide 6)
made no difference, so NaN propagation isn't the mechanism.

What we've ruled out
--------------------

- **Parity vs signed winding as a distinct variable:** signed winding
  alone (with single-seed Newton, slide 3) fixes P0/P2 completely.
- **NaN/Inf from bad divides in Newton:** guarded version (slide 6)
  is visually identical to slide 4.
- **Endpoint-distance candidates in the old single-seed code:**
  removing `min(d2i, d20, d21)` and keeping only `d2i` made no visible
  change (experiment done early, pre-signed-winding).
- **Interior Newton seeds away from t=0/1:** slide 5 has *many new*
  notches, so the exact endpoint seeds in slide 4 are doing real
  work — specifically, they catch cases where the true closest-point
  is at or near a curve endpoint.

What we haven't ruled out (ordered by hypothesis strength)
----------------------------------------------------------

1. **Multi-seed is more accurate, and the asymmetric AA ramp makes
   that visible as notches.**  `sdf_as_coverage_build` uses
   `clamp(-f, 0, 1)`, which ramps coverage from 0 (at the boundary)
   to 1 (one font unit inside).  Anything inside-but-close-to-boundary
   renders at partial coverage.  If slide 3's single-seed Newton
   over-estimated distance at vertex-adjacent pixels (landing above
   the saturation point, cov=1) and slide 4's multi-seed correctly
   identifies them as close (cov<1), we'd see exactly this pattern of
   "extra" notches at every vertex — but slide 4 would be *right*.
   Testing this means widening the AA ramp (symmetric `clamp(0.5-f, 0, 1)`
   or `clamp(0.5 - 0.5*f, 0, 1)`) in a sixth slide and checking if
   the notches disappear.
2. **C⁰ kinks in the flattened contour for the specific 'g' data.**
   5 of 29 vertices in Arial 'g' are non-tangent-continuous (plus 2
   subpath boundaries; see analysis below).  Smooth C¹ vertices
   produce a smooth distance field; C⁰ kinks produce a "pinch" —
   genuine low-coverage cone at the vertex.  Can't be the *whole*
   story because notches appear at every vertex (not just the kinks),
   but probably contributes at the 5 kinks.
3. **Subpath boundaries wrap the loop incorrectly.**  The loop treats
   the curve buffer as a single closed cycle, so the "next curve"
   after the last curve of the outer contour is the first curve of
   the inner counter.  `slug_extract` doesn't mark subpath breaks in
   any way.  Winding contributions from this wrap-around are probably
   wrong at those two positions.  Separate bug from the notch
   question.

Kink analysis for 'g'
---------------------

Scriptlet at `/tmp/gkinks.c` (not saved in-tree; recreate if needed).
Loads Arial, extracts quadratic beziers for 'g', and for each vertex V
computes the sine of the angle between `V − P1_A` and `P1_B − V`.
C¹ continuity requires these to be parallel (sin ≈ 0).

Results (100px font height):

- 29 total curves.
- 22 smooth vertices (`|sin(angle)| < 0.02`).
- 7 kinks:
  - v00 (12.11, 86.01): sin = −0.96, tail attachment.
  - v05 (35.80, 74.96): sin = +0.65, ear junction.
  - v12 (36.54, 40.21): sin = +0.63, tail hook.
  - v13 (36.54, 34.62): sin = −1.00, 90° corner.
  - v14 (43.79, 34.62): sin = −1.00, 90° corner.
  - v20 (4.46, 84.88): subpath boundary (`joint_err` = 28.6).
  - v28 (10.97, 56.99): subpath boundary.

If you need to re-run this analysis, paste `/tmp/gkinks.c` (the
self-contained stb_truetype-only version).  Here's the full source:

```c
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb/stb_truetype.h"
#pragma clang diagnostic pop

int main(void) {
    FILE *fp = fopen("/System/Library/Fonts/Supplemental/Arial.ttf", "rb");
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    unsigned char *fd = malloc((size_t)sz);
    fread(fd, 1, (size_t)sz, fp); fclose(fp);

    stbtt_fontinfo font;
    stbtt_InitFont(&font, fd, stbtt_GetFontOffsetForIndex(fd, 0));
    float scale = stbtt_ScaleForPixelHeight(&font, 100.0f);
    int asc, des, gap; stbtt_GetFontVMetrics(&font, &asc, &des, &gap);
    float baseline = (float)asc * scale;

    float data[1024]; int count = 0;
    stbtt_vertex *v; int nv = stbtt_GetCodepointShape(&font, 'g', &v);
    float px = 0, py = 0;
    for (int j = 0; j < nv; j++) {
        float vx = (float)v[j].x * scale;
        float vy = baseline - (float)v[j].y * scale;
        if      (v[j].type == STBTT_vmove) { px = vx; py = vy; }
        else if (v[j].type == STBTT_vline) {
            float *d = data + count*6;
            d[0]=px; d[1]=py;
            d[2]=(px+vx)*0.5f; d[3]=(py+vy)*0.5f;
            d[4]=vx; d[5]=vy; count++;
            px = vx; py = vy;
        } else {
            float kx = (float)v[j].cx * scale;
            float ky = baseline - (float)v[j].cy * scale;
            float *d = data + count*6;
            d[0]=px; d[1]=py; d[2]=kx; d[3]=ky; d[4]=vx; d[5]=vy; count++;
            px = vx; py = vy;
        }
    }
    stbtt_FreeShape(&font, v);

    for (int i = 0; i < count; i++) {
        float const *cur = data + i*6;
        float const *nxt = data + ((i+1)%count)*6;
        float vx = cur[4], vy = cur[5];
        float vax = cur[2] - vx, vay = cur[3] - vy;
        float vbx = nxt[2] - vx, vby = nxt[3] - vy;
        float cross = vax*vby - vay*vbx;
        float la = sqrtf(vax*vax + vay*vay);
        float lb = sqrtf(vbx*vbx + vby*vby);
        float sin_angle = (la > 0 && lb > 0) ? cross/(la*lb) : 0;
        float jerr = sqrtf((cur[4]-nxt[0])*(cur[4]-nxt[0]) +
                           (cur[5]-nxt[1])*(cur[5]-nxt[1]));
        printf("v%02d sin=%+.4f joint_err=%.3f %s\n",
               i, (double)sin_angle, (double)jerr,
               fabsf(sin_angle) > 0.02f ? "KINK" : "smooth");
    }
    free(fd);
    return 0;
}
```

Key code fragments
------------------

**Newton step with guard (slide 6)** — pattern we should keep if we do
any more Newton iteration work:

```c
umbra_val32 const t       = umbra_load_var32(b, t_var),
                  tt      = umbra_mul_f32(b, t, t),
                  ft      = /* cubic */,
                  fp      = /* derivative */,
                  abs_fp  = umbra_abs_f32(b, fp),
                  fp_big  = umbra_lt_f32(b, n_eps, abs_fp),
                  safe_fp = umbra_sel_32(b, fp_big, fp, one),
                  raw_st  = umbra_div_f32(b, ft, safe_fp),
                  step    = umbra_sel_32(b, fp_big, raw_st, zero),
                  nt      = umbra_sub_f32(b, t, step),
                  ct      = umbra_min_f32(b, one, umbra_max_f32(b, zero, nt));
umbra_store_var32(b, t_var, ct);
```

**Signed winding contribution** — handles horizontal tangents (dy/dt=0)
correctly by contributing 0 rather than flipping parity:

```c
umbra_val32 const dydt    = umbra_add_f32(b, by,
                                umbra_mul_f32(b, two_ay, root)),
                  up      = umbra_lt_f32(b, zero, dydt),
                  down    = umbra_lt_f32(b, dydt, zero),
                  sign    = umbra_add_i32(b,
                                umbra_sel_32(b, up,   pos1_i, zero_i),
                                umbra_sel_32(b, down, neg1_i, zero_i)),
                  contrib = umbra_sel_32(b, crossing_mask, sign, zero_i);
```

**Linear fallback in the winding quadratic solver** — `ay=0` is common
because `slug_extract` stores line segments as degenerate quadratics
with P1 = midpoint of P0-P2, so `ay = p0y - 2·p1y + p2y = 0`.  Without
the fallback, `1/(2·ay) = Inf` poisons the roots:

```c
umbra_val32 const abs_ay   = umbra_abs_f32(b, ay),
                  is_quad  = umbra_lt_f32(b, eps, abs_ay),
                  abs_by   = umbra_abs_f32(b, by),
                  by_big   = umbra_lt_f32(b, eps, abs_by);
umbra_val32 const safe_2ay = umbra_sel_32(b, is_quad,
                                 umbra_mul_f32(b, two, ay), one),
                  inv2ay   = umbra_div_f32(b, one, safe_2ay),
                  /* quadratic roots qt0, qt1 use inv2ay */;
umbra_val32 const safe_by  = umbra_sel_32(b, by_big, by, one),
                  lt_root  = umbra_div_f32(b,
                                 umbra_sub_f32(b, zero, cypy), safe_by);
umbra_val32 const r0        = umbra_sel_32(b, is_quad, qt0, lt_root),
                  r1        = qt1,
                  r0_valid  = umbra_sel_32(b, is_quad, disc_ok, by_big),
                  r1_valid  = umbra_and_32(b, is_quad, disc_ok);
```

Other loose ends we hit along the way
-------------------------------------

- **MAX_SLIDES.** Was `32` in `slides/slides.c`; `slides_init` registers
  all `SLIDE()` factories *plus* one for `make_overview`, so the cap
  before the array overflows is implicitly `MAX_SLIDES − 1`.  With the
  new slides we hit the cap and saw a crash with `_platform_strlen`
  dereferencing a pointer made of ASCII bytes.  Bumped `MAX_SLIDES` to
  `32768` to stop worrying about it.
- **fp16-only cross-backend divergence.**  My fill implementation had a
  separate issue where `fmt=fp16` showed a 1-ULP, 1–2 byte per-backend
  diff (interp/jit/vulkan agreed; metal/wgpu agreed; cross-group
  disagreed).  Root cause not pinned down — we observed that our
  `metal.msl` dumps and MoltenVK's `vulkan.msl` dumps both emit the
  same count of `fma()` calls (grep -c lies; grep -o counts correctly),
  but differ in qualifiers: we leave `min/max` unqualified (resolves to
  `metal::`), MoltenVK emits `fast::min`/`fast::max`.  User confirmed
  that our Metal backend does no FMA fusion decisions — all fusion is
  in `src/builder.c`'s `umbra_add_f32` which auto-fuses `mul+add` into
  `op_fma_f32`.  So the IR going to all backends has the same fma ops;
  the divergence must be in op semantics.  Investigation stopped short
  of identifying the culprit.  The whole question became academic once
  we realized all backends render the slide visibly *wrong* — backend
  agreement is moot when the shared answer is incorrect.

Next steps, in order of most-likely-to-be-illuminating
------------------------------------------------------

1. **Widen the AA ramp in a new slide.**  Copy slide 4 but use a
   symmetric or wider `clamp` in a custom coverage adapter (bypass
   `sdf_as_coverage_build`, or write a variant that accepts an
   `umbra_sdf *` directly).  If the notches disappear with the wider
   ramp, slide 4's distance field is correct and the asymmetric ramp
   was just exposing real sub-pixel proximity as darkness.
2. **Plot the actual signed-distance values** at a handful of
   vertex-adjacent pixels in both slide 3 and slide 4.  Dump to
   stderr from inside an init function.  This would give us hard
   numbers on whether slide 4's distance is smaller (theory correct)
   or whether something else is going on.
3. **Fix the subpath-boundary wrap bug.**  Store or detect
   `MoveTo`-induced breaks and don't treat the last-curve-of-a-subpath
   and first-curve-of-the-next-subpath as adjacent.  Orthogonal to the
   notch question but a real bug.
4. **If (1) and (2) establish the notches are genuine AA of a correct
   distance field:** consider replacing `sdf_as_coverage_build`'s ramp
   with a centered-on-f=0 version for this specific slide, or for all
   SDF slides.  iv2d uses `coverage = clamp(0.5 − f, 0, 1)` which puts
   cov=0.5 at the boundary and has a 1-unit band centered on it — much
   less susceptible to these artifacts.

Open question we didn't dig into
--------------------------------

Is there a scheme that combines slide 3's P0/P2 immunity with slide
4's P1 correctness without having to widen the AA ramp?  Conceptually:
we want multi-seed Newton's accuracy for P1-area pixels but
single-seed's "fuzzing" at vertices.  One idea that never got tried:
compute per-seed distance, and weight the final min by something that
excludes the endpoint seeds when their result is suspiciously close to
`|V − pixel|` (where V is the curve's endpoint).  Awkward; probably
not the right tree.
