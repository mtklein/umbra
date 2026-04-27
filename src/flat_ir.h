#pragma once
#include "../include/umbra.h"
#include "hash.h"
#include "op.h"
#include <stdint.h>

// TODO: explore replacing flat_ir with a graph representation.
//       now that we have complicated control flow like if/endif and a loop.
//       It is starting to feel error prone optimizing and code generating with
//       control flow ops present, and it would also be nice to generalize loop
//       support to multiple and nested loops, maybe even loops over varyings?

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

// Value scope: the broadest frame across which a value is invariant.  Frames
// nest from broadest to narrowest:
//
//   SCOPE_COMPILE  — fixed at IR build (literals).
//   SCOPE_DISPATCH — fixed for one queue() call (uniform_32 reads).
//   SCOPE_ROW      — fixed across one output row (umbra_y, gathers).
//   SCOPE_BATCH    — fixed across one K-lane column step.
//   SCOPE_ITER     — fixed within one loop iteration.
//   SCOPE_LANE     — varies per SIMD lane (umbra_x, all loads/stores).
//
// Scope is computed from each op's intrinsic scope and the scopes of its
// operands.  The scheduler partitions the IR into three tiers based on
// scope: dispatch tier (SCOPE_COMPILE, SCOPE_DISPATCH) emitted once per
// queue() call, row tier (SCOPE_ROW) emitted at each row's entry, and the
// per-batch body (SCOPE_BATCH and narrower).  See dispatch_end / preamble
// in struct umbra_flat_ir.
enum scope {
    SCOPE_COMPILE  = 0,
    SCOPE_DISPATCH = 1,
    SCOPE_ROW      = 2,
    SCOPE_BATCH    = 3,
    SCOPE_ITER     = 4,
    SCOPE_LANE     = 5,
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

// A buf registration pinned to a specific ptr handle (.ix).  The resolver
// populates buf[.ix] at dispatch time by (optionally) reading the early
// default stored here, then letting any matching umbra_late_binding win.
//
//   BIND_BUF        `.buf` is a caller-owned umbra_buf* (or NULL) dereferenced
//                   on every dispatch.
//   BIND_SEALED_BUF Same storage as BIND_BUF, but the caller has sealed it:
//                   the host bytes don't change after binding, so the cache
//                   may skip fingerprinting and re-uploads.
//   BIND_UNIFORMS   `.uniforms` carries the slot count; `.uniforms.ptr` is the
//                   optional early-default pointer (or NULL).
enum binding_kind {
    BIND_BUF,
    BIND_SEALED_BUF,
    BIND_UNIFORMS,
};

// The cache reads BUF_SEALED alongside BUF_READ/BUF_WRITTEN to decide
// whether to skip fingerprinting and re-uploads for an entry.
enum { BUF_READ = 1, BUF_WRITTEN = 2, BUF_SEALED = 4 };

struct buffer_binding {
    enum binding_kind kind;
    int               ix;
    union {
        struct umbra_buf const *buf;       // BIND_BUF: live ptr, may be NULL.
        struct umbra_buf        uniforms;  // BIND_UNIFORMS: .ptr optional, .count = slots.
    };
};

// Per-ptr metadata gathered by a single IR walk; indexed by ptr.bits.
struct buffer_metadata {
    uint8_t shift;            // op_elem_shift of any op on this ptr
    uint8_t rw;               // BUF_READ | BUF_WRITTEN flags
    uint8_t is_uniform;       // 1 if the binding's kind is a uniform kind
    uint8_t sealed;           // 1 if the binding is BIND_SEALED_BUF
    int     uniform_slots;    // u32 slot count for uniform bindings, else 0
};

_Bool binding_is_uniform(enum binding_kind);

void resolve_bindings(struct umbra_buf *out,
                      struct buffer_binding const *binding, int bindings,
                      struct umbra_late_binding const *late, int lates);

// True iff the IR stores to any buffer that has an early default.  Such a
// buffer is a shared piece of state across concurrent queue() calls, so the
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
        preamble;      // inst[dispatch_end..preamble) have scope == SCOPE_ROW.
                       // inst[preamble..insts) is the per-batch body.
    int loop_begin,    // Instruction index of op_loop_begin, or -1.
        loop_end;      // Instruction index of op_loop_end,   or -1.
    int vars, bindings;
    int total_bufs;
};
