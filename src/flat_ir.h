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
// populates buf[.ix] at dispatch time by (optionally) reading the early
// default stored here, then letting any matching umbra_late_binding win.
//
//   BIND_BUF                `.buf` is a caller-owned umbra_buf* (or NULL)
//                           dereferenced on every dispatch.
//   BIND_HOST_READONLY_BUF  Same storage as BIND_BUF, but the caller promises
//                           the host bytes don't change after binding -- the
//                           cache may skip fingerprinting and re-uploads.
//   BIND_UNIFORMS           `.uniforms` carries the slot count; `.uniforms.ptr`
//                           is the optional early-default pointer (or NULL).
enum binding_kind {
    BIND_BUF,
    BIND_HOST_READONLY_BUF,
    BIND_UNIFORMS,
};

// The cache reads BUF_HOST_READONLY alongside BUF_READ/BUF_WRITTEN to decide
// whether to skip fingerprinting and re-uploads for an entry.
enum { BUF_READ = 1, BUF_WRITTEN = 2, BUF_HOST_READONLY = 4 };

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
    uint8_t host_readonly;    // 1 if the binding is BIND_HOST_READONLY_BUF
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

    int insts,      // Total instruction count.
        preamble;   // inst[0..preamble) are uniform, [preamble..) varying.
    int loop_begin, // Instruction index of op_loop_begin, or -1.
        loop_end;   // Instruction index of op_loop_end,   or -1.
    int vars, bindings;
    int total_bufs, :32;
};

// TODO: remove op_join() and all associated code like flat_ir_resolve().
//       send everyone down the JOIN_KEEP_X path; remove all _imm op variants
enum join_policy { JOIN_KEEP_X, JOIN_PREFER_IMM };

struct umbra_flat_ir* flat_ir_resolve(struct umbra_flat_ir const*, enum join_policy);
