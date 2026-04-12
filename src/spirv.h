#pragma once
#include "basic_block.h"
#include "gpu_buf_cache.h"
#include <stdint.h>

enum { SPIRV_WG_SIZE = 64 };

enum {
    SPIRV_FLOAT_CONTROLS = 1,
    SPIRV_PUSH_VIA_SSBO  = 4,
};

struct spirv_result {
    uint32_t         *spirv;
    struct deref_info *deref;
    uint8_t          *buf_rw;
    int               spirv_words;
    int               max_ptr;
    int               total_bufs;
    int               n_deref;
    int               push_words, :32;
};

struct spirv_result build_spirv(struct umbra_basic_block const *bb, int flags);
