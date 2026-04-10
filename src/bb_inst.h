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

// Type-erased ptr: holds any pointer width, plus bits overlay.
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

    // Compiler bookkeeping, doesn't need to be hashed.
    _Bool live, uniform, varying, pad_;
    int   final_id;
};

