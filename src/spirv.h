#pragma once
#include "flat_ir.h"
#include "gpu_buf_cache.h"
#include <stdint.h>

enum { SPIRV_WG_SIZE = 64 };

enum {
    SPIRV_FLOAT_CONTROLS = 1,
    SPIRV_PUSH_VIA_SSBO  = 4,
    SPIRV_NO_CONTRACT    = 8,  // lower add/sub/mul/square to Fma wrappers
};

struct spirv_result {
    uint32_t         *spirv;
    uint8_t          *buf_rw;
    uint8_t          *buf_shift;
    uint8_t          *buf_is_uniform;
    int               spirv_words;
    int               max_ptr;
    int               total_bufs;
    int               push_words;
};

struct spirv_result build_spirv(struct umbra_flat_ir const *ir, int flags);
