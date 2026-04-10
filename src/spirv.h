#pragma once
#include "basic_block.h"
#include <stdint.h>

enum { SPIRV_WG_SIZE = 64 };

struct deref_info { int buf_idx, src_buf, off; };

enum {
    SPIRV_FLOAT_CONTROLS   = 1,
    SPIRV_ALWAYS_16BIT     = 2,  // Always emit Float16 cap + 16-bit types.
    SPIRV_PUSH_VIA_SSBO    = 4,  // Emit push data as an SSBO instead of PushConstant.
    SPIRV_NO_16BIT_TYPES   = 8,  // Emulate 16-bit via u32 shift/mask + UnpackHalf2x16.
};

uint32_t *build_spirv(struct umbra_basic_block const *bb,
                      int flags,
                      int *out_spirv_words,
                      int *out_max_ptr,
                      int *out_total_bufs,
                      int *out_n_deref,
                      struct deref_info **out_deref,
                      int *out_push_words);
