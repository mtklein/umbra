#pragma once
#include "../include/umbra.h"
#include "hash.h"
#include "op.h"

typedef union {
    int         bits;
    umbra_val32 v32;
    umbra_val16 v16;
    struct { int id : 30; unsigned chan : 2; };
} val;

typedef union {
    int         bits;
    umbra_ptr16 p16;
    umbra_ptr32 p32;
    umbra_ptr64 p64;
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

// A caller-owned buf registration pinned to a specific ptr handle (.ix).
// Programs auto-populate buf[.ix] at dispatch time so callers don't thread
// these through queue() args.  If buf != NULL, the dispatch reads the current
// contents of *buf (fully mutable between dispatches).  Otherwise the dispatch
// uses the fixed `storage` snapshot captured when registered.
struct umbra_uniform_reg {
    struct umbra_buf const *buf;
    struct umbra_buf        storage;
    int                     ix, pad;
};

struct umbra_builder {
    struct ir_inst           *inst;
    _Bool                    *var_uniform;
    int                       insts, cap;
    struct hash               ht;
    int                       n_vars;
    _Bool                     has_loop, loop_closed, pad0[2];
    umbra_val32               loop_trip;
    umbra_var32        loop_var;
    int                       if_depth, pad1;
    struct umbra_uniform_reg *uniforms;
    int                       n_uniforms, cap_uniforms;
};

struct umbra_flat_ir {
    struct ir_inst           *inst;
    int                       insts,      // Total instruction count.
                              preamble;   // Fence: inst[0..preamble) are uniform, [preamble..) vary.
    int                       loop_begin, // Instruction index of op_loop_begin, or -1.
                              loop_end;   // Instruction index of op_loop_end,   or -1.
    int                       n_vars, pad;
    struct umbra_uniform_reg *uniforms;
    int                       n_uniforms, pad2;
};

enum join_policy { JOIN_KEEP_X, JOIN_PREFER_IMM };

struct umbra_flat_ir* flat_ir_resolve(struct umbra_flat_ir const*, enum join_policy);
