#pragma once
#include "../include/umbra.h"
#include "hash.h"
#include "op.h"
#include <stdint.h>

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

struct ir_inst {
    enum op op;
    val     x, y, z, w;
    ptr     ptr;
    int     imm;

    // Bookkeeping metadata, doesn't need to be hashed.
    _Bool live, uniform, varying; int :8;
    int   final_id;
};

// A buf registration pinned to a specific ptr handle (.ix).  The resolver
// populates buf[.ix] at dispatch time; how depends on `kind`:
//
//   BIND_EARLY_BUF       caller-owned umbra_buf*, dereferenced each dispatch
//   BIND_EARLY_UNIFORMS  umbra_buf snapshot (ptr + slot count) captured at build
//   BIND_LATE_BUF        umbra_buf supplied at queue() via umbra_late_binding
//   BIND_LATE_UNIFORMS   data pointer supplied at queue(); slot count baked
enum binding_kind {
    BIND_EARLY_BUF,
    BIND_EARLY_UNIFORMS,
    BIND_LATE_BUF,
    BIND_LATE_UNIFORMS,
};

enum { BUF_READ = 1, BUF_WRITTEN = 2 };

// buf and uniforms are kind-disjoint: BIND_EARLY_BUF reads *buf, the two
// UNIFORMS kinds read uniforms (the full snapshot for EARLY_UNIFORMS,
// .count alone for LATE_UNIFORMS), and BIND_LATE_BUF reads neither.
struct buffer_binding {
    enum binding_kind kind;
    int               ix;
    union {
        struct umbra_buf const *buf;
        struct umbra_buf        uniforms;
    };
};

// Per-ptr metadata gathered by a single IR walk; indexed by ptr.bits.
struct buffer_metadata {
    uint8_t shift;        // op_elem_shift of any op on this ptr
    uint8_t rw;           // BUF_READ | BUF_WRITTEN flags
    uint8_t is_uniform;   // 1 if the binding's kind is a uniform kind
    int    :8;
    int     uniform_slots; // u32 slot count for uniform bindings, else 0
};

_Bool binding_is_uniform(enum binding_kind);

void resolve_bindings(struct umbra_buf *out,
                      struct buffer_binding const *binding, int bindings,
                      struct umbra_late_binding const *late, int lates);

// True iff the IR stores to any early-bound buffer.  Used to tag compiled
// programs as thread-safe (along with the backend's own threadsafe bit).
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

    int insts,      // Total instruction count.
        preamble;   // inst[0..preamble) are uniform, [preamble..) varying.
    int loop_begin, // Instruction index of op_loop_begin, or -1.
        loop_end;   // Instruction index of op_loop_end,   or -1.
    int vars, bindings;
    int total_bufs, :32;
};

enum join_policy { JOIN_KEEP_X, JOIN_PREFER_IMM };

struct umbra_flat_ir* flat_ir_resolve(struct umbra_flat_ir const*, enum join_policy);
