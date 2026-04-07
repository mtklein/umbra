#include "assume.h"
#include "bb.h"
#include "uniform_ring.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(__APPLE__) || !defined(__aarch64__) || defined(__wasm__)

struct umbra_backend *umbra_backend_vulkan(void) { return 0; }

#else

#include <vulkan/vulkan.h>

// ---------------------------------------------------------------------------
//  SPIR-V constants.
// ---------------------------------------------------------------------------

enum {
    SpvMagic           = 0x07230203,
    SpvVersion         = 0x00010000, // SPIR-V 1.0
    SpvGenerator       = 0,
    SpvSchema          = 0,

    // Capabilities
    SpvCapabilityShader                      = 1,
    SpvCapabilityFloat16                     = 9,
    SpvCapabilitySignedZeroInfNanPreserve    = 4466,
    SpvCapabilityStorageBuffer16BitAccess    = 4433,

    // Addressing / Memory models
    SpvAddressingModelLogical    = 0,
    SpvMemoryModelGLSL450        = 1,

    // Execution models
    SpvExecutionModelGLCompute = 5,

    // Execution modes
    SpvExecutionModeLocalSize                = 17,
    SpvExecutionModeSignedZeroInfNanPreserve = 4461,

    // Storage classes
    SpvStorageClassUniformConstant = 0,
    SpvStorageClassInput           = 1,
    SpvStorageClassUniform         = 2,
    SpvStorageClassStorageBuffer    = 12,
    SpvStorageClassFunction        = 7,
    SpvStorageClassPushConstant    = 9,

    // Decorations
    SpvDecorationBlock            = 2,
    SpvDecorationBufferBlock      = 3,
    SpvDecorationArrayStride      = 6,
    SpvDecorationBuiltIn          = 11,
    SpvDecorationBinding          = 33,
    SpvDecorationDescriptorSet    = 34,
    SpvDecorationOffset           = 35,
    SpvDecorationNonWritable      = 24,
    SpvDecorationNonReadable      = 25,

    // Built-ins
    SpvBuiltInGlobalInvocationId = 28,

    // Opcodes (shifted left by 16 gives the high half)
    SpvOpNop                  = 0,
    SpvOpExtInstImport        = 11,
    SpvOpExtInst              = 12,
    SpvOpMemoryModel          = 14,
    SpvOpEntryPoint           = 15,
    SpvOpExecutionMode        = 16,
    SpvOpCapability            = 17,
    SpvOpTypeVoid             = 19,
    SpvOpTypeBool             = 20,
    SpvOpTypeInt              = 21,
    SpvOpTypeFloat            = 22,
    SpvOpTypeVector           = 23,
    SpvOpTypeArray            = 28,
    SpvOpTypeRuntimeArray     = 29,
    SpvOpTypeStruct           = 30,
    SpvOpTypePointer          = 32,
    SpvOpTypeFunction         = 33,
    SpvOpConstant             = 43,
    SpvOpConstantComposite    = 44,
    SpvOpConstantTrue         = 41,
    SpvOpConstantFalse        = 42,
    SpvOpFunction             = 54,
    SpvOpFunctionParameter    = 55,
    SpvOpFunctionEnd          = 56,
    SpvOpVariable             = 59,
    SpvOpLoad                 = 61,
    SpvOpStore                = 62,
    SpvOpAccessChain          = 65,
    SpvOpDecorate             = 71,
    SpvOpMemberDecorate       = 72,
    SpvOpDecorationGroup      = 73,
    SpvOpCompositeExtract     = 81,
    SpvOpCompositeConstruct   = 80,
    SpvOpConvertFToS          = 110,
    SpvOpConvertSToF          = 111,
    SpvOpConvertFToU          = 109,
    SpvOpConvertUToF          = 112,
    SpvOpUConvert             = 113,
    SpvOpSConvert             = 114,
    SpvOpFConvert             = 115,
    SpvOpBitcast              = 124,
    SpvOpSNegate              = 126,
    SpvOpFNegate              = 127,
    SpvOpIAdd                 = 128,
    SpvOpFAdd                 = 129,
    SpvOpISub                 = 130,
    SpvOpFSub                 = 131,
    SpvOpIMul                 = 132,
    SpvOpFMul                 = 133,
    SpvOpUDiv                 = 134,
    SpvOpSDiv                 = 135,
    SpvOpFDiv                 = 136,
    SpvOpUMod                 = 137,
    SpvOpSMod                 = 139,
    SpvOpFMod                 = 141,
    SpvOpShiftRightLogical    = 194,
    SpvOpShiftRightArithmetic = 195,
    SpvOpShiftLeftLogical     = 196,
    SpvOpBitwiseOr            = 197,
    SpvOpBitwiseXor           = 198,
    SpvOpBitwiseAnd           = 199,
    SpvOpLogicalOr            = 166,
    SpvOpLogicalAnd           = 167,
    SpvOpLogicalNot           = 168,
    SpvOpSelect               = 169,
    SpvOpIEqual               = 170,
    SpvOpINotEqual            = 171,
    SpvOpUGreaterThan         = 172,
    SpvOpSGreaterThan         = 173,
    SpvOpUGreaterThanEqual    = 174,
    SpvOpSGreaterThanEqual    = 175,
    SpvOpULessThan            = 176,
    SpvOpSLessThan            = 177,
    SpvOpULessThanEqual       = 178,
    SpvOpSLessThanEqual       = 179,
    SpvOpFOrdEqual            = 180,
    SpvOpFOrdNotEqual         = 182,
    SpvOpFOrdLessThan         = 184,
    SpvOpFOrdGreaterThan      = 186,
    SpvOpFOrdLessThanEqual    = 188,
    SpvOpFOrdGreaterThanEqual = 190,
    SpvOpLabel                = 248,
    SpvOpBranch               = 249,
    SpvOpBranchConditional    = 250,
    SpvOpReturn               = 253,
    SpvOpReturnValue          = 254,
    SpvOpFunctionCall         = 57,
    SpvOpKill                 = 252,

    // GLSL.std.450 extended instructions
    GLSLstd450Round = 1,
    GLSLstd450RoundEven = 2,
    GLSLstd450Trunc = 3,
    GLSLstd450FAbs  = 4,
    GLSLstd450Floor = 8,
    GLSLstd450Ceil  = 9,
    GLSLstd450Sqrt  = 31,
    GLSLstd450FMin  = 37,
    GLSLstd450FMax  = 40,
    GLSLstd450FMix  = 46,
    GLSLstd450Fma   = 50,

    WG_SIZE = 64,
};

// ---------------------------------------------------------------------------
//  SPIR-V binary buffer.
// ---------------------------------------------------------------------------

typedef struct {
    uint32_t *buf;
    int       len, cap;
} Spv;

static void spv_grow(Spv *s) {
    s->cap = s->cap ? 2 * s->cap : 1024;
    s->buf = realloc(s->buf, (size_t)s->cap * sizeof(uint32_t));
}

static void spv_word(Spv *s, uint32_t w) {
    if (s->len >= s->cap) { spv_grow(s); }
    s->buf[s->len++] = w;
}

static void spv_op(Spv *s, int opcode, int word_count) {
    spv_word(s, (uint32_t)((word_count << 16) | (opcode & 0xFFFF)));
}

// ---------------------------------------------------------------------------
//  Backend state.
// ---------------------------------------------------------------------------

struct buf_cache_entry {
    VkBuffer       buf;
    VkDeviceMemory mem;
    void          *mapped;
    void          *host;
    VkDeviceSize   size;
    _Bool          nocopy, writable; int :16, :32;
};

enum { VK_N_FRAMES = 2 };

struct vk_backend {
    struct umbra_backend  base;
    VkInstance            instance;
    VkPhysicalDevice      phys;
    VkDevice              device;
    VkQueue               queue;
    VkCommandPool         cmd_pool;
    uint32_t              queue_family;
    uint32_t              mem_type_host;
    uint32_t              mem_type_host_import; int :32;
    PFN_vkGetMemoryHostPointerPropertiesEXT get_host_props;
    VkDeviceSize          host_import_align;
    VkCommandBuffer       batch_cmd;                       // currently-encoding cmdbuf, or NULL
    VkCommandBuffer       frame_committed[VK_N_FRAMES];    // last committed cmdbuf per frame
    VkFence               frame_fences   [VK_N_FRAMES];    // signals when frame_committed[i] is done
    struct copyback      *batch_copies;
    int                   batch_n_copies, batch_copies_cap;
    struct buf_cache_entry *batch_cache;
    int                     batch_cache_n, batch_cache_cap;
    struct uniform_ring   uni_rings[VK_N_FRAMES];
    int                   cur_frame, :32;
    _Bool                 batch_has_dispatch; int :24, :32;
};

static int find_compute_queue(VkPhysicalDevice phys) {
    uint32_t n = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &n, 0);
    VkQueueFamilyProperties *props = calloc(n, sizeof *props);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &n, props);
    int idx = -1;
    for (uint32_t i = 0; i < n; i++) {
        if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { idx = (int)i; break; }
    }
    free(props);
    return idx;
}

static uint32_t find_host_memory(VkPhysicalDevice phys) {
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(phys, &mem);
    for (uint32_t i = 0; i < mem.memoryTypeCount; i++) {
        VkMemoryPropertyFlags f = mem.memoryTypes[i].propertyFlags;
        if ((f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            return i;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
//  Deref info — same idea as Metal backend.
// ---------------------------------------------------------------------------

struct deref_info { int buf_idx, src_buf, off; };

// ---------------------------------------------------------------------------
//  Copyback tracking.
// ---------------------------------------------------------------------------

struct copyback {
    void *host;
    void *mapped;
    size_t bytes;
};

// ---------------------------------------------------------------------------
//  Program.
// ---------------------------------------------------------------------------

struct vk_program {
    struct umbra_program base;
    struct vk_backend *be;
    VkShaderModule          shader;
    VkDescriptorSetLayout   ds_layout;
    VkPipelineLayout        pipe_layout;
    VkPipeline              pipeline;

    int max_ptr;
    int total_bufs;
    int n_deref;
    int push_words;

    struct deref_info *deref;

    uint32_t *spirv;
    int       spirv_len, :32;
};

// ---------------------------------------------------------------------------
//  SPIR-V codegen.
// ---------------------------------------------------------------------------

// We build SPIR-V in multiple sections that get concatenated at the end:
//   capabilities, extensions, ext_import, memory_model, entry_point,
//   execution_mode, decorations, types+constants, global_vars, function_body

typedef struct {
    Spv caps;
    Spv exts;
    Spv ext_import;
    Spv mem_model;
    Spv entry;
    Spv exec_mode;
    Spv decor;
    Spv types;
    Spv globals;
    Spv func;

    uint32_t next_id;

    // Well-known type IDs.
    uint32_t t_void, t_bool, t_u32, t_i32, t_f32, t_f16, t_u16;
    uint32_t t_uvec3;
    uint32_t t_fn_void;
    uint32_t t_ptr_input_uvec3;
    uint32_t t_ptr_ssbo_u32;
    uint32_t t_ptr_ssbo_u16;
    uint32_t t_ptr_push_u32;
    uint32_t t_rta_u32;             // RuntimeArray of u32
    uint32_t t_rta_u16;             // RuntimeArray of u16
    uint32_t t_struct_rta_u32;      // struct { RuntimeArray<u32> }
    uint32_t t_struct_rta_u16;      // struct { RuntimeArray<u16> }
    uint32_t t_ptr_ssbo_struct;     // pointer to struct { RuntimeArray<u32> }
    uint32_t t_ptr_ssbo_struct_u16; // pointer to struct { RuntimeArray<u16> }
    uint32_t t_ptr_func_u32;
    uint32_t t_ptr_func_f32;

    // GLSL.std.450 import.
    uint32_t ext_glsl, :32;

    // Global variable IDs.
    uint32_t *v_ssbo;      // one per buffer (total_bufs)
    uint32_t v_global_id;  // gl_GlobalInvocationID
    uint32_t v_push;       // push constant block variable

    // Push constant layout:
    //   [0] = w, [1] = x0, [2] = y0,
    //   [3..3+total_bufs-1] = buf_szs, [3+total_bufs..3+2*total_bufs-1] = buf_rbs
    int total_bufs;
    int push_words;

    // Constant cache for small integer constants.
    uint32_t c_0, c_1, c_2, c_3, c_4, c_8, c_16;
    uint32_t c_0f;  // float 0.0
    uint32_t c_allones, :32;  // 0xFFFFFFFF

    // Per-BB-instruction result IDs. Some instructions produce multiple results
    // (e.g. load_16x4 produces 4 channels).
    uint32_t *val;        // val[i] = SPIR-V ID for bb_inst i (channel 0)
    uint32_t *val_1;      // channel 1 for 16x4 loads
    uint32_t *val_2;      // channel 2
    uint32_t *val_3;      // channel 3

    // Track which values are float type (vs uint).
    _Bool *is_f;

    // Map from bb_inst index -> deref buffer index.
    int *deref_buf;

    // Per-buffer flag: true if the buffer needs 16-bit typed access.
    _Bool *buf_is_16;
    int    has_16, :32;

    // Constant deduplication cache.
    struct { uint32_t type, value, id; } *const_cache;
    int n_consts, consts_cap;
} SpvBuilder;

static uint32_t spv_id(SpvBuilder *b) { return b->next_id++; }

static uint32_t spv_const(SpvBuilder *b, uint32_t type, uint32_t bits) {
    for (int i = 0; i < b->n_consts; i++) {
        if (b->const_cache[i].type == type && b->const_cache[i].value == bits) {
            return b->const_cache[i].id;
        }
    }
    uint32_t id = spv_id(b);
    spv_op(&b->types, SpvOpConstant, 4);
    spv_word(&b->types, type);
    spv_word(&b->types, id);
    spv_word(&b->types, bits);
    if (b->n_consts >= b->consts_cap) {
        b->consts_cap = b->consts_cap ? 2 * b->consts_cap : 64;
        b->const_cache = realloc(b->const_cache,
                                 (size_t)b->consts_cap * sizeof *b->const_cache);
    }
    b->const_cache[b->n_consts].type  = type;
    b->const_cache[b->n_consts].value = bits;
    b->const_cache[b->n_consts].id    = id;
    b->n_consts++;
    return id;
}

static uint32_t spv_const_u32(SpvBuilder *b, uint32_t value) {
    return spv_const(b, b->t_u32, value);
}

static uint32_t spv_const_i32(SpvBuilder *b, int32_t value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    return spv_const(b, b->t_i32, bits);
}

static uint32_t spv_const_f32(SpvBuilder *b, float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    return spv_const(b, b->t_f32, bits);
}

// Emit a bitcast: result = bitcast<dst_type>(src).
static uint32_t spv_bitcast(SpvBuilder *b, uint32_t dst_type, uint32_t src) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpBitcast, 4);
    spv_word(&b->func, dst_type);
    spv_word(&b->func, id);
    spv_word(&b->func, src);
    return id;
}

// Emit: result = OpAccessChain ptr_type base [indices...]
// (spv_access_chain_1 removed — we always need at least 2 indices for our layouts.)

static uint32_t spv_access_chain_2(SpvBuilder *b, uint32_t ptr_type,
                                    uint32_t base, uint32_t ix0, uint32_t ix1) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpAccessChain, 6);
    spv_word(&b->func, ptr_type);
    spv_word(&b->func, id);
    spv_word(&b->func, base);
    spv_word(&b->func, ix0);
    spv_word(&b->func, ix1);
    return id;
}

static uint32_t spv_load(SpvBuilder *b, uint32_t type, uint32_t ptr) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpLoad, 4);
    spv_word(&b->func, type);
    spv_word(&b->func, id);
    spv_word(&b->func, ptr);
    return id;
}

static void spv_store(SpvBuilder *b, uint32_t ptr, uint32_t value) {
    spv_op(&b->func, SpvOpStore, 3);
    spv_word(&b->func, ptr);
    spv_word(&b->func, value);
}

// Binary arithmetic/logic ops (result = op(x, y))
static uint32_t spv_binop(SpvBuilder *b, int opcode, uint32_t type,
                           uint32_t x, uint32_t y) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, opcode, 5);
    spv_word(&b->func, type);
    spv_word(&b->func, id);
    spv_word(&b->func, x);
    spv_word(&b->func, y);
    return id;
}

// Unary op (e.g. negate)
static uint32_t spv_unop(SpvBuilder *b, int opcode, uint32_t type, uint32_t x) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, opcode, 4);
    spv_word(&b->func, type);
    spv_word(&b->func, id);
    spv_word(&b->func, x);
    return id;
}

// GLSL.std.450 extended instruction (1 operand).
static uint32_t spv_glsl_1(SpvBuilder *b, uint32_t type, uint32_t ext_op, uint32_t x) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpExtInst, 6);
    spv_word(&b->func, type);
    spv_word(&b->func, id);
    spv_word(&b->func, b->ext_glsl);
    spv_word(&b->func, ext_op);
    spv_word(&b->func, x);
    return id;
}

// GLSL.std.450 extended instruction (2 operands).
static uint32_t spv_glsl_2(SpvBuilder *b, uint32_t type, uint32_t ext_op,
                            uint32_t x, uint32_t y) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpExtInst, 7);
    spv_word(&b->func, type);
    spv_word(&b->func, id);
    spv_word(&b->func, b->ext_glsl);
    spv_word(&b->func, ext_op);
    spv_word(&b->func, x);
    spv_word(&b->func, y);
    return id;
}

// GLSL.std.450 extended instruction (3 operands).
static uint32_t spv_glsl_3(SpvBuilder *b, uint32_t type, uint32_t ext_op,
                            uint32_t x, uint32_t y, uint32_t z) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpExtInst, 8);
    spv_word(&b->func, type);
    spv_word(&b->func, id);
    spv_word(&b->func, b->ext_glsl);
    spv_word(&b->func, ext_op);
    spv_word(&b->func, x);
    spv_word(&b->func, y);
    spv_word(&b->func, z);
    return id;
}

// OpSelect: result = cond ? a : b
static uint32_t spv_select(SpvBuilder *b, uint32_t type,
                            uint32_t cond, uint32_t a, uint32_t val_b) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpSelect, 6);
    spv_word(&b->func, type);
    spv_word(&b->func, id);
    spv_word(&b->func, cond);
    spv_word(&b->func, a);
    spv_word(&b->func, val_b);
    return id;
}

static uint32_t spv_composite_extract(SpvBuilder *b, uint32_t type,
                                       uint32_t composite, uint32_t index) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpCompositeExtract, 5);
    spv_word(&b->func, type);
    spv_word(&b->func, id);
    spv_word(&b->func, composite);
    spv_word(&b->func, index);
    return id;
}

// Helper: ensure a value is uint. If it's float, bitcast it.
static uint32_t as_u32(SpvBuilder *b, uint32_t val, int inst_id) {
    if (b->is_f[inst_id]) {
        return spv_bitcast(b, b->t_u32, val);
    }
    return val;
}

// Helper: ensure a value is float. If it's uint, bitcast it.
static uint32_t as_f32(SpvBuilder *b, uint32_t val, int inst_id) {
    if (!b->is_f[inst_id]) {
        return spv_bitcast(b, b->t_f32, val);
    }
    return val;
}

// Get the SPIR-V result ID for a bb_inst operand, handling channel extraction.
static uint32_t get_val(SpvBuilder *b, val_ v) {
    int id = (int)v.id;
    int ch = (int)v.chan;
    switch (ch) {
        case 0: return b->val[id];
        case 1: return b->val_1[id];
        case 2: return b->val_2[id];
        case 3: return b->val_3[id];
    }
    __builtin_unreachable();
}

static int get_id(val_ v) { return (int)v.id; }

// Resolve a pointer index: if negative, it's a deref reference.
static int resolve_ptr(SpvBuilder *b, struct bb_inst const *inst) {
    return ptr_is_deref(inst->ptr) ? b->deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
}

// Load a push constant at a given word offset.
// Push layout is struct { uint data[N]; }, so we need two indices:
// member 0, then array element.
static uint32_t load_push_u32(SpvBuilder *b, uint32_t offset_id) {
    uint32_t ptr = spv_access_chain_2(b, b->t_ptr_push_u32, b->v_push, b->c_0, offset_id);
    return spv_load(b, b->t_u32, ptr);
}

// Load from SSBO[buf_idx] at element_index.
static uint32_t load_ssbo_u32(SpvBuilder *b, int buf_idx, uint32_t elem_idx) {
    uint32_t ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u32,
                                       b->v_ssbo[buf_idx],
                                       b->c_0, elem_idx);
    return spv_load(b, b->t_u32, ptr);
}

// Store to SSBO[buf_idx] at element_index.
static void store_ssbo_u32(SpvBuilder *b, int buf_idx, uint32_t elem_idx, uint32_t value) {
    uint32_t ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u32,
                                       b->v_ssbo[buf_idx],
                                       b->c_0, elem_idx);
    spv_store(b, ptr, value);
}

// Load u16 from SSBO[buf_idx] at element_index, zero-extended to u32.
static uint32_t load_ssbo_u16(SpvBuilder *b, int buf_idx, uint32_t elem_idx) {
    uint32_t ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u16,
                                       b->v_ssbo[buf_idx],
                                       b->c_0, elem_idx);
    uint32_t raw = spv_load(b, b->t_u16, ptr);
    // Zero-extend u16 -> u32.
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpUConvert, 4);
    spv_word(&b->func, b->t_u32);
    spv_word(&b->func, id);
    spv_word(&b->func, raw);
    return id;
}

// Store u16 to SSBO[buf_idx] at element_index, truncating from u32.
static void store_ssbo_u16(SpvBuilder *b, int buf_idx, uint32_t elem_idx, uint32_t value) {
    // Truncate u32 -> u16.
    uint32_t val16 = spv_id(b);
    spv_op(&b->func, SpvOpUConvert, 4);
    spv_word(&b->func, b->t_u16);
    spv_word(&b->func, val16);
    spv_word(&b->func, value);
    uint32_t ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u16,
                                       b->v_ssbo[buf_idx],
                                       b->c_0, elem_idx);
    spv_store(b, ptr, val16);
}

// Compute linear address for row-structured buffer:
//   addr = y * (row_bytes/stride) + x
// Where stride = 4 for u32, 2 for u16.
static uint32_t compute_addr(SpvBuilder *b, uint32_t x, uint32_t y,
                              int buf_idx, uint32_t stride_shift) {
    // buf_rbs[buf_idx] is at push offset 3 + total_bufs + buf_idx
    uint32_t rb_off = spv_const_u32(b, (uint32_t)(3 + b->total_bufs + buf_idx));
    uint32_t rb = load_push_u32(b, rb_off);
    // row_bytes >> stride_shift = number of elements per row
    uint32_t elems_per_row = spv_binop(b, SpvOpShiftRightLogical, b->t_u32,
                                        rb, stride_shift);
    uint32_t row_off = spv_binop(b, SpvOpIMul, b->t_u32, y, elems_per_row);
    return spv_binop(b, SpvOpIAdd, b->t_u32, row_off, x);
}

// Compute safe gather index: clamp to [0, count-1], return 0 for OOB.
// buf_szs[buf_idx] is at push offset 3 + buf_idx.
// elem_bytes = 4 for u32, 2 for u16.
// Returns (clamped_index, oob_mask) where oob_mask is 0xFFFFFFFF for in-bounds, 0 for OOB.
static void gather_safe(SpvBuilder *b, uint32_t ix_val, int buf_idx,
                         uint32_t elem_shift,
                         uint32_t *out_idx, uint32_t *out_mask) {
    uint32_t sz_off = spv_const_u32(b, (uint32_t)(3 + buf_idx));
    uint32_t sz = load_push_u32(b, sz_off);
    uint32_t count = spv_binop(b, SpvOpShiftRightLogical, b->t_u32, sz, elem_shift);

    // max_idx = max(count-1, 0)
    uint32_t count_minus_1 = spv_binop(b, SpvOpISub, b->t_u32, count, b->c_1);
    // clamp: UMin(UMax(ix, 0), max_idx) — but ix is uint so UMax(ix,0)=ix
    // Actually we need to handle negative indices (signed). Use SMax then UMin.
    // Use GLSL.std.450 UMin and SMax:
    // clamped = UMin(ix, count_minus_1)  — this clamps the upper bound
    uint32_t clamped = spv_glsl_2(b, b->t_u32, 38 /*UMin*/, ix_val, count_minus_1);

    // OOB check: ix >= 0 && ix < count  (signed comparison for < 0)
    // in_bounds = (ix >=s 0) && (ix <u count)
    uint32_t ge_zero = spv_binop(b, SpvOpSGreaterThanEqual, b->t_bool, ix_val,
                                  b->c_0);
    uint32_t lt_count = spv_binop(b, SpvOpULessThan, b->t_bool, ix_val, count);
    uint32_t in_bounds = spv_binop(b, SpvOpLogicalAnd, b->t_bool, ge_zero, lt_count);
    // mask = in_bounds ? 0xFFFFFFFF : 0
    *out_mask = spv_select(b, b->t_u32, in_bounds, b->c_allones, b->c_0);
    *out_idx = clamped;
}

// fp16 <-> fp32 via native OpFConvert (requires Float16 capability).
// f32_from_f16: convert a u32 holding a fp16 bit pattern in low 16 bits to float.
static uint32_t spv_f16_to_f32(SpvBuilder *b, uint32_t u32_val) {
    uint32_t u16_val = spv_unop(b, SpvOpUConvert, b->t_u16, u32_val);
    uint32_t h       = spv_bitcast(b, b->t_f16, u16_val);
    return spv_unop(b, SpvOpFConvert, b->t_f32, h);
}

// f16_from_f32: convert float to u32 holding fp16 bit pattern in low 16 bits.
static uint32_t spv_f32_to_f16(SpvBuilder *b, uint32_t f32_val) {
    uint32_t h   = spv_unop(b, SpvOpFConvert, b->t_f16, f32_val);
    uint32_t u16 = spv_bitcast(b, b->t_u16, h);
    return spv_unop(b, SpvOpUConvert, b->t_u32, u16);
}

static _Bool produces_float(enum op op) {
    return op == op_add_f32     || op == op_sub_f32
        || op == op_mul_f32     || op == op_div_f32
        || op == op_min_f32     || op == op_max_f32
        || op == op_sqrt_f32    || op == op_abs_f32
        || op == op_round_f32   || op == op_floor_f32  || op == op_ceil_f32
        || op == op_fma_f32     || op == op_fms_f32
        || op == op_add_f32_imm || op == op_sub_f32_imm
        || op == op_mul_f32_imm || op == op_div_f32_imm
        || op == op_min_f32_imm || op == op_max_f32_imm
        || op == op_f32_from_i32
        || op == op_f32_from_f16
        || op == op_sample_32;
}

// Build the full SPIR-V binary for a basic block.
static uint32_t *build_spirv(struct umbra_basic_block const *bb,
                              int *out_spirv_len,
                              int *out_max_ptr,
                              int *out_total_bufs,
                              int *out_n_deref,
                              struct deref_info **out_deref,
                              int *out_push_words) {
    SpvBuilder B;
    memset(&B, 0, sizeof B);
    B.next_id = 1; // 0 is reserved

    // --- Analyze the BB to find buffer usage. ---
    int max_ptr = -1;
    for (int i = 0; i < bb->insts; i++) {
        if (has_ptr(bb->inst[i].op) && bb->inst[i].ptr >= 0) {
            if (bb->inst[i].ptr > max_ptr) {
                max_ptr = bb->inst[i].ptr;
            }
        }
    }
    *out_max_ptr = max_ptr;

    int *deref_buf = calloc((size_t)(bb->insts + 1), sizeof *deref_buf);
    B.deref_buf = deref_buf;
    int next_buf = max_ptr + 1;
    for (int i = 0; i < bb->insts; i++) {
        if (bb->inst[i].op == op_deref_ptr) {
            deref_buf[i] = next_buf++;
        }
    }
    int total_bufs = next_buf;
    int n_deref    = total_bufs - max_ptr - 1;
    *out_total_bufs = total_bufs;
    *out_n_deref = n_deref;
    B.total_bufs = total_bufs;

    struct deref_info *di = calloc((size_t)(n_deref ? n_deref : 1), sizeof *di);
    {
        int d = 0;
        for (int i = 0; i < bb->insts; i++) {
            if (bb->inst[i].op == op_deref_ptr) {
                di[d].buf_idx  = deref_buf[i];
                di[d].src_buf  = bb->inst[i].ptr;
                di[d].off = bb->inst[i].imm;
                d++;
            }
        }
    }
    *out_deref = di;

    // --- Scan for 16-bit buffer access. ---
    B.buf_is_16 = calloc((size_t)(total_bufs + 1), sizeof *B.buf_is_16);
    for (int i = 0; i < bb->insts; i++) {
        enum op op = bb->inst[i].op;
        if (op == op_load_16 || op == op_store_16 || op == op_gather_16
         || op == op_load_16x4_planar || op == op_store_16x4_planar) {
            int p = ptr_is_deref(bb->inst[i].ptr) ? deref_buf[ptr_ix(bb->inst[i].ptr)]
                                                   : bb->inst[i].ptr;
            B.buf_is_16[p] = 1;
            B.has_16 = 1;
        }
    }

    // Push constant layout: w, x0, y0, buf_szs[total_bufs], buf_rbs[total_bufs].
    // User uniforms (buf[0]) go through the per-batch uniform ring as a
    // storage buffer at descriptor binding 0; only this small backend-side
    // metadata rides in push constants.
    int push_words = 3 + 2 * total_bufs;
    *out_push_words = push_words;
    B.push_words = push_words;

    // --- Capabilities ---
    spv_op(&B.caps, SpvOpCapability, 2);
    spv_word(&B.caps, SpvCapabilityShader);

    spv_op(&B.caps, SpvOpCapability, 2);
    spv_word(&B.caps, SpvCapabilityFloat16);

    spv_op(&B.caps, SpvOpCapability, 2);
    spv_word(&B.caps, SpvCapabilitySignedZeroInfNanPreserve);

    // SPV_KHR_float_controls: SignedZeroInfNanPreserve for float32 makes
    // MoltenVK/SPIRV-Cross clear NotNaN|NotInf|NSZ from fpFastMathFlags,
    // which produces MTLMathModeSafe + precise:: transcendentals — matching
    // our Metal backend.
    {
        char const name[] = "SPV_KHR_float_controls";
        int name_words = ((int)sizeof(name) + 3) / 4;
        spv_op(&B.exts, 10 /*OpExtension*/, 1 + name_words);
        for (int w = 0; w < name_words; w++) {
            uint32_t word = 0;
            for (int b2 = 0; b2 < 4 && w * 4 + b2 < (int)sizeof(name); b2++) {
                word |= (uint32_t)(unsigned char)name[w * 4 + b2] << (b2 * 8);
            }
            spv_word(&B.exts, word);
        }
    }

    if (B.has_16) {
        spv_op(&B.caps, SpvOpCapability, 2);
        spv_word(&B.caps, SpvCapabilityStorageBuffer16BitAccess);

        // OpExtension "SPV_KHR_16bit_storage"
        {
            char const name[] = "SPV_KHR_16bit_storage";
            int name_words = ((int)sizeof(name) + 3) / 4;
            spv_op(&B.exts, 10 /*OpExtension*/, 1 + name_words);
            for (int w = 0; w < name_words; w++) {
                uint32_t word = 0;
                for (int b2 = 0; b2 < 4 && w * 4 + b2 < (int)sizeof(name); b2++) {
                    word |= (uint32_t)(unsigned char)name[w * 4 + b2] << (b2 * 8);
                }
                spv_word(&B.exts, word);
            }
        }
    }

    // --- Extension import (GLSL.std.450) ---
    B.ext_glsl = spv_id(&B);
    {
        char const name[] = "GLSL.std.450";
        int name_words = ((int)sizeof(name) + 3) / 4; // includes null terminator
        spv_op(&B.ext_import, SpvOpExtInstImport, 2 + name_words);
        spv_word(&B.ext_import, B.ext_glsl);
        // Write name as words.
        for (int w = 0; w < name_words; w++) {
            uint32_t word = 0;
            for (int b2 = 0; b2 < 4 && w * 4 + b2 < (int)sizeof(name); b2++) {
                word |= (uint32_t)(unsigned char)name[w * 4 + b2] << (b2 * 8);
            }
            spv_word(&B.ext_import, word);
        }
    }

    // --- Memory model ---
    spv_op(&B.mem_model, SpvOpMemoryModel, 3);
    spv_word(&B.mem_model, SpvAddressingModelLogical);
    spv_word(&B.mem_model, SpvMemoryModelGLSL450);

    // --- Types ---
    B.t_void = spv_id(&B);
    spv_op(&B.types, SpvOpTypeVoid, 2);
    spv_word(&B.types, B.t_void);

    B.t_bool = spv_id(&B);
    spv_op(&B.types, SpvOpTypeBool, 2);
    spv_word(&B.types, B.t_bool);

    B.t_u32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeInt, 4);
    spv_word(&B.types, B.t_u32);
    spv_word(&B.types, 32);
    spv_word(&B.types, 0); // unsigned

    B.t_i32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeInt, 4);
    spv_word(&B.types, B.t_i32);
    spv_word(&B.types, 32);
    spv_word(&B.types, 1); // signed

    B.t_f32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeFloat, 3);
    spv_word(&B.types, B.t_f32);
    spv_word(&B.types, 32);

    B.t_u16 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeInt, 4);
    spv_word(&B.types, B.t_u16);
    spv_word(&B.types, 16);
    spv_word(&B.types, 0); // unsigned

    B.t_f16 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeFloat, 3);
    spv_word(&B.types, B.t_f16);
    spv_word(&B.types, 16);

    B.t_uvec3 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeVector, 4);
    spv_word(&B.types, B.t_uvec3);
    spv_word(&B.types, B.t_u32);
    spv_word(&B.types, 3);

    B.t_fn_void = spv_id(&B);
    spv_op(&B.types, SpvOpTypeFunction, 3);
    spv_word(&B.types, B.t_fn_void);
    spv_word(&B.types, B.t_void);

    // Pointer to Input uvec3 (for gl_GlobalInvocationID).
    B.t_ptr_input_uvec3 = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, B.t_ptr_input_uvec3);
    spv_word(&B.types, SpvStorageClassInput);
    spv_word(&B.types, B.t_uvec3);

    // RuntimeArray<u32>
    B.t_rta_u32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeRuntimeArray, 3);
    spv_word(&B.types, B.t_rta_u32);
    spv_word(&B.types, B.t_u32);

    // Decorate RuntimeArray stride = 4
    spv_op(&B.decor, SpvOpDecorate, 4);
    spv_word(&B.decor, B.t_rta_u32);
    spv_word(&B.decor, SpvDecorationArrayStride);
    spv_word(&B.decor, 4);

    // struct { RuntimeArray<u32> } — one per buffer
    B.t_struct_rta_u32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeStruct, 3);
    spv_word(&B.types, B.t_struct_rta_u32);
    spv_word(&B.types, B.t_rta_u32);

    // Decorate struct as BufferBlock (SPIR-V 1.0 way for SSBO).
    spv_op(&B.decor, SpvOpDecorate, 3);
    spv_word(&B.decor, B.t_struct_rta_u32);
    spv_word(&B.decor, SpvDecorationBufferBlock);

    // MemberDecorate offset 0.
    spv_op(&B.decor, SpvOpMemberDecorate, 5);
    spv_word(&B.decor, B.t_struct_rta_u32);
    spv_word(&B.decor, 0);
    spv_word(&B.decor, SpvDecorationOffset);
    spv_word(&B.decor, 0);

    // Pointer to SSBO struct.
    B.t_ptr_ssbo_struct = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, B.t_ptr_ssbo_struct);
    spv_word(&B.types, SpvStorageClassUniform); // BufferBlock uses Uniform SC in SPIR-V 1.0
    spv_word(&B.types, B.t_struct_rta_u32);

    // Pointer to u32 in SSBO.
    B.t_ptr_ssbo_u32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, B.t_ptr_ssbo_u32);
    spv_word(&B.types, SpvStorageClassUniform);
    spv_word(&B.types, B.t_u32);

    // 16-bit storage types (only when needed).
    if (B.has_16) {
        B.t_rta_u16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeRuntimeArray, 3);
        spv_word(&B.types, B.t_rta_u16);
        spv_word(&B.types, B.t_u16);

        spv_op(&B.decor, SpvOpDecorate, 4);
        spv_word(&B.decor, B.t_rta_u16);
        spv_word(&B.decor, SpvDecorationArrayStride);
        spv_word(&B.decor, 2);

        B.t_struct_rta_u16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeStruct, 3);
        spv_word(&B.types, B.t_struct_rta_u16);
        spv_word(&B.types, B.t_rta_u16);

        spv_op(&B.decor, SpvOpDecorate, 3);
        spv_word(&B.decor, B.t_struct_rta_u16);
        spv_word(&B.decor, SpvDecorationBufferBlock);

        spv_op(&B.decor, SpvOpMemberDecorate, 5);
        spv_word(&B.decor, B.t_struct_rta_u16);
        spv_word(&B.decor, 0);
        spv_word(&B.decor, SpvDecorationOffset);
        spv_word(&B.decor, 0);

        B.t_ptr_ssbo_struct_u16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_ssbo_struct_u16);
        spv_word(&B.types, SpvStorageClassUniform);
        spv_word(&B.types, B.t_struct_rta_u16);

        B.t_ptr_ssbo_u16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_ssbo_u16);
        spv_word(&B.types, SpvStorageClassUniform);
        spv_word(&B.types, B.t_u16);
    }

    // Push constant block: struct { uint data[push_words]; }
    // We model it as struct { uint[push_words] }.
    uint32_t t_arr_push = spv_id(&B);
    uint32_t c_push_len = spv_const_u32(&B, (uint32_t)push_words);
    spv_op(&B.types, SpvOpTypeArray, 4);
    spv_word(&B.types, t_arr_push);
    spv_word(&B.types, B.t_u32);
    spv_word(&B.types, c_push_len);

    // Decorate array stride.
    spv_op(&B.decor, SpvOpDecorate, 4);
    spv_word(&B.decor, t_arr_push);
    spv_word(&B.decor, SpvDecorationArrayStride);
    spv_word(&B.decor, 4);

    uint32_t t_push_struct = spv_id(&B);
    spv_op(&B.types, SpvOpTypeStruct, 3);
    spv_word(&B.types, t_push_struct);
    spv_word(&B.types, t_arr_push);

    spv_op(&B.decor, SpvOpDecorate, 3);
    spv_word(&B.decor, t_push_struct);
    spv_word(&B.decor, SpvDecorationBlock);

    spv_op(&B.decor, SpvOpMemberDecorate, 5);
    spv_word(&B.decor, t_push_struct);
    spv_word(&B.decor, 0);
    spv_word(&B.decor, SpvDecorationOffset);
    spv_word(&B.decor, 0);

    uint32_t t_ptr_push_struct = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, t_ptr_push_struct);
    spv_word(&B.types, SpvStorageClassPushConstant);
    spv_word(&B.types, t_push_struct);

    B.t_ptr_push_u32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, B.t_ptr_push_u32);
    spv_word(&B.types, SpvStorageClassPushConstant);
    spv_word(&B.types, B.t_u32);

    // Function-local pointer types.
    B.t_ptr_func_u32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, B.t_ptr_func_u32);
    spv_word(&B.types, SpvStorageClassFunction);
    spv_word(&B.types, B.t_u32);

    B.t_ptr_func_f32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, B.t_ptr_func_f32);
    spv_word(&B.types, SpvStorageClassFunction);
    spv_word(&B.types, B.t_f32);

    // --- Constants ---
    B.c_0 = spv_const_u32(&B, 0);
    B.c_1 = spv_const_u32(&B, 1);
    B.c_2 = spv_const_u32(&B, 2);
    B.c_3 = spv_const_u32(&B, 3);
    B.c_4 = spv_const_u32(&B, 4);
    B.c_8 = spv_const_u32(&B, 8);
    B.c_16 = spv_const_u32(&B, 16);
    B.c_0f = spv_const_f32(&B, 0.0f);
    B.c_allones = spv_const_u32(&B, 0xFFFFFFFFu);

    // --- Global variables ---

    // gl_GlobalInvocationID
    B.v_global_id = spv_id(&B);
    spv_op(&B.globals, SpvOpVariable, 4);
    spv_word(&B.globals, B.t_ptr_input_uvec3);
    spv_word(&B.globals, B.v_global_id);
    spv_word(&B.globals, SpvStorageClassInput);

    spv_op(&B.decor, SpvOpDecorate, 4);
    spv_word(&B.decor, B.v_global_id);
    spv_word(&B.decor, SpvDecorationBuiltIn);
    spv_word(&B.decor, SpvBuiltInGlobalInvocationId);

    // SSBO variables: one per buffer, descriptor binding = buffer index.
    B.v_ssbo = calloc((size_t)total_bufs, sizeof *B.v_ssbo);
    for (int i = 0; i < total_bufs; i++) {
        B.v_ssbo[i] = spv_id(&B);
        uint32_t ptr_type = B.buf_is_16[i] ? B.t_ptr_ssbo_struct_u16
                                            : B.t_ptr_ssbo_struct;
        spv_op(&B.globals, SpvOpVariable, 4);
        spv_word(&B.globals, ptr_type);
        spv_word(&B.globals, B.v_ssbo[i]);
        spv_word(&B.globals, SpvStorageClassUniform);

        spv_op(&B.decor, SpvOpDecorate, 4);
        spv_word(&B.decor, B.v_ssbo[i]);
        spv_word(&B.decor, SpvDecorationDescriptorSet);
        spv_word(&B.decor, 0);

        spv_op(&B.decor, SpvOpDecorate, 4);
        spv_word(&B.decor, B.v_ssbo[i]);
        spv_word(&B.decor, SpvDecorationBinding);
        spv_word(&B.decor, (uint32_t)i);
    }

    // Push constant variable.
    B.v_push = spv_id(&B);
    spv_op(&B.globals, SpvOpVariable, 4);
    spv_word(&B.globals, t_ptr_push_struct);
    spv_word(&B.globals, B.v_push);
    spv_word(&B.globals, SpvStorageClassPushConstant);

    // --- Entry point ---
    // OpEntryPoint GLCompute %main "main" %gl_GlobalInvocationID
    // In SPIR-V 1.0, only Input/Output variables go in the interface list.
    {
        char const name[] = "main";
        int name_words = ((int)sizeof(name) + 3) / 4;
        // word count = 3 + name_words + 1 (just gl_GlobalInvocationID)
        spv_op(&B.entry, SpvOpEntryPoint, 3 + name_words + 1);
        spv_word(&B.entry, SpvExecutionModelGLCompute);
        uint32_t fn_main = spv_id(&B);
        spv_word(&B.entry, fn_main);
        for (int w = 0; w < name_words; w++) {
            uint32_t word = 0;
            for (int b2 = 0; b2 < 4 && w * 4 + b2 < (int)sizeof(name); b2++) {
                word |= (uint32_t)(unsigned char)name[w * 4 + b2] << (b2 * 8);
            }
            spv_word(&B.entry, word);
        }
        spv_word(&B.entry, B.v_global_id);

        // --- Execution mode ---
        spv_op(&B.exec_mode, SpvOpExecutionMode, 6);
        spv_word(&B.exec_mode, fn_main);
        spv_word(&B.exec_mode, SpvExecutionModeLocalSize);
        spv_word(&B.exec_mode, WG_SIZE);
        spv_word(&B.exec_mode, 1);
        spv_word(&B.exec_mode, 1);

        spv_op(&B.exec_mode, SpvOpExecutionMode, 4);
        spv_word(&B.exec_mode, fn_main);
        spv_word(&B.exec_mode, SpvExecutionModeSignedZeroInfNanPreserve);
        spv_word(&B.exec_mode, 32); // float32

        // --- Function body ---
        // OpFunction
        spv_op(&B.func, SpvOpFunction, 5);
        spv_word(&B.func, B.t_void);
        spv_word(&B.func, fn_main);
        spv_word(&B.func, 0); // None function control
        spv_word(&B.func, B.t_fn_void);

        // Entry label.
        uint32_t label_entry = spv_id(&B);
        spv_op(&B.func, SpvOpLabel, 2);
        spv_word(&B.func, label_entry);

        // Load gl_GlobalInvocationID.
        uint32_t gid = spv_load(&B, B.t_uvec3, B.v_global_id);
        uint32_t gid_x = spv_composite_extract(&B, B.t_u32, gid, 0);
        uint32_t gid_y = spv_composite_extract(&B, B.t_u32, gid, 1);

        // Load w from push constants [0].
        uint32_t w_val = load_push_u32(&B, B.c_0);

        // Guard: if (gid_x >= w) return.
        uint32_t oob = spv_binop(&B, SpvOpUGreaterThanEqual, B.t_bool, gid_x, w_val);

        uint32_t label_body = spv_id(&B);
        uint32_t label_end = spv_id(&B);

        // OpSelectionMerge %label_end None
        spv_op(&B.func, 247 /*OpSelectionMerge*/, 3);
        spv_word(&B.func, label_end);
        spv_word(&B.func, 0); // None

        spv_op(&B.func, SpvOpBranchConditional, 4);
        spv_word(&B.func, oob);
        spv_word(&B.func, label_end);
        spv_word(&B.func, label_body);

        spv_op(&B.func, SpvOpLabel, 2);
        spv_word(&B.func, label_body);

        // x0 and y0 from push constants [1] and [2].
        uint32_t x0 = load_push_u32(&B, B.c_1);
        uint32_t y0 = load_push_u32(&B, B.c_2);

        uint32_t x_coord = spv_binop(&B, SpvOpIAdd, B.t_u32, x0, gid_x);
        uint32_t y_coord = spv_binop(&B, SpvOpIAdd, B.t_u32, y0, gid_y);

        // --- Allocate per-instruction result arrays. ---
        B.val   = calloc((size_t)(bb->insts + 1), sizeof *B.val);
        B.val_1 = calloc((size_t)(bb->insts + 1), sizeof *B.val_1);
        B.val_2 = calloc((size_t)(bb->insts + 1), sizeof *B.val_2);
        B.val_3 = calloc((size_t)(bb->insts + 1), sizeof *B.val_3);
        B.is_f  = calloc((size_t)(bb->insts + 1), sizeof *B.is_f);

        // Mark float-producing ops.
        for (int i = 0; i < bb->insts; i++) {
            B.is_f[i] = produces_float(bb->inst[i].op);
        }

        // --- Emit instructions. ---
        for (int i = 0; i < bb->insts; i++) {
            struct bb_inst const *inst = &bb->inst[i];

            int xid = get_id(inst->x);
            int yid = get_id(inst->y);

            switch (inst->op) {
                case op_x:
                    B.val[i] = x_coord;
                    break;
                case op_y:
                    B.val[i] = y_coord;
                    break;

                case op_imm_32:
                    B.val[i] = spv_const_u32(&B, (uint32_t)inst->imm);
                    break;

                case op_deref_ptr:
                    // Deref is handled on the host side. Nothing to emit in shader.
                    break;

                case op_uniform_32: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t slot_id = spv_const_u32(&B, (uint32_t)(inst->imm / 4));
                    B.val[i] = load_ssbo_u32(&B, p, slot_id);
                } break;

                case op_load_32: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p, B.c_2);
                    B.val[i] = load_ssbo_u32(&B, p, addr);
                } break;

                case op_store_32: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p, B.c_2);
                    uint32_t v = as_u32(&B, get_val(&B, inst->y), yid);
                    store_ssbo_u32(&B, p, addr, v);
                } break;

                case op_load_16: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr16 = compute_addr(&B, x_coord, y_coord, p, B.c_1);
                    B.val[i] = load_ssbo_u16(&B, p, addr16);
                } break;

                case op_store_16: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr16 = compute_addr(&B, x_coord, y_coord, p, B.c_1);
                    uint32_t v = as_u32(&B, get_val(&B, inst->y), yid);
                    store_ssbo_u16(&B, p, addr16, v);
                } break;

                case op_gather_uniform_32:
                case op_gather_32: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t ix_val = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t safe_idx, mask;
                    gather_safe(&B, ix_val, p, B.c_2, &safe_idx, &mask);
                    uint32_t raw = load_ssbo_u32(&B, p, safe_idx);
                    B.val[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, raw, mask);
                } break;

                case op_gather_16: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t ix_val = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t safe_idx, mask;
                    gather_safe(&B, ix_val, p, B.c_1, &safe_idx, &mask);
                    uint32_t raw = load_ssbo_u16(&B, p, safe_idx);
                    B.val[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, raw, mask);
                } break;

                case op_sample_32: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t ix_f = as_f32(&B, get_val(&B, inst->x), xid);
                    uint32_t fl = spv_glsl_1(&B, B.t_f32, GLSLstd450Floor, ix_f);
                    uint32_t frac = spv_binop(&B, SpvOpFSub, B.t_f32, ix_f, fl);
                    uint32_t lo_i = spv_unop(&B, SpvOpConvertFToS, B.t_i32, fl);
                    uint32_t lo_u = spv_bitcast(&B, B.t_u32, lo_i);
                    uint32_t hi_u = spv_binop(&B, SpvOpIAdd, B.t_u32, lo_u, B.c_1);
                    uint32_t lo_safe, lo_mask, hi_safe, hi_mask;
                    gather_safe(&B, lo_u, p, B.c_2, &lo_safe, &lo_mask);
                    gather_safe(&B, hi_u, p, B.c_2, &hi_safe, &hi_mask);
                    uint32_t lo_raw = load_ssbo_u32(&B, p, lo_safe);
                    uint32_t hi_raw = load_ssbo_u32(&B, p, hi_safe);
                    uint32_t lo_v = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, lo_raw, lo_mask);
                    uint32_t hi_v = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, hi_raw, hi_mask);
                    uint32_t lo_f = spv_bitcast(&B, B.t_f32, lo_v);
                    uint32_t hi_f = spv_bitcast(&B, B.t_f32, hi_v);
                    B.val[i] = spv_glsl_3(&B, B.t_f32, GLSLstd450FMix, lo_f, hi_f, frac);
                } break;

                case op_load_8x4: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p, B.c_2);
                    uint32_t px = load_ssbo_u32(&B, p, addr);
                    uint32_t c_0xFF = spv_const_u32(&B, 0xFFu);
                    B.val[i]   = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, px, c_0xFF);
                    uint32_t sh8  = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, px, B.c_8);
                    B.val_1[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, sh8, c_0xFF);
                    uint32_t sh16 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, px, B.c_16);
                    B.val_2[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, sh16, c_0xFF);
                    uint32_t c_24 = spv_const_u32(&B, 24);
                    B.val_3[i] = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, px, c_24);
                } break;
                case op_store_8x4: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p, B.c_2);
                    uint32_t r  = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t g  = as_u32(&B, get_val(&B, inst->y), yid);
                    uint32_t b_ = as_u32(&B, get_val(&B, inst->z), get_id(inst->z));
                    uint32_t a  = as_u32(&B, get_val(&B, inst->w), get_id(inst->w));
                    uint32_t g_sh = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, g, B.c_8);
                    uint32_t b_sh = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, b_, B.c_16);
                    uint32_t c_24 = spv_const_u32(&B, 24);
                    uint32_t a_sh = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, a, c_24);
                    uint32_t rg  = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, r, g_sh);
                    uint32_t rgb = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, rg, b_sh);
                    uint32_t px  = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, rgb, a_sh);
                    store_ssbo_u32(&B, p, addr, px);
                } break;

                case op_load_16x4: {
                    // Load 4 consecutive u16 values (8 bytes = 2 u32 words).
                    int p = resolve_ptr(&B, inst);
                    uint32_t rb_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t rb = load_push_u32(&B, rb_off);
                    uint32_t elems_per_row = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32,
                                                        rb, B.c_2);
                    uint32_t row_off = spv_binop(&B, SpvOpIMul, B.t_u32, y_coord, elems_per_row);
                    uint32_t x_off = spv_binop(&B, SpvOpIMul, B.t_u32, x_coord, B.c_2);
                    uint32_t base = spv_binop(&B, SpvOpIAdd, B.t_u32, row_off, x_off);

                    uint32_t w0 = load_ssbo_u32(&B, p, base);
                    uint32_t base1 = spv_binop(&B, SpvOpIAdd, B.t_u32, base, B.c_1);
                    uint32_t w1 = load_ssbo_u32(&B, p, base1);

                    uint32_t c_0xFFFF = spv_const_u32(&B, 0xFFFFu);
                    B.val[i]   = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, w0, c_0xFFFF);
                    B.val_1[i] = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, w0, B.c_16);
                    B.val_2[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, w1, c_0xFFFF);
                    B.val_3[i] = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, w1, B.c_16);
                } break;

                case op_load_16x4_planar: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t sz_off = spv_const_u32(&B, (uint32_t)(3 + p));
                    uint32_t sz = load_push_u32(&B, sz_off);
                    uint32_t ps_u16 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, sz, B.c_3);

                    uint32_t rb_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t rb = load_push_u32(&B, rb_off);

                    uint32_t rb_u16 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, rb, B.c_1);
                    uint32_t addr16 = spv_binop(&B, SpvOpIMul, B.t_u32, y_coord, rb_u16);
                    addr16 = spv_binop(&B, SpvOpIAdd, B.t_u32, addr16, x_coord);

                    for (int ch = 0; ch < 4; ch++) {
                        uint32_t ch_id = ch == 0 ? B.c_0
                                       : ch == 1 ? B.c_1
                                       : ch == 2 ? B.c_2
                                       :           B.c_3;
                        uint32_t plane_off = spv_binop(&B, SpvOpIMul, B.t_u32, ch_id, ps_u16);
                        uint32_t final_addr = spv_binop(&B, SpvOpIAdd, B.t_u32, addr16, plane_off);
                        uint32_t h_val = load_ssbo_u16(&B, p, final_addr);
                        switch (ch) {
                            case 0: B.val[i]   = h_val; break;
                            case 1: B.val_1[i] = h_val; break;
                            case 2: B.val_2[i] = h_val; break;
                            case 3: B.val_3[i] = h_val; break;
                        }
                    }
                } break;

                case op_store_16x4: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t rb_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t rb = load_push_u32(&B, rb_off);
                    uint32_t elems_per_row = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32,
                                                        rb, B.c_2);
                    uint32_t row_off = spv_binop(&B, SpvOpIMul, B.t_u32, y_coord, elems_per_row);
                    uint32_t x_off = spv_binop(&B, SpvOpIMul, B.t_u32, x_coord, B.c_2);
                    uint32_t base = spv_binop(&B, SpvOpIAdd, B.t_u32, row_off, x_off);

                    uint32_t h0 = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t h1 = as_u32(&B, get_val(&B, inst->y), yid);
                    uint32_t h2 = as_u32(&B, get_val(&B, inst->z), get_id(inst->z));
                    uint32_t h3 = as_u32(&B, get_val(&B, inst->w), get_id(inst->w));

                    uint32_t h1_shifted = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, h1, B.c_16);
                    uint32_t w0 = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, h0, h1_shifted);
                    uint32_t h3_shifted = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, h3, B.c_16);
                    uint32_t w1 = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, h2, h3_shifted);

                    store_ssbo_u32(&B, p, base, w0);
                    uint32_t base1 = spv_binop(&B, SpvOpIAdd, B.t_u32, base, B.c_1);
                    store_ssbo_u32(&B, p, base1, w1);
                } break;

                case op_store_16x4_planar: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t sz_off = spv_const_u32(&B, (uint32_t)(3 + p));
                    uint32_t sz = load_push_u32(&B, sz_off);
                    uint32_t ps_u16 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, sz, B.c_3);

                    uint32_t rb_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t rb = load_push_u32(&B, rb_off);
                    uint32_t rb_u16 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, rb, B.c_1);
                    uint32_t addr16 = spv_binop(&B, SpvOpIMul, B.t_u32, y_coord, rb_u16);
                    addr16 = spv_binop(&B, SpvOpIAdd, B.t_u32, addr16, x_coord);

                    val_ channels[4] = { inst->x, inst->y, inst->z, inst->w };
                    int channel_ids[4] = { xid, yid, get_id(inst->z), get_id(inst->w) };

                    for (int ch = 0; ch < 4; ch++) {
                        uint32_t ch_id = ch == 0 ? B.c_0
                                       : ch == 1 ? B.c_1
                                       : ch == 2 ? B.c_2
                                       :           B.c_3;
                        uint32_t plane_off = spv_binop(&B, SpvOpIMul, B.t_u32, ch_id, ps_u16);
                        uint32_t final_addr = spv_binop(&B, SpvOpIAdd, B.t_u32, addr16, plane_off);

                        uint32_t h_val = as_u32(&B, get_val(&B, channels[ch]),
                                                    channel_ids[ch]);
                        store_ssbo_u16(&B, p, final_addr, h_val);
                    }
                } break;

                case op_f32_from_f16: {
                    uint32_t h = as_u32(&B, get_val(&B, inst->x), xid);
                    B.val[i] = spv_f16_to_f32(&B, h);
                } break;

                case op_f16_from_f32: {
                    uint32_t f = as_f32(&B, get_val(&B, inst->x), xid);
                    B.val[i] = spv_f32_to_f16(&B, f);
                } break;

                case op_i32_from_s16: {
                    // Sign-extend 16-bit value to 32-bit.
                    uint32_t v = as_u32(&B, get_val(&B, inst->x), xid);
                    // Shift left 16, then arithmetic shift right 16.
                    uint32_t shifted = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, v, B.c_16);
                    // Use signed shift right.
                    uint32_t signed_val = spv_bitcast(&B, B.t_i32, shifted);
                    uint32_t sext = spv_binop(&B, SpvOpShiftRightArithmetic, B.t_i32, signed_val, B.c_16);
                    B.val[i] = spv_bitcast(&B, B.t_u32, sext);
                } break;

                case op_i32_from_u16: {
                    uint32_t v = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t c_0xFFFF = spv_const_u32(&B, 0xFFFFu);
                    B.val[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, v, c_0xFFFF);
                } break;

                case op_i16_from_i32: {
                    uint32_t v = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t c_0xFFFF = spv_const_u32(&B, 0xFFFFu);
                    B.val[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, v, c_0xFFFF);
                } break;

                case op_f32_from_i32: {
                    uint32_t v = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t sv = spv_bitcast(&B, B.t_i32, v);
                    B.val[i] = spv_unop(&B, SpvOpConvertSToF, B.t_f32, sv);
                } break;

                case op_i32_from_f32: {
                    uint32_t v = as_f32(&B, get_val(&B, inst->x), xid);
                    uint32_t sv = spv_unop(&B, SpvOpConvertFToS, B.t_i32, v);
                    B.val[i] = spv_bitcast(&B, B.t_u32, sv);
                } break;

                // --- Arithmetic float ops ---
                case op_add_f32:
                    B.val[i] = spv_binop(&B, SpvOpFAdd, B.t_f32,
                                          as_f32(&B, get_val(&B, inst->x), xid),
                                          as_f32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_sub_f32:
                    B.val[i] = spv_binop(&B, SpvOpFSub, B.t_f32,
                                          as_f32(&B, get_val(&B, inst->x), xid),
                                          as_f32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_mul_f32:
                    B.val[i] = spv_binop(&B, SpvOpFMul, B.t_f32,
                                          as_f32(&B, get_val(&B, inst->x), xid),
                                          as_f32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_div_f32:
                    B.val[i] = spv_binop(&B, SpvOpFDiv, B.t_f32,
                                          as_f32(&B, get_val(&B, inst->x), xid),
                                          as_f32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_min_f32:
                    B.val[i] = spv_glsl_2(&B, B.t_f32, GLSLstd450FMin,
                                            as_f32(&B, get_val(&B, inst->x), xid),
                                            as_f32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_max_f32:
                    B.val[i] = spv_glsl_2(&B, B.t_f32, GLSLstd450FMax,
                                            as_f32(&B, get_val(&B, inst->x), xid),
                                            as_f32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_sqrt_f32:
                    B.val[i] = spv_glsl_1(&B, B.t_f32, GLSLstd450Sqrt,
                                            as_f32(&B, get_val(&B, inst->x), xid));
                    break;
                case op_abs_f32:
                    B.val[i] = spv_glsl_1(&B, B.t_f32, GLSLstd450FAbs,
                                            as_f32(&B, get_val(&B, inst->x), xid));
                    break;
                case op_round_f32:
                    B.val[i] = spv_glsl_1(&B, B.t_f32, GLSLstd450RoundEven,
                                            as_f32(&B, get_val(&B, inst->x), xid));
                    break;
                case op_floor_f32:
                    B.val[i] = spv_glsl_1(&B, B.t_f32, GLSLstd450Floor,
                                            as_f32(&B, get_val(&B, inst->x), xid));
                    break;
                case op_ceil_f32:
                    B.val[i] = spv_glsl_1(&B, B.t_f32, GLSLstd450Ceil,
                                            as_f32(&B, get_val(&B, inst->x), xid));
                    break;
                case op_round_i32: {
                    uint32_t r = spv_glsl_1(&B, B.t_f32, GLSLstd450RoundEven,
                                             as_f32(&B, get_val(&B, inst->x), xid));
                    uint32_t si = spv_unop(&B, SpvOpConvertFToS, B.t_i32, r);
                    B.val[i] = spv_bitcast(&B, B.t_u32, si);
                } break;
                case op_floor_i32: {
                    uint32_t r = spv_glsl_1(&B, B.t_f32, GLSLstd450Floor,
                                             as_f32(&B, get_val(&B, inst->x), xid));
                    uint32_t si = spv_unop(&B, SpvOpConvertFToS, B.t_i32, r);
                    B.val[i] = spv_bitcast(&B, B.t_u32, si);
                } break;
                case op_ceil_i32: {
                    uint32_t r = spv_glsl_1(&B, B.t_f32, GLSLstd450Ceil,
                                             as_f32(&B, get_val(&B, inst->x), xid));
                    uint32_t si = spv_unop(&B, SpvOpConvertFToS, B.t_i32, r);
                    B.val[i] = spv_bitcast(&B, B.t_u32, si);
                } break;
                case op_fma_f32:
                    B.val[i] = spv_glsl_3(&B, B.t_f32, GLSLstd450Fma,
                                            as_f32(&B, get_val(&B, inst->x), xid),
                                            as_f32(&B, get_val(&B, inst->y), yid),
                                            as_f32(&B, get_val(&B, inst->z), get_id(inst->z)));
                    break;
                case op_fms_f32: {
                    uint32_t neg_x = spv_unop(&B, SpvOpFNegate, B.t_f32,
                                               as_f32(&B, get_val(&B, inst->x), xid));
                    B.val[i] = spv_glsl_3(&B, B.t_f32, GLSLstd450Fma,
                                            neg_x,
                                            as_f32(&B, get_val(&B, inst->y), yid),
                                            as_f32(&B, get_val(&B, inst->z), get_id(inst->z)));
                } break;

                // --- Integer arithmetic ---
                case op_add_i32:
                    B.val[i] = spv_binop(&B, SpvOpIAdd, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          as_u32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_sub_i32:
                    B.val[i] = spv_binop(&B, SpvOpISub, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          as_u32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_mul_i32:
                    B.val[i] = spv_binop(&B, SpvOpIMul, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          as_u32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_shl_i32:
                    B.val[i] = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          as_u32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_shr_u32:
                    B.val[i] = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          as_u32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_shr_s32: {
                    uint32_t sx = spv_bitcast(&B, B.t_i32, as_u32(&B, get_val(&B, inst->x), xid));
                    uint32_t r = spv_binop(&B, SpvOpShiftRightArithmetic, B.t_i32,
                                            sx, as_u32(&B, get_val(&B, inst->y), yid));
                    B.val[i] = spv_bitcast(&B, B.t_u32, r);
                } break;

                case op_and_32:
                    B.val[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          as_u32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_or_32:
                    B.val[i] = spv_binop(&B, SpvOpBitwiseOr, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          as_u32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_xor_32:
                    B.val[i] = spv_binop(&B, SpvOpBitwiseXor, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          as_u32(&B, get_val(&B, inst->y), yid));
                    break;

                case op_sel_32: {
                    // sel_32(cond, true_val, false_val): cond != 0 ? true_val : false_val
                    uint32_t cond = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t cond_bool = spv_binop(&B, SpvOpINotEqual, B.t_bool, cond, B.c_0);

                    _Bool yf = B.is_f[yid], zf = B.is_f[get_id(inst->z)];
                    if (yf && zf) {
                        B.is_f[i] = 1;
                        B.val[i] = spv_select(&B, B.t_f32, cond_bool,
                                               as_f32(&B, get_val(&B, inst->y), yid),
                                               as_f32(&B, get_val(&B, inst->z), get_id(inst->z)));
                    } else {
                        B.val[i] = spv_select(&B, B.t_u32, cond_bool,
                                               as_u32(&B, get_val(&B, inst->y), yid),
                                               as_u32(&B, get_val(&B, inst->z), get_id(inst->z)));
                    }
                } break;

                // --- Float comparisons (return 0xFFFFFFFF or 0) ---
                case op_eq_f32: {
                    uint32_t r = spv_binop(&B, SpvOpFOrdEqual, B.t_bool,
                                            as_f32(&B, get_val(&B, inst->x), xid),
                                            as_f32(&B, get_val(&B, inst->y), yid));
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_lt_f32: {
                    uint32_t r = spv_binop(&B, SpvOpFOrdLessThan, B.t_bool,
                                            as_f32(&B, get_val(&B, inst->x), xid),
                                            as_f32(&B, get_val(&B, inst->y), yid));
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_le_f32: {
                    uint32_t r = spv_binop(&B, SpvOpFOrdLessThanEqual, B.t_bool,
                                            as_f32(&B, get_val(&B, inst->x), xid),
                                            as_f32(&B, get_val(&B, inst->y), yid));
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;

                // --- Integer comparisons ---
                case op_eq_i32: {
                    uint32_t r = spv_binop(&B, SpvOpIEqual, B.t_bool,
                                            as_u32(&B, get_val(&B, inst->x), xid),
                                            as_u32(&B, get_val(&B, inst->y), yid));
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_lt_s32: {
                    uint32_t sx = spv_bitcast(&B, B.t_i32, as_u32(&B, get_val(&B, inst->x), xid));
                    uint32_t sy = spv_bitcast(&B, B.t_i32, as_u32(&B, get_val(&B, inst->y), yid));
                    uint32_t r = spv_binop(&B, SpvOpSLessThan, B.t_bool, sx, sy);
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_le_s32: {
                    uint32_t sx = spv_bitcast(&B, B.t_i32, as_u32(&B, get_val(&B, inst->x), xid));
                    uint32_t sy = spv_bitcast(&B, B.t_i32, as_u32(&B, get_val(&B, inst->y), yid));
                    uint32_t r = spv_binop(&B, SpvOpSLessThanEqual, B.t_bool, sx, sy);
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_lt_u32: {
                    uint32_t r = spv_binop(&B, SpvOpULessThan, B.t_bool,
                                            as_u32(&B, get_val(&B, inst->x), xid),
                                            as_u32(&B, get_val(&B, inst->y), yid));
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_le_u32: {
                    uint32_t r = spv_binop(&B, SpvOpULessThanEqual, B.t_bool,
                                            as_u32(&B, get_val(&B, inst->x), xid),
                                            as_u32(&B, get_val(&B, inst->y), yid));
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;

                // --- Immediate ops ---
                case op_shl_i32_imm:
                    B.val[i] = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          spv_const_u32(&B, (uint32_t)inst->imm));
                    break;
                case op_shr_u32_imm:
                    B.val[i] = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          spv_const_u32(&B, (uint32_t)inst->imm));
                    break;
                case op_shr_s32_imm: {
                    uint32_t sx = spv_bitcast(&B, B.t_i32, as_u32(&B, get_val(&B, inst->x), xid));
                    uint32_t r = spv_binop(&B, SpvOpShiftRightArithmetic, B.t_i32,
                                            sx, spv_const_u32(&B, (uint32_t)inst->imm));
                    B.val[i] = spv_bitcast(&B, B.t_u32, r);
                } break;
                case op_and_32_imm:
                    B.val[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          spv_const_u32(&B, (uint32_t)inst->imm));
                    break;
                case op_or_32_imm:
                    B.val[i] = spv_binop(&B, SpvOpBitwiseOr, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          spv_const_u32(&B, (uint32_t)inst->imm));
                    break;
                case op_xor_32_imm:
                    B.val[i] = spv_binop(&B, SpvOpBitwiseXor, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          spv_const_u32(&B, (uint32_t)inst->imm));
                    break;

                case op_add_f32_imm: {
                    uint32_t imm_f = spv_const_f32(&B, 0);
                    // Overwrite the constant bits directly.
                    // Actually, inst->imm holds the float bits as int.
                    // We need to create a float constant from those bits.
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    imm_f = spv_const_f32(&B, fval);
                    B.val[i] = spv_binop(&B, SpvOpFAdd, B.t_f32,
                                          as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                } break;
                case op_sub_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    B.val[i] = spv_binop(&B, SpvOpFSub, B.t_f32,
                                          as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                } break;
                case op_mul_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    B.val[i] = spv_binop(&B, SpvOpFMul, B.t_f32,
                                          as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                } break;
                case op_div_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    B.val[i] = spv_binop(&B, SpvOpFDiv, B.t_f32,
                                          as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                } break;
                case op_min_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    B.val[i] = spv_glsl_2(&B, B.t_f32, GLSLstd450FMin,
                                            as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                } break;
                case op_max_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    B.val[i] = spv_glsl_2(&B, B.t_f32, GLSLstd450FMax,
                                            as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                } break;
                case op_add_i32_imm:
                    B.val[i] = spv_binop(&B, SpvOpIAdd, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          spv_const_u32(&B, (uint32_t)inst->imm));
                    break;
                case op_sub_i32_imm:
                    B.val[i] = spv_binop(&B, SpvOpISub, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          spv_const_u32(&B, (uint32_t)inst->imm));
                    break;
                case op_mul_i32_imm:
                    B.val[i] = spv_binop(&B, SpvOpIMul, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          spv_const_u32(&B, (uint32_t)inst->imm));
                    break;

                // --- Immediate comparisons ---
                case op_eq_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    uint32_t r = spv_binop(&B, SpvOpFOrdEqual, B.t_bool,
                                            as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_lt_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    uint32_t r = spv_binop(&B, SpvOpFOrdLessThan, B.t_bool,
                                            as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_le_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    uint32_t r = spv_binop(&B, SpvOpFOrdLessThanEqual, B.t_bool,
                                            as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_eq_i32_imm: {
                    uint32_t r = spv_binop(&B, SpvOpIEqual, B.t_bool,
                                            as_u32(&B, get_val(&B, inst->x), xid),
                                            spv_const_u32(&B, (uint32_t)inst->imm));
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_lt_s32_imm: {
                    uint32_t sx = spv_bitcast(&B, B.t_i32, as_u32(&B, get_val(&B, inst->x), xid));
                    uint32_t sy = spv_const_i32(&B, inst->imm);
                    uint32_t r = spv_binop(&B, SpvOpSLessThan, B.t_bool, sx, sy);
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
                case op_le_s32_imm: {
                    uint32_t sx = spv_bitcast(&B, B.t_i32, as_u32(&B, get_val(&B, inst->x), xid));
                    uint32_t sy = spv_const_i32(&B, inst->imm);
                    uint32_t r = spv_binop(&B, SpvOpSLessThanEqual, B.t_bool, sx, sy);
                    B.val[i] = spv_select(&B, B.t_u32, r, B.c_allones, B.c_0);
                } break;
            }
        }

        // Return from function (end of body block).
        spv_op(&B.func, SpvOpReturn, 1);

        // End block (the early-return target).
        spv_op(&B.func, SpvOpLabel, 2);
        spv_word(&B.func, label_end);
        spv_op(&B.func, SpvOpReturn, 1);

        spv_op(&B.func, SpvOpFunctionEnd, 1);
    }

    // --- Concatenate all sections into final SPIR-V binary. ---
    int total_words = 5 // header
        + B.caps.len
        + B.exts.len
        + B.ext_import.len
        + B.mem_model.len
        + B.entry.len
        + B.exec_mode.len
        + B.decor.len
        + B.types.len
        + B.globals.len
        + B.func.len;

    uint32_t *spirv = malloc((size_t)total_words * sizeof(uint32_t));
    int off = 0;

    // Header.
    spirv[off++] = SpvMagic;
    spirv[off++] = SpvVersion;
    spirv[off++] = SpvGenerator;
    spirv[off++] = B.next_id; // bound
    spirv[off++] = SpvSchema;

    #define COPY_SECTION(s) do { \
        memcpy(spirv + off, (s).buf, (size_t)(s).len * sizeof(uint32_t)); \
        off += (s).len; \
    } while (0)

    COPY_SECTION(B.caps);
    COPY_SECTION(B.exts);
    COPY_SECTION(B.ext_import);
    COPY_SECTION(B.mem_model);
    COPY_SECTION(B.entry);
    COPY_SECTION(B.exec_mode);
    COPY_SECTION(B.decor);
    COPY_SECTION(B.types);
    COPY_SECTION(B.globals);
    COPY_SECTION(B.func);

    #undef COPY_SECTION

    *out_spirv_len = total_words;

    // Free temporary buffers.
    free(B.caps.buf);
    free(B.exts.buf);
    free(B.ext_import.buf);
    free(B.mem_model.buf);
    free(B.entry.buf);
    free(B.exec_mode.buf);
    free(B.decor.buf);
    free(B.types.buf);
    free(B.globals.buf);
    free(B.func.buf);
    free(B.v_ssbo);
    free(B.val);
    free(B.val_1);
    free(B.val_2);
    free(B.val_3);
    free(B.is_f);
    free(B.buf_is_16);
    free(B.const_cache);
    free(deref_buf);

    return spirv;
}

// ---------------------------------------------------------------------------
//  Vulkan resource creation helpers.
// ---------------------------------------------------------------------------

static VkBuffer create_buffer(VkDevice device, VkDeviceSize size) {
    VkBufferCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer buf;
    VkResult rc = vkCreateBuffer(device, &ci, 0, &buf);
    assume(rc == VK_SUCCESS);
    return buf;
}

static VkDeviceMemory alloc_and_bind(VkDevice device, VkBuffer buf, uint32_t mem_type) {
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buf, &req);
    VkMemoryAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = mem_type,
    };
    VkDeviceMemory mem;
    VkResult rc = vkAllocateMemory(device, &ai, 0, &mem);
    assume(rc == VK_SUCCESS);
    vkBindBufferMemory(device, buf, mem, 0);
    return mem;
}


// ---------------------------------------------------------------------------
//  Uniform ring chunk lifecycle.
// ---------------------------------------------------------------------------

enum { VK_RING_HIGH_WATER = 64 * 1024 };

struct vk_ring_chunk {
    VkBuffer       buf;
    VkDeviceMemory mem;
};

static struct uniform_ring_chunk vk_ring_new_chunk(size_t min_bytes, void *ctx) {
    struct vk_backend *be = ctx;
    size_t cap = min_bytes > VK_RING_HIGH_WATER ? min_bytes : VK_RING_HIGH_WATER;
    struct vk_ring_chunk *chunk = calloc(1, sizeof *chunk);
    chunk->buf = create_buffer(be->device, (VkDeviceSize)cap);
    chunk->mem = alloc_and_bind(be->device, chunk->buf, be->mem_type_host);
    void *mapped = 0;
    vkMapMemory(be->device, chunk->mem, 0, (VkDeviceSize)cap, 0, &mapped);
    return (struct uniform_ring_chunk){
        .handle=chunk,
        .mapped=mapped,
        .cap   =cap,
        .used  =0,
    };
}

static void vk_ring_free_chunk(void *handle, void *ctx) {
    struct vk_backend    *be    = ctx;
    struct vk_ring_chunk *chunk = handle;
    vkFreeMemory   (be->device, chunk->mem, 0);
    vkDestroyBuffer(be->device, chunk->buf, 0);
    free(chunk);
}

// ---------------------------------------------------------------------------
//  Batch helpers.
// ---------------------------------------------------------------------------

static void begin_batch(struct vk_backend *be) {
    if (be->batch_cmd) { return; }
    VkCommandBufferAllocateInfo ai = {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool=be->cmd_pool,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount=1,
    };
    vkAllocateCommandBuffers(be->device, &ai, &be->batch_cmd);
    VkCommandBufferBeginInfo bi = {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(be->batch_cmd, &bi);
    be->batch_has_dispatch = 0;
}

static void batch_track_copy(struct vk_backend *be, void *host, void *mapped, size_t bytes) {
    if (be->batch_n_copies >= be->batch_copies_cap) {
        be->batch_copies_cap = be->batch_copies_cap ? 2 * be->batch_copies_cap : 64;
        be->batch_copies = realloc(be->batch_copies, (size_t)be->batch_copies_cap * sizeof *be->batch_copies);
    }
    be->batch_copies[be->batch_n_copies++] = (struct copyback){host, mapped, bytes};
}


// ---------------------------------------------------------------------------
//  Program implementation.
// ---------------------------------------------------------------------------

static void vk_flush(struct umbra_backend *be);
static void vk_submit_cmdbuf(struct vk_backend *be);

// Returns an index into be->batch_cache. The entry is valid until vk_flush.
// Writable lookups reuse any existing entry for the same host pointer.
// Read-only lookups reuse only an existing *writable* entry for the same host
// pointer — that's the cross-program hand-off case (e.g. slug acc writes
// wind_buf, slug draw reads it via a read-only deref). A read-only request
// with no matching writable entry creates a fresh snapshot, because the host
// may have mutated the bytes since any prior read-only entry was made
// (e.g. slug acc loop counter in the uniforms buffer).
static int cache_buf(struct vk_backend *be, void *host, size_t bytes,
                     VkDeviceSize sz, _Bool read_only) {
    for (int i = 0; i < be->batch_cache_n; i++) {
        struct buf_cache_entry *ce = &be->batch_cache[i];
        if (ce->host == host && ce->size >= sz && (!read_only || ce->writable)) {
            return i;
        }
    }

    if (be->batch_cache_n >= be->batch_cache_cap) {
        be->batch_cache_cap = be->batch_cache_cap ? 2 * be->batch_cache_cap : 16;
        be->batch_cache = realloc(be->batch_cache,
            (size_t)be->batch_cache_cap * sizeof *be->batch_cache);
    }
    int idx = be->batch_cache_n++;
    struct buf_cache_entry *ce = &be->batch_cache[idx];
    *ce = (struct buf_cache_entry){0};

    // Try zero-copy host import for page-aligned writable pointers.
    VkDeviceSize align = be->host_import_align;
    if (align && host && !read_only && ((uintptr_t)host % align) == 0) {
        VkDeviceSize import_sz = (sz + align - 1) & ~(align - 1);
        VkBuffer buf = create_buffer(be->device, import_sz);
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(be->device, buf, &req);
        VkImportMemoryHostPointerInfoEXT imp = {
            .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
            .pHostPointer = host,
        };
        VkMemoryAllocateInfo ai = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &imp,
            .allocationSize = import_sz > req.size ? import_sz : req.size,
            .memoryTypeIndex = be->mem_type_host_import,
        };
        VkDeviceMemory mem = VK_NULL_HANDLE;
        if (vkAllocateMemory(be->device, &ai, 0, &mem) == VK_SUCCESS &&
            vkBindBufferMemory(be->device, buf, mem, 0) == VK_SUCCESS) {
            ce->buf      = buf;
            ce->mem      = mem;
            ce->size     = import_sz;
            ce->mapped   = host;
            ce->host     = host;
            ce->nocopy   = 1;
            ce->writable = 1;
            return idx;
        }
        // Import failed — fall back.
        if (mem) { vkFreeMemory(be->device, mem, 0); }
        vkDestroyBuffer(be->device, buf, 0);
    }

    ce->buf      = create_buffer(be->device, sz);
    ce->mem      = alloc_and_bind(be->device, ce->buf, be->mem_type_host);
    ce->size     = sz;
    vkMapMemory(be->device, ce->mem, 0, sz, 0, &ce->mapped);
    ce->host     = host;
    ce->writable = !read_only;
    if (bytes) { memcpy(ce->mapped, host, bytes); }
    if (!read_only && host && bytes) {
        batch_track_copy(be, host, ce->mapped, bytes);
    }
    return idx;
}

static void vk_program_queue(struct umbra_program *p, int l, int t, int r, int b,
                              struct umbra_buf buf[]) {
    struct vk_program *vp = (struct vk_program *)p;
    struct vk_backend *be = vp->be;

    int w = r - l, h = b - t;
    if (w <= 0 || h <= 0) { return; }

    begin_batch(be);

    int n = vp->total_bufs;

    // For each binding we need a (VkBuffer, offset, range) triple. Read-only
    // flat top-level buffers go through the per-batch uniform ring; everything
    // else (writable, row-structured, deref'd) goes through cache_buf so the
    // writable->readonly handoff path is preserved.
    VkBuffer     bind_buffer[32];
    VkDeviceSize bind_offset[32];
    VkDeviceSize bind_range [32];
    assume(n <= 32);
    for (int i = 0; i < n; i++) {
        bind_buffer[i] = VK_NULL_HANDLE;
        bind_offset[i] = 0;
        bind_range [i] = 0;
    }

    struct uniform_ring *cur_ring = &be->uni_rings[be->cur_frame];
    for (int i = 0; i <= vp->max_ptr; i++) {
        if (!buf[i].ptr || !buf[i].sz) { continue; }
        if (buf[i].read_only && !buf[i].row_bytes) {
            struct uniform_ring_loc loc =
                uniform_ring_alloc(cur_ring, buf[i].ptr, buf[i].sz);
            struct vk_ring_chunk *chunk = loc.handle;
            bind_buffer[i] = chunk->buf;
            bind_offset[i] = (VkDeviceSize)loc.offset;
            bind_range [i] = (VkDeviceSize)buf[i].sz;
            continue;
        }
        VkDeviceSize sz = (VkDeviceSize)buf[i].sz;
        if (sz < 4) { sz = 4; }
        int idx = cache_buf(be, buf[i].ptr, buf[i].sz, sz, buf[i].read_only);
        bind_buffer[i] = be->batch_cache[idx].buf;
        bind_range [i] = VK_WHOLE_SIZE;
    }

    uint32_t *push_data = calloc((size_t)vp->push_words, sizeof *push_data);
    push_data[0] = (uint32_t)w;
    push_data[1] = (uint32_t)l;
    push_data[2] = (uint32_t)t;

    for (int i = 0; i <= vp->max_ptr; i++) {
        push_data[3 + i] = (uint32_t)buf[i].sz;
        push_data[3 + vp->total_bufs + i] = (uint32_t)buf[i].row_bytes;
    }

    for (int d = 0; d < vp->n_deref; d++) {
        char *base = (char *)buf[vp->deref[d].src_buf].ptr;
        void *derived;
        ptrdiff_t ssz;
        size_t drb;
        memcpy(&derived, base + vp->deref[d].off, sizeof derived);
        memcpy(&ssz,     base + vp->deref[d].off + 8, sizeof ssz);
        memcpy(&drb,     base + vp->deref[d].off + 16, sizeof drb);
        size_t bytes = ssz < 0 ? (size_t)-ssz : (size_t)ssz;
        _Bool deref_read_only = ssz < 0;
        int bi = vp->deref[d].buf_idx;

        VkDeviceSize sz = (VkDeviceSize)bytes;
        if (sz < 4) { sz = 4; }
        int idx = cache_buf(be, derived, bytes, sz, deref_read_only);
        bind_buffer[bi] = be->batch_cache[idx].buf;
        bind_range [bi] = VK_WHOLE_SIZE;

        push_data[3 + bi] = (uint32_t)bytes;
        push_data[3 + vp->total_bufs + bi] = (uint32_t)drb;
    }

    for (int i = 0; i < n; i++) {
        if (bind_buffer[i] == VK_NULL_HANDLE) {
            int idx = cache_buf(be, 0, 0, 4, 1);
            bind_buffer[i] = be->batch_cache[idx].buf;
            bind_range [i] = VK_WHOLE_SIZE;
        }
    }

    // Push descriptors: each dispatch records its own buffer bindings directly
    // in the command buffer, so no external descriptor set to protect.
    VkDescriptorBufferInfo buf_infos[32];
    VkWriteDescriptorSet   writes[32];
    for (int i = 0; i < n; i++) {
        buf_infos[i] = (VkDescriptorBufferInfo){
            .buffer=bind_buffer[i],
            .offset=bind_offset[i],
            .range =bind_range [i],
        };
        writes[i] = (VkWriteDescriptorSet){
            .sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding=(uint32_t)i,
            .descriptorCount=1,
            .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo=&buf_infos[i],
        };
    }
    int n_desc = n;

    vkCmdBindPipeline(be->batch_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vp->pipeline);
    if (n_desc) {
        vkCmdPushDescriptorSet(be->batch_cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                               vp->pipe_layout, 0, (uint32_t)n_desc, writes);
    }
    vkCmdPushConstants(be->batch_cmd, vp->pipe_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, (uint32_t)(vp->push_words * (int)sizeof(uint32_t)), push_data);

    if (be->batch_has_dispatch) {
        VkMemoryBarrier mb = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        };
        vkCmdPipelineBarrier(be->batch_cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &mb, 0, NULL, 0, NULL);
    }
    be->batch_has_dispatch = 1;
    uint32_t gx = ((uint32_t)w + WG_SIZE - 1) / WG_SIZE;
    vkCmdDispatch(be->batch_cmd, gx, (uint32_t)h, 1);

    free(push_data);

    if (uniform_ring_used(&be->uni_rings[be->cur_frame]) > VK_RING_HIGH_WATER) {
        vk_submit_cmdbuf(be);
    }
}

static void vk_program_dump(struct umbra_program const *p, FILE *f) {
    struct vk_program const *vp = (struct vk_program const *)p;

    // Write SPIR-V binary to a temp file, disassemble with spirv-dis if available.
    char tmp[] = "/tmp/umbra_spirv_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd >= 0) {
        size_t n = (size_t)vp->spirv_len * sizeof(uint32_t);
        if (write(fd, vp->spirv, n) == (ssize_t)n) {
            close(fd);
            char cmd[256];
            snprintf(cmd, sizeof cmd, "spirv-dis --no-color '%s' 2>/dev/null", tmp);
            FILE *dis = popen(cmd, "r");
            if (dis) {
                char line[256];
                while (fgets(line, (int)sizeof line, dis)) { fputs(line, f); }
                if (pclose(dis) == 0) { unlink(tmp); return; }
            }
        } else {
            close(fd);
        }
        unlink(tmp);
    }
    // Fallback: summary only.
    fprintf(f, "vulkan SPIR-V (%d words, %d bufs, %d push words)\n",
            vp->spirv_len, vp->total_bufs, vp->push_words);
}

static void vk_program_free(struct umbra_program *p) {
    struct vk_program *vp = (struct vk_program *)p;
    struct vk_backend *be = vp->be;

    vkDestroyPipeline(be->device, vp->pipeline, 0);
    vkDestroyPipelineLayout(be->device, vp->pipe_layout, 0);
    vkDestroyDescriptorSetLayout(be->device, vp->ds_layout, 0);
    vkDestroyShaderModule(be->device, vp->shader, 0);
    free(vp->deref);
    free(vp->spirv);
    free(vp);
}

static struct umbra_program *vk_compile(struct umbra_backend *be,
                                         struct umbra_basic_block const *bb) {
    struct vk_backend *vbe = (struct vk_backend *)be;

    int spirv_len = 0, max_ptr = -1, total_bufs = 0, n_deref = 0, push_words = 0;
    struct deref_info *deref = 0;
    uint32_t *spirv = build_spirv(bb, &spirv_len, &max_ptr, &total_bufs,
                                   &n_deref, &deref, &push_words);
    if (!spirv) { return 0; }

    int n_desc = total_bufs;

    // Create shader module.
    VkShaderModule shader;
    {
        VkShaderModuleCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = (size_t)spirv_len * sizeof(uint32_t),
            .pCode = spirv,
        };
        VkResult rc = vkCreateShaderModule(vbe->device, &ci, 0, &shader);
        assume(rc == VK_SUCCESS);
    }

    // Descriptor set layout: one storage buffer per non-push buffer slot.
    VkDescriptorSetLayoutBinding *bindings = calloc((size_t)(n_desc ? n_desc : 1), sizeof *bindings);
    for (int i = 0; i < n_desc; i++) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = (uint32_t)i,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        };
    }
    VkDescriptorSetLayout ds_layout;
    {
        VkDescriptorSetLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = (uint32_t)n_desc,
            .pBindings = bindings,
        };
        vkCreateDescriptorSetLayout(vbe->device, &ci, 0, &ds_layout);
    }
    free(bindings);

    // Push constant range.
    VkPushConstantRange pcr = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = (uint32_t)(push_words * (int)sizeof(uint32_t)),
    };

    // Pipeline layout.
    VkPipelineLayout pipe_layout;
    {
        VkPipelineLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ds_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pcr,
        };
        vkCreatePipelineLayout(vbe->device, &ci, 0, &pipe_layout);
    }

    // Compute pipeline.
    VkPipeline pipeline;
    {
        VkComputePipelineCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main",
            },
            .layout = pipe_layout,
        };
        VkResult rc = vkCreateComputePipelines(vbe->device, VK_NULL_HANDLE, 1, &ci, 0, &pipeline);
        assume(rc == VK_SUCCESS);
    }

    struct vk_program *p = calloc(1, sizeof *p);
    p->be          = vbe;
    p->shader      = shader;
    p->ds_layout   = ds_layout;
    p->pipe_layout = pipe_layout;
    p->pipeline    = pipeline;
    p->max_ptr     = max_ptr;
    p->total_bufs  = total_bufs;
    p->n_deref     = n_deref;
    p->push_words    = push_words;
    p->deref       = deref;
    p->spirv       = spirv;
    p->spirv_len   = spirv_len;

    p->base.queue   = vk_program_queue;
    p->base.dump    = vk_program_dump;
    p->base.free    = vk_program_free;
    p->base.backend = be;
    return &p->base;
}

// ---------------------------------------------------------------------------
//  Backend lifecycle.
// ---------------------------------------------------------------------------

// Drain a single frame: wait for its in-flight cmdbuf to complete, free
// it, reset its fence, and reset the frame's ring. No-op on a frame with
// no in-flight cmdbuf.
static void vk_drain_frame(struct vk_backend *v, int frame) {
    if (v->frame_committed[frame]) {
        vkWaitForFences(v->device, 1, &v->frame_fences[frame], VK_TRUE, UINT64_MAX);
        vkResetFences(v->device, 1, &v->frame_fences[frame]);
        vkFreeCommandBuffers(v->device, v->cmd_pool, 1, &v->frame_committed[frame]);
        v->frame_committed[frame] = VK_NULL_HANDLE;
    }
    uniform_ring_reset(&v->uni_rings[frame]);
}

// Backpressure release: submit the current frame's cmdbuf without waiting,
// rotate to the other frame, and drain it. Drain only blocks if the new
// frame's prior cmdbuf is still running. Writebacks and cache_buf entries
// stay live across rotation.
static void vk_submit_cmdbuf(struct vk_backend *v) {
    if (!v->batch_cmd) { return; }

    vkEndCommandBuffer(v->batch_cmd);

    VkSubmitInfo si = {
        .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount=1,
        .pCommandBuffers=&v->batch_cmd,
    };
    vkQueueSubmit(v->queue, 1, &si, v->frame_fences[v->cur_frame]);

    v->frame_committed[v->cur_frame] = v->batch_cmd;
    v->batch_cmd                     = VK_NULL_HANDLE;
    v->batch_has_dispatch            = 0;

    v->cur_frame ^= 1;
    vk_drain_frame(v, v->cur_frame);
}

static void vk_flush(struct umbra_backend *be) {
    struct vk_backend *v = (struct vk_backend *)be;

    vk_submit_cmdbuf(v);
    for (int i = 0; i < VK_N_FRAMES; i++) { vk_drain_frame(v, i); }

    for (int i = 0; i < v->batch_n_copies; i++) {
        memcpy(v->batch_copies[i].host, v->batch_copies[i].mapped, v->batch_copies[i].bytes);
    }

    for (int i = 0; i < v->batch_cache_n; i++) {
        struct buf_cache_entry *ce = &v->batch_cache[i];
        vkFreeMemory  (v->device, ce->mem, 0);
        vkDestroyBuffer(v->device, ce->buf, 0);
    }

    v->batch_n_copies = 0;
    v->batch_cache_n  = 0;
}

static void vk_free(struct umbra_backend *be) {
    vk_flush(be);
    struct vk_backend *v = (struct vk_backend *)be;
    for (int i = 0; i < VK_N_FRAMES; i++) {
        uniform_ring_free(&v->uni_rings[i]);
        vkDestroyFence(v->device, v->frame_fences[i], 0);
    }
    vkDestroyCommandPool(v->device, v->cmd_pool, 0);
    vkDestroyDevice(v->device, 0);
    vkDestroyInstance(v->instance, 0);
    free(v->batch_copies);
    free(v->batch_cache);
    free(v);
}

struct umbra_backend *umbra_backend_vulkan(void) {
    VkInstance instance;
    {
        VkApplicationInfo app = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = VK_MAKE_VERSION(1, 0, 0),
        };
        char const *inst_exts[] = { "VK_KHR_get_physical_device_properties2" };
        VkInstanceCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app,
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = inst_exts,
        };
        if (vkCreateInstance(&ci, 0, &instance) != VK_SUCCESS) { return 0; }
    }

    VkPhysicalDevice phys;
    {
        uint32_t n = 1;
        if (vkEnumeratePhysicalDevices(instance, &n, &phys) < 0 || n == 0) {
            vkDestroyInstance(instance, 0);
            return 0;
        }
    }

    int qf = find_compute_queue(phys);
    if (qf < 0) { vkDestroyInstance(instance, 0); return 0; }

    VkDevice device;
    {
        float prio = 1.0f;
        VkDeviceQueueCreateInfo qci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = (uint32_t)qf,
            .queueCount = 1,
            .pQueuePriorities = &prio,
        };
        char const *exts[] = {
            "VK_KHR_16bit_storage",
            "VK_KHR_shader_float16_int8",
            "VK_KHR_external_memory",
            "VK_EXT_external_memory_host",
            "VK_KHR_shader_float_controls",
            "VK_KHR_push_descriptor",
        };
        VkPhysicalDeviceFloat16Int8FeaturesKHR f16_int8 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR,
            .shaderFloat16 = VK_TRUE,
        };
        VkPhysicalDevice16BitStorageFeatures f16 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
            .pNext = &f16_int8,
            .storageBuffer16BitAccess = VK_TRUE,
        };
        VkDeviceCreateInfo dci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &f16,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &qci,
            .enabledExtensionCount = sizeof exts / sizeof *exts,
            .ppEnabledExtensionNames = exts,
        };
        if (vkCreateDevice(phys, &dci, 0, &device) != VK_SUCCESS) {
            vkDestroyInstance(instance, 0);
            return 0;
        }
    }

    VkQueue queue;
    vkGetDeviceQueue(device, (uint32_t)qf, 0, &queue);

    VkCommandPool cmd_pool;
    {
        VkCommandPoolCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = (uint32_t)qf,
        };
        if (vkCreateCommandPool(device, &ci, 0, &cmd_pool) != VK_SUCCESS) {
            vkDestroyDevice(device, 0);
            vkDestroyInstance(instance, 0);
            return 0;
        }
    }

    // Query host import alignment.
    VkDeviceSize host_import_align = 0;
    {
        PFN_vkVoidFunction fn =
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
        PFN_vkGetPhysicalDeviceProperties2KHR getProps2;
        memcpy(&getProps2, &fn, sizeof fn);
        if (getProps2) {
            VkPhysicalDeviceExternalMemoryHostPropertiesEXT host_props = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
            };
            VkPhysicalDeviceProperties2 props2 = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &host_props,
            };
            getProps2(phys, &props2);
            host_import_align = host_props.minImportedHostPointerAlignment;
        }
    }

    // Find memory type for host pointer import.
    uint32_t mem_type_host_import = 0;
    PFN_vkGetMemoryHostPointerPropertiesEXT get_host_props = 0;
    if (host_import_align) {
        PFN_vkVoidFunction fn2 =
            vkGetDeviceProcAddr(device, "vkGetMemoryHostPointerPropertiesEXT");
        memcpy(&get_host_props, &fn2, sizeof fn2);
        if (get_host_props) {
            // Probe with a page-aligned allocation to discover compatible memory types.
            void *probe = 0;
            posix_memalign(&probe, (size_t)host_import_align, (size_t)host_import_align);
            VkMemoryHostPointerPropertiesEXT hp = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT,
            };
            if (get_host_props(device,
                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
                               probe, &hp) == VK_SUCCESS && hp.memoryTypeBits) {
                // Pick the first compatible type.
                for (uint32_t i = 0; i < 32; i++) {
                    if (hp.memoryTypeBits & (1u << i)) { mem_type_host_import = i; break; }
                }
            } else {
                host_import_align = 0;  // disable import
            }
            free(probe);
        } else {
            host_import_align = 0;  // disable import
        }
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys, &props);
    size_t ssbo_align = (size_t)props.limits.minStorageBufferOffsetAlignment;
    if (ssbo_align < 16) { ssbo_align = 16; }

    struct vk_backend *v = calloc(1, sizeof *v);
    v->instance            = instance;
    v->phys                = phys;
    v->device              = device;
    v->queue               = queue;
    v->cmd_pool            = cmd_pool;
    v->queue_family        = (uint32_t)qf;
    v->mem_type_host       = find_host_memory(phys);
    v->mem_type_host_import = mem_type_host_import;
    v->host_import_align   = host_import_align;
    v->get_host_props      = get_host_props;
    for (int i = 0; i < VK_N_FRAMES; i++) {
        v->uni_rings[i] = (struct uniform_ring){
            .align     =ssbo_align,
            .ctx       =v,
            .new_chunk =vk_ring_new_chunk,
            .free_chunk=vk_ring_free_chunk,
        };
        VkFenceCreateInfo fi = { .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        vkCreateFence(device, &fi, 0, &v->frame_fences[i]);
    }
    v->base.compile  = vk_compile;
    v->base.flush    = vk_flush;
    v->base.free     = vk_free;
    return &v->base;
}

#endif
