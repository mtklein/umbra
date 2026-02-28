#pragma once
#include <stdint.h>

// ============================================================
// umbra VM
//
// A straight-line (single basic block) virtual machine designed
// to back onto vectorized CPU (SIMD) or GPU compute shaders.
//
// TYPE SYSTEM
//   Two value widths — semantics (float vs int) come from the operation:
//     vm_V32   32-bit value: f32 coordinates or i32 indices/masks
//     vm_V16   16-bit value: f16 color channels or i16 data
//     vm_Ptr   opaque pointer slot, bound to a real address at run time
//
//   Using struct wrappers prevents accidentally mixing widths or
//   passing a vm_Ptr where a value is expected.
//
// OPERATIONS
//   Suffix conventions:
//     f / f16    float arithmetic (f32 or f16)
//     i / i16    integer arithmetic (i32 or i16)
//     _32 / _16  memory width (load/store)
//
// MASKS
//   Comparisons return vm_V32 with all bits 0 (false) or all bits 1
//   (true), matching SIMD convention.  Mask logic reuses ordinary
//   integer bitwise ops (vm_andi, vm_ori, vm_noti).
//   vm_sel32 / vm_sel16 blend values using a mask.
//
// MEMORY
//   Five load/store shapes — all reference a vm_Ptr slot that is
//   bound to an actual base address only when vm_run() is called:
//     uniform_load    one load, result broadcast to all lanes        → uniform
//     varying_load    strided load, one element per lane             → varying
//     varying_gather  per-lane indexed load                          → varying
//     varying_store   strided store, one element per lane            → (sink)
//   No uniform stores; no varying scatters.
//
// UNIFORMITY
//   Automatically propagated: an op is uniform iff all its inputs are.
//   On CPU, uniform values become scalars; varying become SIMD vectors.
//   On GPU, uniform values become push constants; varying are per-thread.
// ============================================================

// ---- Opaque value handles -------------------------------------------------------

typedef struct { uint32_t id; } vm_V32;  // 32-bit value (f32 or i32)
typedef struct { uint32_t id; } vm_V16;  // 16-bit value (f16 or i16)
typedef struct { uint32_t id; } vm_Ptr;  // late-bound memory pointer slot

// ---- Opaque program handles -----------------------------------------------------

typedef struct vm_Program  vm_Program;
typedef struct vm_Compiled vm_Compiled;

// ---- Backend selection ----------------------------------------------------------

typedef enum {
    VM_BACKEND_INTERP,  // scalar reference interpreter, always available
    VM_BACKEND_CPU,     // SIMD JIT (AVX2/AVX-512 on x86, NEON on ARM)
    VM_BACKEND_SPIRV,   // SPIR-V compute shader for SDL_gpu / Vulkan
    VM_BACKEND_AUTO,    // pick best backend available at runtime
} vm_Backend;

// ============================================================
// Program construction
// ============================================================

vm_Program *vm_program(void);
void        vm_program_free(vm_Program *);

// Declare a new late-bound pointer slot.
vm_Ptr vm_ptr(vm_Program *);

// ---- Immediates (always uniform) ------------------------------------------------

vm_V32 vm_immf  (vm_Program *, float);    // f32 constant
vm_V32 vm_immi  (vm_Program *, int32_t);  // i32 constant
vm_V16 vm_immf16(vm_Program *, float);    // f16 constant  (specify as float)
vm_V16 vm_immi16(vm_Program *, int16_t);  // i16 constant

// ---- Built-in varying value -----------------------------------------------------

// Lane index: i32 value equal to the lane's position in the batch (0, 1, 2 … N-1).
vm_V32 vm_lane_id(vm_Program *);

// ---- Memory operations ----------------------------------------------------------

// _32 ops load/store 4 bytes; _16 ops load/store 2 bytes.
// Uniform: one load, result broadcast to all lanes.
// Varying: lane i accesses  base[ptr] + offset + i*stride.
// Gather:  lane i accesses  base[ptr] + byte_offsets[i]  (byte_offsets: varying i32).

vm_V32 vm_uniform_load32 (vm_Program *, vm_Ptr, int32_t offset);
vm_V16 vm_uniform_load16 (vm_Program *, vm_Ptr, int32_t offset);
vm_V32 vm_varying_load32 (vm_Program *, vm_Ptr, int32_t offset, int32_t stride);
vm_V16 vm_varying_load16 (vm_Program *, vm_Ptr, int32_t offset, int32_t stride);
vm_V32 vm_varying_gather32(vm_Program *, vm_Ptr, vm_V32 byte_offsets);
vm_V16 vm_varying_gather16(vm_Program *, vm_Ptr, vm_V32 byte_offsets);
void   vm_varying_store32 (vm_Program *, vm_Ptr, int32_t offset, int32_t stride, vm_V32);
void   vm_varying_store16 (vm_Program *, vm_Ptr, int32_t offset, int32_t stride, vm_V16);

// ---- Float 32-bit arithmetic ----------------------------------------------------

vm_V32 vm_addf  (vm_Program *, vm_V32, vm_V32);
vm_V32 vm_subf  (vm_Program *, vm_V32, vm_V32);
vm_V32 vm_mulf  (vm_Program *, vm_V32, vm_V32);
vm_V32 vm_divf  (vm_Program *, vm_V32, vm_V32);
vm_V32 vm_fmaf  (vm_Program *, vm_V32 a, vm_V32 b, vm_V32 c);  // a*b+c
vm_V32 vm_negf  (vm_Program *, vm_V32);
vm_V32 vm_absf  (vm_Program *, vm_V32);
vm_V32 vm_minf  (vm_Program *, vm_V32, vm_V32);
vm_V32 vm_maxf  (vm_Program *, vm_V32, vm_V32);
vm_V32 vm_sqrtf (vm_Program *, vm_V32);
vm_V32 vm_rcpf  (vm_Program *, vm_V32);   // ~1/x
vm_V32 vm_rsqrtf(vm_Program *, vm_V32);   // ~1/sqrt(x)
vm_V32 vm_floorf(vm_Program *, vm_V32);
vm_V32 vm_ceilf (vm_Program *, vm_V32);

// ---- Integer 32-bit arithmetic --------------------------------------------------

vm_V32 vm_addi(vm_Program *, vm_V32, vm_V32);
vm_V32 vm_subi(vm_Program *, vm_V32, vm_V32);
vm_V32 vm_muli(vm_Program *, vm_V32, vm_V32);
vm_V32 vm_negi(vm_Program *, vm_V32);
vm_V32 vm_absi(vm_Program *, vm_V32);
vm_V32 vm_mini(vm_Program *, vm_V32, vm_V32);   // signed
vm_V32 vm_maxi(vm_Program *, vm_V32, vm_V32);   // signed
vm_V32 vm_andi(vm_Program *, vm_V32, vm_V32);
vm_V32 vm_ori (vm_Program *, vm_V32, vm_V32);
vm_V32 vm_xori(vm_Program *, vm_V32, vm_V32);
vm_V32 vm_noti(vm_Program *, vm_V32);
vm_V32 vm_shli(vm_Program *, vm_V32, vm_V32 shift);  // logical left
vm_V32 vm_shri(vm_Program *, vm_V32, vm_V32 shift);  // logical right (zero-fill)
vm_V32 vm_sari(vm_Program *, vm_V32, vm_V32 shift);  // arithmetic right (sign-fill)

// ---- Float 16-bit arithmetic (color channels) -----------------------------------

vm_V16 vm_addf16 (vm_Program *, vm_V16, vm_V16);
vm_V16 vm_subf16 (vm_Program *, vm_V16, vm_V16);
vm_V16 vm_mulf16 (vm_Program *, vm_V16, vm_V16);
vm_V16 vm_divf16 (vm_Program *, vm_V16, vm_V16);
vm_V16 vm_fmaf16 (vm_Program *, vm_V16 a, vm_V16 b, vm_V16 c);  // a*b+c
vm_V16 vm_negf16 (vm_Program *, vm_V16);
vm_V16 vm_absf16 (vm_Program *, vm_V16);
vm_V16 vm_minf16 (vm_Program *, vm_V16, vm_V16);
vm_V16 vm_maxf16 (vm_Program *, vm_V16, vm_V16);
vm_V16 vm_sqrtf16(vm_Program *, vm_V16);
vm_V16 vm_rcpf16 (vm_Program *, vm_V16);

// ---- Integer 16-bit arithmetic --------------------------------------------------

vm_V16 vm_addi16(vm_Program *, vm_V16, vm_V16);
vm_V16 vm_subi16(vm_Program *, vm_V16, vm_V16);
vm_V16 vm_muli16(vm_Program *, vm_V16, vm_V16);
vm_V16 vm_andi16(vm_Program *, vm_V16, vm_V16);
vm_V16 vm_ori16 (vm_Program *, vm_V16, vm_V16);
vm_V16 vm_xori16(vm_Program *, vm_V16, vm_V16);
vm_V16 vm_noti16(vm_Program *, vm_V16);

// ---- Comparisons ----------------------------------------------------------------
// Results are vm_V32 with all bits 0 (false) or all bits 1 (true).
// Use vm_andi / vm_ori / vm_noti for mask logic.

vm_V32 vm_eqf  (vm_Program *, vm_V32, vm_V32);  // f32 ==
vm_V32 vm_nef  (vm_Program *, vm_V32, vm_V32);  // f32 !=
vm_V32 vm_ltf  (vm_Program *, vm_V32, vm_V32);  // f32 <
vm_V32 vm_lef  (vm_Program *, vm_V32, vm_V32);  // f32 <=

vm_V32 vm_eqi  (vm_Program *, vm_V32, vm_V32);  // i32 ==
vm_V32 vm_nei  (vm_Program *, vm_V32, vm_V32);  // i32 !=
vm_V32 vm_lti  (vm_Program *, vm_V32, vm_V32);  // i32 < (signed)
vm_V32 vm_lei  (vm_Program *, vm_V32, vm_V32);  // i32 <= (signed)

vm_V32 vm_eqf16(vm_Program *, vm_V16, vm_V16);  // f16 ==  → V32 mask
vm_V32 vm_ltf16(vm_Program *, vm_V16, vm_V16);  // f16 <   → V32 mask
vm_V32 vm_lef16(vm_Program *, vm_V16, vm_V16);  // f16 <=  → V32 mask

// ---- Select ---------------------------------------------------------------------

// mask: vm_V32 with all bits 0 or all bits 1 (output of a comparison or bitwise op)
vm_V32 vm_sel32(vm_Program *, vm_V32 mask, vm_V32 if_true, vm_V32 if_false);
vm_V16 vm_sel16(vm_Program *, vm_V32 mask, vm_V16 if_true, vm_V16 if_false);

// ---- Conversions ----------------------------------------------------------------

vm_V16 vm_f32_to_f16(vm_Program *, vm_V32);   // round to f16 precision
vm_V32 vm_f16_to_f32(vm_Program *, vm_V16);   // exact (f16 ⊂ f32)
vm_V32 vm_f32_to_i32(vm_Program *, vm_V32);   // truncate toward zero
vm_V32 vm_i32_to_f32(vm_Program *, vm_V32);
vm_V16 vm_i32_to_i16(vm_Program *, vm_V32);   // truncate to low 16 bits
vm_V32 vm_i16_to_i32(vm_Program *, vm_V16);   // sign-extend

// ============================================================
// Compilation and execution
// ============================================================

vm_Compiled *vm_compile(vm_Program *, vm_Backend);
void         vm_compiled_free(vm_Compiled *);

// Maps a vm_Ptr slot index to a concrete base address for one vm_run() call.
// 'ptr' matches vm_Ptr.id from vm_ptr(); 'base' must stay valid until vm_run() returns.
// Explicit _pad avoids implicit struct padding on 64-bit targets.
typedef struct {
    void     *base;
    uint32_t  ptr;
    uint32_t  _pad;
} vm_Binding;

// Execute the compiled program over n_lanes lanes.
// CPU backends process in SIMD-width chunks; SPIRV dispatches n_lanes threads.
// Re-entrant: multiple threads may call concurrently with different bindings.
void vm_run(vm_Compiled      *,
            const vm_Binding *bindings,
            int               n_bindings,
            int               n_lanes);
