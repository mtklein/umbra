#include "vm.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wfloat-equal"                // IEEE 754 comparisons intentional
#pragma clang diagnostic ignored "-Wdeclaration-after-statement" // C11, not C89
#pragma clang diagnostic ignored "-Wswitch-default"             // all enum values handled explicitly
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"        // raw pointer arithmetic is intentional

// ============================================================
// Internal opcode set
//
// Each opcode encodes width AND semantics, so no runtime type
// field is needed.  Memory ops only encode width (f/i is a
// compute-time distinction; the bits in memory are the same).
// ============================================================

typedef enum {
    // Immediates
    OP_IMMF,   OP_IMMI,   // 32-bit float / int immediate
    OP_IMMF16, OP_IMMI16, // 16-bit float / int immediate
    // Built-in
    OP_LANE_ID,            // varying i32: 0, 1, 2 … N-1
    // Memory (width-only: 32 or 16 bytes)
    OP_UNIFORM_LOAD32,  OP_UNIFORM_LOAD16,
    OP_VARYING_LOAD32,  OP_VARYING_LOAD16,
    OP_VARYING_GATHER32, OP_VARYING_GATHER16,
    OP_VARYING_STORE32, OP_VARYING_STORE16,
    // Float 32
    OP_ADDF,   OP_SUBF,  OP_MULF,  OP_DIVF, OP_FMAF,
    OP_NEGF,   OP_ABSF,  OP_MINF,  OP_MAXF,
    OP_SQRTF,  OP_RCPF,  OP_RSQRTF, OP_FLOORF, OP_CEILF,
    // Integer 32
    OP_ADDI,   OP_SUBI,  OP_MULI,
    OP_NEGI,   OP_ABSI,  OP_MINI,  OP_MAXI,
    OP_ANDI,   OP_ORI,   OP_XORI,  OP_NOTI,
    OP_SHLI,   OP_SHRI,  OP_SARI,
    // Float 16
    OP_ADDF16, OP_SUBF16, OP_MULF16, OP_DIVF16, OP_FMAF16,
    OP_NEGF16, OP_ABSF16, OP_MINF16, OP_MAXF16,
    OP_SQRTF16, OP_RCPF16,
    // Integer 16
    OP_ADDI16, OP_SUBI16, OP_MULI16,
    OP_ANDI16, OP_ORI16,  OP_XORI16, OP_NOTI16,
    // Comparisons → V32 mask (0 or ~0)
    OP_EQF,   OP_NEF,   OP_LTF,   OP_LEF,   // f32
    OP_EQI,   OP_NEI,   OP_LTI,   OP_LEI,   // i32 signed
    OP_EQF16, OP_LTF16, OP_LEF16,           // f16
    // Select
    OP_SEL32, OP_SEL16,
    // Conversions
    OP_F32_TO_F16, OP_F16_TO_F32,
    OP_F32_TO_I32, OP_I32_TO_F32,
    OP_I32_TO_I16, OP_I16_TO_I32,
} Op;

// ---- Internal IR instruction ----------------------------------------------------
//
// x/y/z are input operand IDs (0 = unused).
// id is the output SSA value ID (0 for stores).
// For immediates, imm holds the constant.
// For memory ops, ptr/offset/stride encode the address formula.
//
// No 'type' field — the opcode encodes everything.

typedef struct {
    Op       op;
    uint32_t id;       // output value ID (0 for stores)
    int      uniform;  // 1 if result is the same for all lanes
    uint32_t x, y, z; // input operand IDs (0 = unused)
    union { float f32; int32_t i32; int16_t i16; } imm;
    uint32_t ptr;
    int32_t  offset;
    int32_t  stride;
} Instr;

// ---- Per-value metadata ---------------------------------------------------------

typedef struct {
    int width;   // 32 or 16
    int uniform; // 1 = uniform across all lanes
} ValInfo;

// ---- Program and compiled-program structs ---------------------------------------

struct vm_Program {
    Instr   *instrs;
    int      n_instrs;
    int      cap_instrs;
    uint32_t n_ptrs;
    uint32_t next_id;  // next SSA ID; starts at 1 (0 = no value)
    ValInfo *vals;
    int      cap_vals;
};

struct vm_Compiled {
    Instr      *instrs;
    int         n_instrs;
    uint32_t    n_ptrs;
    uint32_t    n_vals;
    vm_Backend  backend;
};

// ============================================================
// Builder helpers
// ============================================================

static uint32_t new_val(vm_Program *p, int width, int uniform) {
    uint32_t id = p->next_id++;
    if ((int)id >= p->cap_vals) {
        int cap = p->cap_vals ? p->cap_vals * 2 : 16;
        p->vals = (ValInfo *)realloc(p->vals, (size_t)cap * sizeof(ValInfo));
        p->cap_vals = cap;
    }
    p->vals[id] = (ValInfo){ width, uniform };
    return id;
}

static void push(vm_Program *p, Instr ins) {
    if (p->n_instrs == p->cap_instrs) {
        int cap = p->cap_instrs ? p->cap_instrs * 2 : 16;
        p->instrs = (Instr *)realloc(p->instrs, (size_t)cap * sizeof(Instr));
        p->cap_instrs = cap;
    }
    p->instrs[p->n_instrs++] = ins;
}

static int val_uniform(const vm_Program *p, uint32_t v) {
    return v == 0 || p->vals[v].uniform;
}

// Emit a 32-bit binary op; result width is always 32.
static vm_V32 binop32(vm_Program *p, Op op, uint32_t x, uint32_t y) {
    int u = val_uniform(p, x) && val_uniform(p, y);
    uint32_t id = new_val(p, 32, u);
    push(p, (Instr){ .op=op, .id=id, .uniform=u, .x=x, .y=y });
    return (vm_V32){ id };
}

// Emit a 16-bit binary op; result width is always 16.
static vm_V16 binop16(vm_Program *p, Op op, uint32_t x, uint32_t y) {
    int u = val_uniform(p, x) && val_uniform(p, y);
    uint32_t id = new_val(p, 16, u);
    push(p, (Instr){ .op=op, .id=id, .uniform=u, .x=x, .y=y });
    return (vm_V16){ id };
}

// Emit a 32-bit unary op.
static vm_V32 unop32(vm_Program *p, Op op, uint32_t x) {
    int u = val_uniform(p, x);
    uint32_t id = new_val(p, 32, u);
    push(p, (Instr){ .op=op, .id=id, .uniform=u, .x=x });
    return (vm_V32){ id };
}

// Emit a 16-bit unary op.
static vm_V16 unop16(vm_Program *p, Op op, uint32_t x) {
    int u = val_uniform(p, x);
    uint32_t id = new_val(p, 16, u);
    push(p, (Instr){ .op=op, .id=id, .uniform=u, .x=x });
    return (vm_V16){ id };
}

// Emit a 32-bit ternary op.
static vm_V32 triop32(vm_Program *p, Op op, uint32_t x, uint32_t y, uint32_t z) {
    int u = val_uniform(p, x) && val_uniform(p, y) && val_uniform(p, z);
    uint32_t id = new_val(p, 32, u);
    push(p, (Instr){ .op=op, .id=id, .uniform=u, .x=x, .y=y, .z=z });
    return (vm_V32){ id };
}

// Emit a 16-bit ternary op.
static vm_V16 triop16(vm_Program *p, Op op, uint32_t x, uint32_t y, uint32_t z) {
    int u = val_uniform(p, x) && val_uniform(p, y) && val_uniform(p, z);
    uint32_t id = new_val(p, 16, u);
    push(p, (Instr){ .op=op, .id=id, .uniform=u, .x=x, .y=y, .z=z });
    return (vm_V16){ id };
}

// ============================================================
// Public builder API
// ============================================================

vm_Program *vm_program(void) {
    vm_Program *p = (vm_Program *)calloc(1, sizeof(vm_Program));
    p->next_id = 1;
    return p;
}
void vm_program_free(vm_Program *p) { free(p->instrs); free(p->vals); free(p); }

vm_Ptr vm_ptr(vm_Program *p) { return (vm_Ptr){ p->n_ptrs++ }; }

// Immediates
vm_V32 vm_immf  (vm_Program *p, float v) {
    uint32_t id = new_val(p, 32, 1);
    push(p, (Instr){ .op=OP_IMMF,   .id=id, .uniform=1, .imm.f32=v });
    return (vm_V32){ id };
}
vm_V32 vm_immi  (vm_Program *p, int32_t v) {
    uint32_t id = new_val(p, 32, 1);
    push(p, (Instr){ .op=OP_IMMI,   .id=id, .uniform=1, .imm.i32=v });
    return (vm_V32){ id };
}
vm_V16 vm_immf16(vm_Program *p, float v) {
    uint32_t id = new_val(p, 16, 1);
    push(p, (Instr){ .op=OP_IMMF16, .id=id, .uniform=1, .imm.f32=v });
    return (vm_V16){ id };
}
vm_V16 vm_immi16(vm_Program *p, int16_t v) {
    uint32_t id = new_val(p, 16, 1);
    push(p, (Instr){ .op=OP_IMMI16, .id=id, .uniform=1, .imm.i16=v });
    return (vm_V16){ id };
}

vm_V32 vm_lane_id(vm_Program *p) {
    uint32_t id = new_val(p, 32, 0);
    push(p, (Instr){ .op=OP_LANE_ID, .id=id, .uniform=0 });
    return (vm_V32){ id };
}

// Memory
vm_V32 vm_uniform_load32(vm_Program *p, vm_Ptr ptr, int32_t off) {
    uint32_t id = new_val(p, 32, 1);
    push(p, (Instr){ .op=OP_UNIFORM_LOAD32, .id=id, .uniform=1, .ptr=ptr.id, .offset=off });
    return (vm_V32){ id };
}
vm_V16 vm_uniform_load16(vm_Program *p, vm_Ptr ptr, int32_t off) {
    uint32_t id = new_val(p, 16, 1);
    push(p, (Instr){ .op=OP_UNIFORM_LOAD16, .id=id, .uniform=1, .ptr=ptr.id, .offset=off });
    return (vm_V16){ id };
}
vm_V32 vm_varying_load32(vm_Program *p, vm_Ptr ptr, int32_t off, int32_t stride) {
    uint32_t id = new_val(p, 32, 0);
    push(p, (Instr){ .op=OP_VARYING_LOAD32, .id=id, .uniform=0, .ptr=ptr.id, .offset=off, .stride=stride });
    return (vm_V32){ id };
}
vm_V16 vm_varying_load16(vm_Program *p, vm_Ptr ptr, int32_t off, int32_t stride) {
    uint32_t id = new_val(p, 16, 0);
    push(p, (Instr){ .op=OP_VARYING_LOAD16, .id=id, .uniform=0, .ptr=ptr.id, .offset=off, .stride=stride });
    return (vm_V16){ id };
}
vm_V32 vm_varying_gather32(vm_Program *p, vm_Ptr ptr, vm_V32 byte_offsets) {
    uint32_t id = new_val(p, 32, 0);
    push(p, (Instr){ .op=OP_VARYING_GATHER32, .id=id, .uniform=0, .ptr=ptr.id, .x=byte_offsets.id });
    return (vm_V32){ id };
}
vm_V16 vm_varying_gather16(vm_Program *p, vm_Ptr ptr, vm_V32 byte_offsets) {
    uint32_t id = new_val(p, 16, 0);
    push(p, (Instr){ .op=OP_VARYING_GATHER16, .id=id, .uniform=0, .ptr=ptr.id, .x=byte_offsets.id });
    return (vm_V16){ id };
}
void vm_varying_store32(vm_Program *p, vm_Ptr ptr, int32_t off, int32_t stride, vm_V32 v) {
    push(p, (Instr){ .op=OP_VARYING_STORE32, .id=0, .uniform=0, .ptr=ptr.id, .offset=off, .stride=stride, .x=v.id });
}
void vm_varying_store16(vm_Program *p, vm_Ptr ptr, int32_t off, int32_t stride, vm_V16 v) {
    push(p, (Instr){ .op=OP_VARYING_STORE16, .id=0, .uniform=0, .ptr=ptr.id, .offset=off, .stride=stride, .x=v.id });
}

// Float 32
vm_V32 vm_addf  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_ADDF,   x.id, y.id); }
vm_V32 vm_subf  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_SUBF,   x.id, y.id); }
vm_V32 vm_mulf  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_MULF,   x.id, y.id); }
vm_V32 vm_divf  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_DIVF,   x.id, y.id); }
vm_V32 vm_fmaf  (vm_Program *p, vm_V32 a, vm_V32 b, vm_V32 c) { return triop32(p, OP_FMAF, a.id, b.id, c.id); }
vm_V32 vm_negf  (vm_Program *p, vm_V32 x) { return unop32(p, OP_NEGF,   x.id); }
vm_V32 vm_absf  (vm_Program *p, vm_V32 x) { return unop32(p, OP_ABSF,   x.id); }
vm_V32 vm_minf  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_MINF,   x.id, y.id); }
vm_V32 vm_maxf  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_MAXF,   x.id, y.id); }
vm_V32 vm_sqrtf (vm_Program *p, vm_V32 x) { return unop32(p, OP_SQRTF,  x.id); }
vm_V32 vm_rcpf  (vm_Program *p, vm_V32 x) { return unop32(p, OP_RCPF,   x.id); }
vm_V32 vm_rsqrtf(vm_Program *p, vm_V32 x) { return unop32(p, OP_RSQRTF, x.id); }
vm_V32 vm_floorf(vm_Program *p, vm_V32 x) { return unop32(p, OP_FLOORF, x.id); }
vm_V32 vm_ceilf (vm_Program *p, vm_V32 x) { return unop32(p, OP_CEILF,  x.id); }

// Integer 32
vm_V32 vm_addi(vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_ADDI, x.id, y.id); }
vm_V32 vm_subi(vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_SUBI, x.id, y.id); }
vm_V32 vm_muli(vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_MULI, x.id, y.id); }
vm_V32 vm_negi(vm_Program *p, vm_V32 x) { return unop32(p, OP_NEGI, x.id); }
vm_V32 vm_absi(vm_Program *p, vm_V32 x) { return unop32(p, OP_ABSI, x.id); }
vm_V32 vm_mini(vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_MINI, x.id, y.id); }
vm_V32 vm_maxi(vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_MAXI, x.id, y.id); }
vm_V32 vm_andi(vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_ANDI, x.id, y.id); }
vm_V32 vm_ori (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_ORI,  x.id, y.id); }
vm_V32 vm_xori(vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_XORI, x.id, y.id); }
vm_V32 vm_noti(vm_Program *p, vm_V32 x) { return unop32(p, OP_NOTI, x.id); }
vm_V32 vm_shli(vm_Program *p, vm_V32 x, vm_V32 sh) { return binop32(p, OP_SHLI, x.id, sh.id); }
vm_V32 vm_shri(vm_Program *p, vm_V32 x, vm_V32 sh) { return binop32(p, OP_SHRI, x.id, sh.id); }
vm_V32 vm_sari(vm_Program *p, vm_V32 x, vm_V32 sh) { return binop32(p, OP_SARI, x.id, sh.id); }

// Float 16
vm_V16 vm_addf16 (vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_ADDF16,  x.id, y.id); }
vm_V16 vm_subf16 (vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_SUBF16,  x.id, y.id); }
vm_V16 vm_mulf16 (vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_MULF16,  x.id, y.id); }
vm_V16 vm_divf16 (vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_DIVF16,  x.id, y.id); }
vm_V16 vm_fmaf16 (vm_Program *p, vm_V16 a, vm_V16 b, vm_V16 c) { return triop16(p, OP_FMAF16, a.id, b.id, c.id); }
vm_V16 vm_negf16 (vm_Program *p, vm_V16 x) { return unop16(p, OP_NEGF16,  x.id); }
vm_V16 vm_absf16 (vm_Program *p, vm_V16 x) { return unop16(p, OP_ABSF16,  x.id); }
vm_V16 vm_minf16 (vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_MINF16,  x.id, y.id); }
vm_V16 vm_maxf16 (vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_MAXF16,  x.id, y.id); }
vm_V16 vm_sqrtf16(vm_Program *p, vm_V16 x) { return unop16(p, OP_SQRTF16, x.id); }
vm_V16 vm_rcpf16 (vm_Program *p, vm_V16 x) { return unop16(p, OP_RCPF16,  x.id); }

// Integer 16
vm_V16 vm_addi16(vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_ADDI16, x.id, y.id); }
vm_V16 vm_subi16(vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_SUBI16, x.id, y.id); }
vm_V16 vm_muli16(vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_MULI16, x.id, y.id); }
vm_V16 vm_andi16(vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_ANDI16, x.id, y.id); }
vm_V16 vm_ori16 (vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_ORI16,  x.id, y.id); }
vm_V16 vm_xori16(vm_Program *p, vm_V16 x, vm_V16 y) { return binop16(p, OP_XORI16, x.id, y.id); }
vm_V16 vm_noti16(vm_Program *p, vm_V16 x) { return unop16(p, OP_NOTI16, x.id); }

// Comparisons → V32 mask
vm_V32 vm_eqf  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_EQF,   x.id, y.id); }
vm_V32 vm_nef  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_NEF,   x.id, y.id); }
vm_V32 vm_ltf  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_LTF,   x.id, y.id); }
vm_V32 vm_lef  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_LEF,   x.id, y.id); }
vm_V32 vm_eqi  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_EQI,   x.id, y.id); }
vm_V32 vm_nei  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_NEI,   x.id, y.id); }
vm_V32 vm_lti  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_LTI,   x.id, y.id); }
vm_V32 vm_lei  (vm_Program *p, vm_V32 x, vm_V32 y) { return binop32(p, OP_LEI,   x.id, y.id); }
vm_V32 vm_eqf16(vm_Program *p, vm_V16 x, vm_V16 y) { return binop32(p, OP_EQF16, x.id, y.id); }
vm_V32 vm_ltf16(vm_Program *p, vm_V16 x, vm_V16 y) { return binop32(p, OP_LTF16, x.id, y.id); }
vm_V32 vm_lef16(vm_Program *p, vm_V16 x, vm_V16 y) { return binop32(p, OP_LEF16, x.id, y.id); }

// Select
vm_V32 vm_sel32(vm_Program *p, vm_V32 mask, vm_V32 a, vm_V32 b) {
    return triop32(p, OP_SEL32, mask.id, a.id, b.id);
}
vm_V16 vm_sel16(vm_Program *p, vm_V32 mask, vm_V16 a, vm_V16 b) {
    return triop16(p, OP_SEL16, mask.id, a.id, b.id);
}

// Conversions
vm_V16 vm_f32_to_f16(vm_Program *p, vm_V32 x) {
    uint32_t id = new_val(p, 16, val_uniform(p, x.id));
    push(p, (Instr){ .op=OP_F32_TO_F16, .id=id, .uniform=p->vals[id].uniform, .x=x.id });
    return (vm_V16){ id };
}
vm_V32 vm_f16_to_f32(vm_Program *p, vm_V16 x) {
    uint32_t id = new_val(p, 32, val_uniform(p, x.id));
    push(p, (Instr){ .op=OP_F16_TO_F32, .id=id, .uniform=p->vals[id].uniform, .x=x.id });
    return (vm_V32){ id };
}
vm_V32 vm_f32_to_i32(vm_Program *p, vm_V32 x) { return unop32(p, OP_F32_TO_I32, x.id); }
vm_V32 vm_i32_to_f32(vm_Program *p, vm_V32 x) { return unop32(p, OP_I32_TO_F32, x.id); }
vm_V16 vm_i32_to_i16(vm_Program *p, vm_V32 x) {
    uint32_t id = new_val(p, 16, val_uniform(p, x.id));
    push(p, (Instr){ .op=OP_I32_TO_I16, .id=id, .uniform=p->vals[id].uniform, .x=x.id });
    return (vm_V16){ id };
}
vm_V32 vm_i16_to_i32(vm_Program *p, vm_V16 x) {
    uint32_t id = new_val(p, 32, val_uniform(p, x.id));
    push(p, (Instr){ .op=OP_I16_TO_I32, .id=id, .uniform=p->vals[id].uniform, .x=x.id });
    return (vm_V32){ id };
}

// ============================================================
// Compilation
// ============================================================

vm_Compiled *vm_compile(vm_Program *p, vm_Backend backend) {
    vm_Compiled *c;
    (void)backend;  // TODO: CPU SIMD JIT and SPIRV backends
    c = (vm_Compiled *)calloc(1, sizeof(vm_Compiled));
    c->instrs   = (Instr *)malloc((size_t)p->n_instrs * sizeof(Instr));
    c->n_instrs = p->n_instrs;
    c->n_ptrs   = p->n_ptrs;
    c->n_vals   = p->next_id;
    c->backend  = VM_BACKEND_INTERP;
    memcpy(c->instrs, p->instrs, (size_t)p->n_instrs * sizeof(Instr));
    return c;
}
void vm_compiled_free(vm_Compiled *c) { free(c->instrs); free(c); }

// ============================================================
// Scalar interpreter
//
// One Reg per SSA value.  Both V32 and V16 values use the same
// union; V16 is stored as follows:
//   f16  → low 16 bits of r.u hold the f16 bit pattern
//   i16  → r.i holds the value sign-extended to 32 bits
//
// F16 arithmetic converts to/from float via f16_bits_to_f32 /
// f32_to_f16_bits, faithfully rounding to f16 precision each op.
// ============================================================

typedef union { float f; int32_t i; uint32_t u; } Reg;

// F16 <-> F32 round-trip (no hardware dependency).
static uint16_t f32_to_f16_bits(float v) {
    uint32_t x, sign, mant;
    int32_t  exp;
    memcpy(&x, &v, 4);
    sign =  (x >> 16) & 0x8000u;
    exp  = (int32_t)((x >> 23) & 0xffu) - 127 + 15;
    mant =  (x >> 13) & 0x3ffu;
    if (exp <= 0)  return (uint16_t)sign;
    if (exp >= 31) return (uint16_t)(sign | 0x7c00u);
    return (uint16_t)(sign | ((uint32_t)exp << 10) | mant);
}
static float f16_bits_to_f32(uint16_t h) {
    uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp  = (h >> 10) & 0x1fu;
    uint32_t mant =  h        & 0x3ffu;
    uint32_t x    = 0;
    float    f;
    if      (exp ==  0u) { x = sign; }
    else if (exp == 31u) { x = sign | 0x7f800000u | (mant << 13); }
    else                 { x = sign | ((exp + 127u - 15u) << 23) | (mant << 13); }
    memcpy(&f, &x, 4);
    return f;
}

// Helper: read V16 reg as float, write result back as snapped f16.
#define F16(r)      f16_bits_to_f32((uint16_t)(r).u)
#define SNAP(v)     ((uint32_t)f32_to_f16_bits(v))

static void exec_instr(const Instr *ins, Reg *regs, void **ptrs, int lane) {
    Reg x = ins->x ? regs[ins->x] : (Reg){0};
    Reg y = ins->y ? regs[ins->y] : (Reg){0};
    Reg z = ins->z ? regs[ins->z] : (Reg){0};
    Reg r = {0};

    switch (ins->op) {

        case OP_IMMF:   r.f = ins->imm.f32; break;
        case OP_IMMI:   r.i = ins->imm.i32; break;
        case OP_IMMF16: r.u = SNAP(ins->imm.f32); break;
        case OP_IMMI16: r.i = ins->imm.i16; break;
        case OP_LANE_ID: r.i = lane; break;

        case OP_UNIFORM_LOAD32: { memcpy(&r, (const uint8_t *)ptrs[ins->ptr] + ins->offset, 4); break; }
        case OP_UNIFORM_LOAD16: { uint16_t v; memcpy(&v, (const uint8_t *)ptrs[ins->ptr] + ins->offset, 2); r.u = v; break; }
        case OP_VARYING_LOAD32: { memcpy(&r, (const uint8_t *)ptrs[ins->ptr] + ins->offset + lane * ins->stride, 4); break; }
        case OP_VARYING_LOAD16: { uint16_t v; memcpy(&v, (const uint8_t *)ptrs[ins->ptr] + ins->offset + lane * ins->stride, 2); r.u = v; break; }
        case OP_VARYING_GATHER32: { memcpy(&r, (const uint8_t *)ptrs[ins->ptr] + x.i, 4); break; }
        case OP_VARYING_GATHER16: { uint16_t v; memcpy(&v, (const uint8_t *)ptrs[ins->ptr] + x.i, 2); r.u = v; break; }
        case OP_VARYING_STORE32:  { memcpy((uint8_t *)ptrs[ins->ptr] + ins->offset + lane * ins->stride, &x, 4); break; }
        case OP_VARYING_STORE16:  { uint16_t v = (uint16_t)x.u; memcpy((uint8_t *)ptrs[ins->ptr] + ins->offset + lane * ins->stride, &v, 2); break; }

        case OP_ADDF: r.f = x.f + y.f; break;
        case OP_SUBF: r.f = x.f - y.f; break;
        case OP_MULF: r.f = x.f * y.f; break;
        case OP_DIVF: r.f = x.f / y.f; break;
        case OP_FMAF: r.f = x.f * y.f + z.f; break;
        case OP_NEGF: r.f = -x.f; break;
        case OP_ABSF: r.f = fabsf(x.f); break;
        case OP_MINF: r.f = x.f < y.f ? x.f : y.f; break;
        case OP_MAXF: r.f = x.f > y.f ? x.f : y.f; break;
        case OP_SQRTF:  r.f = sqrtf(x.f); break;
        case OP_RCPF:   r.f = 1.0f / x.f; break;
        case OP_RSQRTF: r.f = 1.0f / sqrtf(x.f); break;
        case OP_FLOORF: r.f = floorf(x.f); break;
        case OP_CEILF:  r.f = ceilf(x.f); break;

        case OP_ADDI: r.i = x.i + y.i; break;
        case OP_SUBI: r.i = x.i - y.i; break;
        case OP_MULI: r.i = x.i * y.i; break;
        case OP_NEGI: r.i = -x.i; break;
        case OP_ABSI: r.i = x.i < 0 ? -x.i : x.i; break;
        case OP_MINI: r.i = x.i < y.i ? x.i : y.i; break;
        case OP_MAXI: r.i = x.i > y.i ? x.i : y.i; break;
        case OP_ANDI: r.i = x.i & y.i; break;
        case OP_ORI:  r.i = x.i | y.i; break;
        case OP_XORI: r.i = x.i ^ y.i; break;
        case OP_NOTI: r.i = ~x.i; break;
        case OP_SHLI: r.i = (int32_t)((uint32_t)x.i << (uint32_t)(y.i & 31)); break;
        case OP_SHRI: r.i = (int32_t)((uint32_t)x.i >> (uint32_t)(y.i & 31)); break;
        case OP_SARI: r.i =           x.i          >> (uint32_t)(y.i & 31);   break;

        case OP_ADDF16: r.u = SNAP(F16(x) + F16(y)); break;
        case OP_SUBF16: r.u = SNAP(F16(x) - F16(y)); break;
        case OP_MULF16: r.u = SNAP(F16(x) * F16(y)); break;
        case OP_DIVF16: r.u = SNAP(F16(x) / F16(y)); break;
        case OP_FMAF16: r.u = SNAP(F16(x) * F16(y) + F16(z)); break;
        case OP_NEGF16: r.u = SNAP(-F16(x)); break;
        case OP_ABSF16: r.u = SNAP(fabsf(F16(x))); break;
        case OP_MINF16: { float a=F16(x), b=F16(y); r.u = SNAP(a < b ? a : b); break; }
        case OP_MAXF16: { float a=F16(x), b=F16(y); r.u = SNAP(a > b ? a : b); break; }
        case OP_SQRTF16: r.u = SNAP(sqrtf(F16(x))); break;
        case OP_RCPF16:  r.u = SNAP(1.0f / F16(x)); break;

        case OP_ADDI16: r.i = (int16_t)(x.i + y.i); break;
        case OP_SUBI16: r.i = (int16_t)(x.i - y.i); break;
        case OP_MULI16: r.i = (int16_t)(x.i * y.i); break;
        case OP_ANDI16: r.i = (int16_t)(x.i & y.i); break;
        case OP_ORI16:  r.i = (int16_t)(x.i | y.i); break;
        case OP_XORI16: r.i = (int16_t)(x.i ^ y.i); break;
        case OP_NOTI16: r.i = (int16_t)(~x.i); break;

        case OP_EQF:   r.i = x.f == y.f ? ~0 : 0; break;
        case OP_NEF:   r.i = x.f != y.f ? ~0 : 0; break;
        case OP_LTF:   r.i = x.f  < y.f ? ~0 : 0; break;
        case OP_LEF:   r.i = x.f <= y.f ? ~0 : 0; break;
        case OP_EQI:   r.i = x.i == y.i ? ~0 : 0; break;
        case OP_NEI:   r.i = x.i != y.i ? ~0 : 0; break;
        case OP_LTI:   r.i = x.i  < y.i ? ~0 : 0; break;
        case OP_LEI:   r.i = x.i <= y.i ? ~0 : 0; break;
        case OP_EQF16: { float a=F16(x), b=F16(y); r.i = a == b ? ~0 : 0; break; }
        case OP_LTF16: { float a=F16(x), b=F16(y); r.i = a  < b ? ~0 : 0; break; }
        case OP_LEF16: { float a=F16(x), b=F16(y); r.i = a <= b ? ~0 : 0; break; }

        // mask is 0 or ~0; select using it
        case OP_SEL32: r = x.i ? y : z; break;
        case OP_SEL16: r = x.i ? y : z; break;

        case OP_F32_TO_F16: r.u = SNAP(x.f); break;
        case OP_F16_TO_F32: r.f = F16(x); break;
        case OP_F32_TO_I32: r.i = (int32_t)x.f; break;
        case OP_I32_TO_F32: r.f = (float)x.i; break;
        case OP_I32_TO_I16: r.i = (int16_t)x.i; break;
        case OP_I16_TO_I32: r.i = (int16_t)x.i; break;
    }

    if (ins->id) {
        regs[ins->id] = r;
    }
}

void vm_run(vm_Compiled      *c,
            const vm_Binding *bindings,
            int               n_bindings,
            int               n_lanes)
{
    void **ptrs = (void **)calloc(c->n_ptrs, sizeof(void *));
    Reg   *regs = (Reg   *)calloc(c->n_vals, sizeof(Reg));

    for (int i = 0; i < n_bindings; i++) {
        if (bindings[i].ptr < c->n_ptrs) {
            ptrs[bindings[i].ptr] = bindings[i].base;
        }
    }

    // Phase 1: evaluate uniform instructions once.
    for (int i = 0; i < c->n_instrs; i++) {
        const Instr *ins = &c->instrs[i];
        if (ins->uniform && ins->id != 0) {
            exec_instr(ins, regs, ptrs, 0);
        }
    }

    // Phase 2: evaluate varying instructions (and stores) per lane.
    for (int lane = 0; lane < n_lanes; lane++) {
        for (int i = 0; i < c->n_instrs; i++) {
            const Instr *ins = &c->instrs[i];
            if (!ins->uniform) {
                exec_instr(ins, regs, ptrs, lane);
            }
        }
    }

    free(regs);
    free(ptrs);
}
