# Refactor plan: one uniform buffer per effect

## Motivation

Today every effect (shader, coverage, sdf) shares a single wide uniforms
buffer (`buf[0]`).  At build time each effect reserves a range of slots
in `struct umbra_uniforms_layout` and records the offset as mutable state
on its own instance (`color_off`, `rect_off`, `coeffs_off`, ...).  At
fill time the effect writes its data into the shared blob at those
offsets.

Two hazards fell out of the recent opaque-polymorphism refactor:

1. **Fragile coincidence.**  Slides and tests that recreate a concrete
   effect between frames (e.g. to update a rect or color) end up with
   fresh zero-initialized offset fields.  These currently *happen* to
   match the compiled program because the affected effect was the first
   reservation (slot 0).  Any change in reservation order silently
   corrupts memory.  Callsites: [slides/anim.c:47](slides/anim.c:47),
   [slides/swatch.c:72](slides/swatch.c:72),
   [tests/draw_test.c:731](tests/draw_test.c:731).

2. **Per-frame throwaway builder.**  [slides/persp.c:62](slides/persp.c:62)
   and [slides/solid.c:74](slides/solid.c:74) work around (1) by running
   a whole `umbra_draw_builder(...)` every frame — allocating a builder,
   building an entire IR, and throwing it away — purely to re-populate
   the two offset fields.

The root cause: offsets live on the *instance* and are populated as a
side effect of `build()`.  Recreate the instance, lose the offsets.

## The target shape

Each effect owns its own small uniform buffer:

    buf[0]  = render target (or whatever the draw writes to)
    buf[1]  = effect 1's uniforms  (typically just the concrete struct)
    buf[2]  = effect 2's uniforms
    ...

At build time the driving code tells each effect "you're buffer index N"
and the effect emits IR that reads from `buf[N]` at struct-field-fixed
offsets.

At dispatch time the caller passes `{.ptr = &effect, .count = ...}` as
`buf[N]`.

The important property: the *layout* of buf[N] is part of the effect's
type, fixed at compile time.  There is no runtime reservation and no
`fill` step.  Recreating an effect struct can't desync because offsets
aren't state.

## What goes away

- `struct umbra_uniforms_layout` (at least the slot-counting parts)
- `struct umbra_draw_layout` (or shrinks to almost nothing)
- `umbra_uniforms_alloc`, `umbra_uniforms_fill_f32`, `umbra_uniforms_fill_i32`,
  `umbra_uniforms_fill_ptr`
- `umbra_uniforms_reserve_f32`, `umbra_uniforms_reserve_ptr`
- `struct umbra_shader::fill`, `struct umbra_coverage::fill`,
  `struct umbra_sdf::fill`
- The `color_off`, `rect_off`, `coeffs_off`, `colors_off`, `pos_off`,
  `lut_off`, `bmp_off`, `mat_off`, `winding_off`, `hard_edge` offset
  fields on every concrete effect (most disappear; any remaining
  runtime data like `hard_edge` stays as a struct field, reached via
  `buf[N]` like any other uniform).

## What stays the same

- `struct umbra_buf` — still the dispatch unit.
- `op_uniform_32` / `op_deref_ptr` / `umbra_ptr32`'s `.ix` channel.
- Backend dispatch interface `queue(prog, l,t,r,b, buf[])`.
- CSE / hashing over `op_uniform_32` — remains correct since distinct
  buffer indices produce distinct `umbra_ptr32` bits.

## Assumptions and optimizations we must unwind carefully

### JIT (x86 and arm64)

- `XBUF` register holds the base of `buf[]`.  Loads today are
  `mov [XBUF + slot*4]` with the buffer index baked in as 0.  For
  higher indices they're `mov [XBUF + buf_idx*sizeof(umbra_buf) + offset_to_ptr]
  → base`, then `mov [base + slot*4]`.  The indirection already exists for
  non-zero ix (used by derived-ptr deref).  We need the non-zero-ix path
  to be as efficient as today's ix=0 fast path.
- Currently the "flat top-level buf at ix=0" is treated as read-only and
  short; the JIT may load individual slots with a short-disp addressing
  mode.  With many small bufs that's still fine per-buf, but the code
  gen needs to confirm it doesn't assume a single flat range.
- **Check**: `jit_x86.c`'s `resolve_ptr_x86` and `jit_arm64.c`'s
  equivalent — are they already general over `.ix`? Looks like yes
  (`deref_gpr`, `last_ptr`), but the ix==0 case may have a constant-fold
  path to verify.

### Interpreter

- `RESOLVE_PTR(inst)` returns either the static `ptr.bits` (= buffer
  index + flags) or a dynamically-derived slot.  `op_uniform_32`'s
  body is already `buf[ptr_ix].ptr[slot*4]` shaped; generalizing should
  be a no-op.  Confirm in `src/interpreter.c`.

### Vulkan / WGPU / Metal / SPIR-V

- Each currently treats `buf[0]` specially: read-only flat top-level goes
  through the per-frame `uniform_ring_pool`; writable or derived go
  through `gpu_buf_cache`.  The "flat read-only" heuristic is `!rw &
  BUF_WRITTEN && !stride` — that's already per-buf, not hard-coded to
  ix=0.  Multiple small buffers with those properties should all land in
  the uniform ring naturally.  Verify.
- **Cost to watch**: more small uniform-ring allocations per dispatch.
  If one shared blob was ~80 bytes, five small ones are each ~16 bytes —
  same total, but each is a separate ring alloc + descriptor binding.
  For GPU backends this may matter; measure with `bench`.
- SPIR-V: the compiled shader will have more descriptor set entries.
  Confirm we're under the descriptor limit (`max_ptr <= 32` is already
  enforced in `src/vulkan.c:282`).

### Uniform ring

- The `uniform_ring_pool` keys by `ptr` — the caller's uniform data
  pointer.  With N small bufs per dispatch we hit the pool N times
  instead of 1.  Pool internals look ring-friendly but verify alignment
  padding doesn't dominate for tiny (<32 byte) allocations.

### Dedup / CSE

- `op_uniform_32` with `.ptr = {.ix = 0, ...}, .imm = slot` is hashed
  and dedup'd.  With per-effect buffers, effect A's slot 0 and effect
  B's slot 0 are different `.ix` values, so they naturally don't collide.
  No action needed.

### sdf_dispatch grid uniforms

- `umbra_sdf_draw()`'s bounds program appends `base_x, base_y, tile_w,
  tile_h` to the tail of the uniforms buffer ([src/draw.c](src/draw.c)).
  Under the new model these become their own buffer too (or get folded
  into the dispatch struct).  Either way, same story: assign an index.

### umbra_deref_ptr

- Used for shader LUT / stops / bmp — the effect's own buf holds a
  `struct umbra_buf` descriptor, and the shader derefs it to read a
  bigger data buf.  Under the new model the effect struct directly
  contains the `struct umbra_buf` field; deref loads from effect's
  buf[N] at the field offset.  No change to the IR op.

## Commit plan

Pick your rung carefully — this touches IR emission, every backend, and
every effect's code. Proposed staging, each commit ninja-green:

### 1. Add a buffer-index parameter to the `build()` trampoline

Each effect's `build()` currently takes `(self, builder, layout, x, y)`.
Replace `layout` with an `int buf_index` saying which buffer it owns
(plus, temporarily, still the layout for backward compat during the
transition).  Concrete effects keep doing what they do today.  Callsites
pass buf_index=0 for all; nothing changes.  Pure interface threading.

### 2. Wire effects to read from their assigned buf_index

Each concrete effect's `build()` uses `(umbra_ptr32){.ix = buf_index}`
instead of `{.ix = 0}` for its uniform reads.  Still use the layout's
slot reservation — so internally nothing changes, but now the IR
references `buf[N]` with N != 0 correctly.

Test this by having `umbra_draw_builder` assign a distinct non-zero
buf_index to e.g. the coverage and confirm end-to-end still works.  (Can
even interleave bits of the layout between effects without breaking
anything.)

### 3. Eliminate the per-effect offset fields

Lock in each concrete effect's on-buf layout at compile time: `struct
shader_solid` lives in its own buf starting at offset 0; `.color.r` is
at slot 0 within the buf, etc.  Build emits IR reading `buf[N].slot[0]`
for r, `.slot[1]` for g, and so on — no `color_off` needed.

Drop the `color_off`/`coeffs_off`/etc. fields.  Drop the `build()`'s
reservation calls.  Drop `umbra_uniforms_reserve_*`.

### 4. Eliminate the fill() step

Now that effect data lives at fixed struct offsets inside its own buf,
the caller can pass `{.ptr = &effect, .count = sizeof(effect)/4}`
directly.  Drop the `fill` vtable entry.  Drop `umbra_uniforms_fill_*`.

Every slide and test updates its dispatch to include a `struct umbra_buf`
per effect.  The draw-builder records which buffer-index goes to which
effect so the caller knows the order.

### 5. Delete umbra_draw_layout and umbra_uniforms_layout

Now empty or trivially replaceable.  Confirm no residual references.
Drop `umbra_uniforms_alloc`.

### 6. Rename / rework remaining public API

`umbra_draw_builder`'s `struct umbra_draw_layout *layout` out-param goes
away.  The caller instead gets the buf-index assignment (as part of the
builder return or a separate out-param).

### 7. Tighten and measure

- `bench` before/after — expect small positive or neutral CPU, confirm
  GPU backends don't regress from extra descriptor bindings.
- Check `umbra_sdf_draw`'s grid uniforms also migrated.
- Verify the fragile-offset hazards are truly gone: scan for the
  old coincidence patterns in slides and confirm they now trivially
  handle per-frame updates.

## Natural struct layout, end to end

The IR is built per-target (each ninja config builds its own `src/`
from scratch, then the backend consumes that config's IR), so there's
no need to force a cross-target "canonical" uniform layout.  We can let
effect uniforms be plain C structs with natural alignment and padding,
and read them via `offsetof` everywhere.

Implications:

- Each effect's buf is just a pointer to the concrete struct.  Its size
  is `sizeof(struct effect)`.  Its fields live at `offsetof(effect,
  field)` — same on CPU and on the GPU side (backends compute GPU
  byte offsets from the same `offsetof`).
- `struct umbra_buf` fields inside an effect struct (LUTs, stop buffers,
  bitmap handles) use natural alignment: `{void* ptr; int count; int
  stride;}` is 16 bytes on 64-bit with ptr at 0, count at 8, stride at
  12; 12 bytes on 32-bit with ptr at 0, count at 4, stride at 8.  The
  IR references each field at its `offsetof` — no hardcoded slot-2 dance.
- Today's `umbra_uniforms_fill_ptr` bakes in a 64-bit layout even on
  32-bit targets (padding out to 16 bytes).  That contract goes away
  when `fill_ptr` goes away; the effect struct's `struct umbra_buf
  lut;` field is written with a plain struct assignment, and the IR
  references each sub-field at its `offsetof` within the effect.
- Keep the IR's 4-byte slot indexing.  Every uniform field in today's
  effect types is 4-byte aligned, so `slot = offsetof(struct, field) / 4`
  always divides evenly, and the op encoding stays unchanged.  The one
  place the IR currently hardcodes an internal layout is the deref
  read of `struct umbra_buf` (today: slot+0 for ptr, slot+2 for count,
  slot+3 for stride — the 64-bit layout).  That becomes slot+
  `offsetof(struct umbra_buf, count)/4` etc., which on 32-bit targets
  differs from today's.  Localized change in the IR emitter for
  `op_deref_ptr` and the backend decoders that match it.

Effect types that today have hand-packed layouts can just be C structs:

    struct shader_solid           { umbra_color color; };
    struct shader_gradient_linear_two_stops {
        umbra_point p0, p1;
        umbra_color c0, c1;
    };
    struct coverage_rect          { umbra_rect rect; };
    struct coverage_bitmap_matrix { struct umbra_matrix mat;
                                    struct umbra_bitmap bmp; };
    // etc.

All the offset fields (`color_off`, `coeffs_off`, ...) disappear; their
job is now `offsetof`.  The vtable shrinks to `build` + `free`.

## Risks to watch

- **Descriptor-limit**: GPU backends cap at 32 bindings.  A complex draw
  (shader + coverage + blend + multiple LUTs) must stay under.  Current
  `max_ptr <= 32` assertion already enforces this; the change is that
  effect uniforms now consume a binding slot each instead of sharing one.
- **GPU backend layout parity**: the backend runs on the host when
  generating shader code, so `offsetof` is host-native.  For GPU memory
  layouts (std430 or equivalent), the backend needs to emit loads at
  the same byte offsets the host would see.  This is already the
  arrangement today — backends already decode `struct umbra_buf` out of
  a CPU-written blob.  We just widen the scope: every field at its
  `offsetof`, no slot math.
- **Uniform ring overhead**: N small allocations vs. 1 big one.  If
  profiling shows this matters, add a per-dispatch coalescing step that
  concatenates all small uniforms into one ring chunk and emits multiple
  bindings into it at known offsets.

## Non-goals / out of scope

- Rewriting the effect types themselves.  We keep the `build()`
  interface, the opaque pointer API, and the user-definable escape
  hatch.  We're only changing how uniforms flow.
- Changing the ptr bitfield encoding or adding new ops.  The IR stays
  the same; we just use `.ix` for more than {0, derived} now.
- `umbra_sdf_draw`'s grid-uniform handling can be fixed in the same
  push (commit 3 or 4) but is a separate concern if it gets tricky.
