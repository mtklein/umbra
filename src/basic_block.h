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

struct bb_inst {
    enum op op;
    val     x, y, z, w;
    ptr     ptr;
    int     imm;

    // Bookkeeping metadata, doesn't need to be hashed.
    _Bool live, uniform, varying, pad_;
    int   final_id;
};

struct umbra_builder {
    struct bb_inst *inst;
    int             insts, inst_cap;
    struct hash     ht;
};

struct umbra_basic_block {
    struct bb_inst *inst;
    int             insts, preamble;
};
