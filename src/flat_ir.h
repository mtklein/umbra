#pragma once
#include "../include/umbra.h"
#include "hash.h"
#include "op.h"
#include <stdint.h>

// TODO: support arbitrary uniform looping, including nested uniform loops
// TODO: replace flat_ir with something less flat?
//
// We now have complicated control flow.  Is there some representation
// for our programs that would be less error-prone than a flat op array?

typedef union {
    int         bits;
    umbra_val32 v32;
    umbra_val16 v16;
    struct { int id : 30; unsigned chan : 2; };
} val;

typedef union {
    int       bits;
    umbra_ptr p;
    struct { int ix; };
} ptr;

// Broadest scope within which a value is invariant, broadest to narrowest:
enum scope {
    SCOPE_COMPILE  = 0,  // compile-time immediates
    SCOPE_DISPATCH = 1,  // per dispatch() call (e.g. uniforms)
    SCOPE_ROW      = 2,  // fixed per-row    (e.g. umbra_y)
    SCOPE_SUBGROUP = 3,  // fixed within one K-lane subgroup
    SCOPE_LANE     = 4,  // varies per lane
};

struct ir_inst {
    enum op op;
    val     x, y, z, w;
    ptr     ptr;
    int     imm;

    // Bookkeeping metadata, doesn't need to be hashed.
    _Bool  live, uniform; int8_t :8;
    int8_t scope;
    int    final_id;
};


enum binding_kind {
    BIND_BUF,
    BIND_SEALED,
    BIND_UNIFORMS,
};

struct buffer_binding {
    enum binding_kind kind;
    int               ix;
    union {
        struct umbra_buf const *buf;
        struct umbra_buf        uniforms;  // when kind=BIND_UNIFORMS, .count = slots.
    };
};

enum { BUF_READ = 1, BUF_WRITTEN = 2 };

struct buffer_metadata {
    uint8_t           shift;          // op_elem_shift of any op on this ptr
    uint8_t           rw;             // BUF_READ | BUF_WRITTEN flags
    int               :16;
    enum binding_kind binding_kind;
    int               uniform_slots;  // slot count when binding_kind == BIND_UNIFORMS
};

void resolve_bindings(struct umbra_buf *out,
                      struct buffer_binding const *binding, int bindings,
                      struct umbra_late_binding const *late, int lates);

// True iff the IR stores to any buffer that has an early default.  Such a
// buffer is a shared piece of state across concurrent dispatch() calls, so the
// backend can't tag the compiled program as thread-safe.
_Bool flat_ir_has_early_writes(struct umbra_flat_ir const*);

struct umbra_builder {
    struct ir_inst           *inst;
    _Bool                    *var_uniform;
    struct buffer_binding    *binding;
    struct hash               ht;
    int                       insts, cap;
    int                       vars, if_depth;
    int                       bindings, cap_bindings;

    umbra_val32               loop_trip;
    umbra_var32               loop_var;
    _Bool                     has_loop, loop_closed; int :16, :32;
};

struct umbra_flat_ir {
    struct ir_inst           *inst;
    struct buffer_binding    *binding;

    // Per-ptr buffer metadata, computed at IR construction.  Sized
    // [total_bufs].  total_bufs == max(ptr.bits) + 1, or 0 if no
    // ptr-bearing op is live.
    struct buffer_metadata   *buf;

    int insts,         // Total instruction count.
        dispatch_end,  // inst[0..dispatch_end) have scope ≤ SCOPE_DISPATCH.
        row_end;       // inst[dispatch_end..row_end) have scope == SCOPE_ROW.
                       // inst[row_end..insts) is the per-subgroup body.
    int loop_begin,    // Instruction index of op_loop_begin, or -1.
        loop_end;      // Instruction index of op_loop_end,   or -1.
    int vars, bindings;
    int total_bufs;
};
