#pragma once
#include <stdint.h>
#include "../include/umbra.h"
#include "op.h"

// Type-erased val: holds either width, plus a bits overlay for hashing/equality.
typedef union {
    int         bits;
    umbra_val32 v32;
    umbra_val16 v16;
    struct { int id : 30; unsigned chan : 2; };
} val;

struct bb_inst {
    enum op op;
    val     x, y, z, w;
    int     ptr, imm;

    // Compiler bookkeeping, doesn't need to be hashed.
    _Bool live, uniform, varying, pad_;
    int   final_id;
};

// TODO: replace with a typedef union { ... } ptr type like val above.
// Ptr encoding: bit 31 = deref flag, bits 0-30 = index.
_Bool ptr_is_deref(int ptr);
int   ptr_ix(int ptr);

