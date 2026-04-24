#pragma once
#include "flat_ir.h"
#include "gpu_buf_cache.h"
#include <stdint.h>

enum { SPIRV_WG_SIZE = 64 };

enum {
    SPIRV_PUSH_VIA_SSBO  = 1,
};

struct spirv_result {
    uint32_t               *spirv;
    struct buffer_metadata *buf;
    int                     spirv_words;
    int                     max_ptr;
    int                     total_bufs;
    int                     push_words;
};

struct spirv_result build_spirv(struct umbra_flat_ir const *ir, int flags);
