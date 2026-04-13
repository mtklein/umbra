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
    struct { int ix : 31, deref : 1; };
} ptr;

struct ir_inst {
    enum op op;
    val     x, y, z, w;
    ptr     ptr;
    int     imm;

    // Bookkeeping metadata, doesn't need to be hashed.
    _Bool live, uniform, varying, pad_;
    int   final_id;
};

struct umbra_builder {
    struct ir_inst *inst;
    _Bool          *var_uniform;
    int             insts, cap;
    struct hash     ht;
    int             n_vars;
    _Bool           has_loop, loop_closed, pad_b_[2];
    umbra_val32     loop_trip;
    umbra_var       loop_var;
    int             if_depth, pad_ifd_;
};

struct umbra_flat_ir {
    struct ir_inst *inst;
    int             insts, preamble;
    int             loop_begin, loop_end;
    int             n_vars, pad_ir_;
};
