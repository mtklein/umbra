#pragma once
#include "bb_inst.h"
#include "hash.h"

struct umbra_builder {
    struct bb_inst *inst;
    int             insts, inst_cap;
    struct hash     ht;
};

struct umbra_basic_block {
    struct bb_inst *inst;
    int             insts, preamble;
};
