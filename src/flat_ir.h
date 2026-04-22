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
    _Bool live, uniform, varying, pad;
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
struct buffer_binding {
    // TODO: two int pads here, reorder to compact
    // TODO: *buf and uniforms predate binding_kind, do we still need both?
    //       is it maybe enough to track (kind, ix, umbra_buf early)
    enum binding_kind       kind; int :32;
    struct umbra_buf const *buf;
    struct umbra_buf        uniforms;
    int                     ix, pad;
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
    int                       insts, cap;
    struct hash               ht;
    int                       vars;
    _Bool                     has_loop, loop_closed, pad0[2];
    umbra_val32               loop_trip;
    umbra_var32               loop_var;
    int                       if_depth, pad1;
    struct buffer_binding    *binding;
    int                       bindings, cap_bindings;
};

struct umbra_flat_ir {
    struct ir_inst           *inst;
    struct buffer_binding    *binding;

    // Per-ptr buffer metadata, computed at IR construction.  Arrays are
    // sized [total_bufs].  total_bufs == max(ptr.bits) + 1, or 0 if no
    // ptr-bearing op is live.
    uint8_t                  *buf_shift;         // op_elem_shift of any op on this ptr
    uint8_t                  *buf_rw;             // BUF_READ | BUF_WRITTEN flags
    uint8_t                  *buf_is_uniform;     // 1 if binding kind is a uniform kind
    int                      *buf_uniform_slots;  // count of u32 slots for uniform bindings

    int insts,      // Total instruction count.
        preamble;   // inst[0..preamble) are uniform, [preamble..) varying.
    int loop_begin, // Instruction index of op_loop_begin, or -1.
        loop_end;   // Instruction index of op_loop_end,   or -1.
    int vars, bindings;
    int total_bufs, :32;
};

enum join_policy { JOIN_KEEP_X, JOIN_PREFER_IMM };

struct umbra_flat_ir* flat_ir_resolve(struct umbra_flat_ir const*, enum join_policy);
