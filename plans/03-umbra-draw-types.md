# Refactor plan: umbra_draw.h CPU/builder type naming

## Convention

Canonical CPU-side type names have **no suffix**. The builder-side sibling
(fields of `umbra_val32`, etc.) is the qualified one, tagged with the `val`
size it holds. Rationale: the point of naming a type is to centralize the
representation, so the default is the unqualified name. The IR-building
variant is the unusual case and wears the marker.

    typedef struct {       float r,g,b,a; } umbra_color;
    typedef struct { umbra_val32 r,g,b,a; } umbra_color_val32;

Types that have no CPU parallel (and no plan for one) stay unsuffixed:
`umbra_interval`, `umbra_matrix`, `umbra_bitmap`, `umbra_buf`. The suffix
appears only when a CPU sibling exists.

## New types

  - `umbra_color` — `{ float r,g,b,a; }`
  - `umbra_color_val32` — `{ umbra_val32 r,g,b,a; }` (current `umbra_color`)
  - `umbra_rect`  — `{ float l,t,r,b; }`
  - `umbra_point` — `{ float x,y; }`

Deferred (only if a concrete callsite wants them):
  - `umbra_point_val32`, `umbra_color_val16`, `umbra_rect_val32`
  - `struct umbra_tile { int l,t,r,b; }` for integer dispatch rects —
    separate concept from float `umbra_rect`; leave `int l,t,r,b` dispatch
    args alone for now.

## Commit plan

Prefer one easily-explained change per commit.  Glom together only if the
change reads as one idea.  If any commit turns tricky (unexpected ABI
issues, ambiguous callsite migration, cross-cutting uniform-layout
surprises) fall back to a smaller split.  `ninja` passes twice after
every commit.

### 1. rename umbra_color → umbra_color_val32

Pure mechanical rename across src/, include/, tests/, tools/, slides/:
typedef, `umbra_blend_fn`, all `umbra_blend_*`, `struct umbra_fmt::load`
and `::store` function pointers, all `umbra_load_*` / `umbra_store_*`
format helpers, `struct umbra_shader::build` return type, and every
callsite.  No behavior change.  Frees the name `umbra_color`.

### 2. introduce umbra_color CPU type; switch solid + LUT-helper APIs

Add `typedef struct { float r,g,b,a; } umbra_color;` to umbra_draw.h.

Switch these public APIs:

  - `umbra_shader_solid(float const color[4])` → `umbra_shader_solid(umbra_color)`
    - struct field `float color[4]` → `umbra_color color`
  - `umbra_gradient_lut_even(…, float const colors[][4])`
    → `umbra_gradient_lut_even(…, umbra_color const *colors)`
  - `umbra_gradient_lut(…, float const colors[][4])`
    → `umbra_gradient_lut(…, umbra_color const *colors)`

Update all callers (tests/draw_test.c, tests/golden_test.c, slides/*,
tools/*) in the same commit.  `(float[]){r,g,b,a}` literals become
`(umbra_color){r,g,b,a}`.

### 3. introduce umbra_rect; switch coverage_rect + sdf_rect

Add `typedef struct { float l,t,r,b; } umbra_rect;`.

  - `umbra_coverage_rect(float const rect[4])` → `umbra_coverage_rect(umbra_rect)`
  - `umbra_sdf_rect(float const rect[4])`       → `umbra_sdf_rect(umbra_rect)`
  - struct fields `float rect[4]` → `umbra_rect rect`

Update callers.

### 4. introduce umbra_point (no callers yet)

Pure add: `typedef struct { float x,y; } umbra_point;`.  The gradient
reshapes below immediately consume it.  Separate commit because "add a
type" is a complete, revertable idea on its own.

### 5. reshape linear_2 / radial_2 APIs

Current:
  - `umbra_shader_linear_2(float const grad[3], float const color[8])`
  - `umbra_shader_radial_2(float const grad[3], float const color[8])`

Replace opaque `grad[3]` with geometry-named inputs, and split `color[8]`
into two stop colors:

  - `umbra_shader_linear_2(umbra_point p0, umbra_point p1, umbra_color c0, umbra_color c1)`
  - `umbra_shader_radial_2(umbra_point center, float radius, umbra_color c0, umbra_color c1)`

Derive `(a, b, c)` / `(cx, cy, inv_r)` coefficients at fill() time from
the natural inputs.  Struct fields change accordingly.

If the geometry derivation turns out to be subtler than expected (numeric
stability, matching previous coefficient results bit-exactly), split into
two commits: one per flavor.

### 6. reshape linear_grad / radial_grad APIs; drop N

Current:
  - `umbra_shader_linear_grad(float const grad[4], struct umbra_buf lut)`
  - `umbra_shader_radial_grad(float const grad[4], struct umbra_buf lut)`

The 4th `grad` slot is the LUT length N, which can be derived from
`lut.size` (4 floats per entry for rgba).  Drop it from the public API
and compute it in the constructor:

  - `umbra_shader_linear_grad(umbra_point p0, umbra_point p1, struct umbra_buf lut)`
  - `umbra_shader_radial_grad(umbra_point center, float radius, struct umbra_buf lut)`

Internal uniform layout keeps the 4th slot (feeds `sample_lut_`'s
`fi+3`); the constructor fills it from the computed N.

### 7. reshape linear_stops / radial_stops APIs; drop N

Current:
  - `umbra_shader_linear_stops(float const grad[4], struct umbra_buf colors, struct umbra_buf pos)`
  - `umbra_shader_radial_stops(float const grad[4], struct umbra_buf colors, struct umbra_buf pos)`

N is the stop count, derivable from `pos.size / sizeof(float)` (or
`colors.size / sizeof(umbra_color)`, whichever is canonical).  Same
treatment as commit 6:

  - `umbra_shader_linear_stops(umbra_point p0, umbra_point p1, struct umbra_buf colors, struct umbra_buf pos)`
  - `umbra_shader_radial_stops(umbra_point center, float radius, struct umbra_buf colors, struct umbra_buf pos)`

Also consider: if `colors` becomes a `umbra_color`-typed buf-of-struct
rather than a plane-of-floats, `walk_stops_`'s per-channel gather math
changes significantly.  That's out of scope for this commit — keep the
internal SoA layout and only touch the public signature.  Flag as a
follow-up TODO.

## Risks / watch-outs

  - Struct-literal construction from designated initializers is ABI-
    identical to a `float[N]` on every target we care about, but
    **passing by value** changes the ABI vs pointer-to-array.  All call
    sites and impls update in one commit per API.
  - `__builtin_memcpy`s sized by `sizeof(color)` / 16 / 32 in src/draw.c
    need auditing when switching types.
  - Geometry derivation in commits 5–7: verify existing tests still pass
    bit-exactly; if they don't, investigate whether the new formula is
    more or less accurate before accepting drift.
  - `walk_stops_` and `sample_lut_` read N as a float uniform at `fi+3`.
    After commits 6–7 the public signature loses N but the internal
    uniform slot stays — don't simultaneously refactor the internals.
  - Keep `umbra_matrix`, `umbra_bitmap`, `umbra_interval` unchanged.

## Out of scope

TODOs at lines 51, 54, 82, 98, 99, 105, 120, 135, 198, 211, 243 in
include/umbra_draw.h (uniforms-layout folding, opaque polymorphism,
hard_edge → quality, sdf_dispatch → sdf_draw rename, trailing underscore
cleanup, gradient shader ensemble rename, etc.) are separate refactors.
This plan touches only: CPU/builder type split, the four new types, and
the gradient API reshape that lets those types do useful work.
