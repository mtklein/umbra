#include "bb.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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
    SpvCapabilityShader                   = 1,
    SpvCapabilityFloat16                  = 9,
    SpvCapabilityStorageBuffer16BitAccess = 4433,

    // Addressing / Memory models
    SpvAddressingModelLogical    = 0,
    SpvMemoryModelGLSL450        = 1,

    // Execution models
    SpvExecutionModelGLCompute = 5,

    // Execution modes
    SpvExecutionModeLocalSize = 17,

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

struct vk_backend {
    struct umbra_backend base;
    VkInstance     instance;
    VkPhysicalDevice phys;
    VkDevice       device;
    VkQueue        queue;
    VkCommandPool  cmd_pool;
    uint32_t       queue_family;
    uint32_t       mem_type_host;   // host-visible, coherent memory type index
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

struct deref_info { int buf_idx, src_buf, byte_off; };

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
    VkDescriptorPool        desc_pool;

    int max_ptr;
    int total_bufs;
    int n_deref;
    int push_words;       // number of uint32_t in push constants

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
    // (e.g. load_fp16x4 produces 4 channels).
    uint32_t *val;        // val[i] = SPIR-V ID for bb_inst i (channel 0)
    uint32_t *val_1;      // channel 1 for fp16x4 loads
    uint32_t *val_2;      // channel 2
    uint32_t *val_3;      // channel 3

    // Track which values are float type (vs uint).
    _Bool *is_f;

    // Map from bb_inst index -> deref buffer index.
    int *deref_buf;

    // Per-buffer flag: true if the buffer needs 16-bit typed access.
    _Bool *buf_is_16;
    int    has_16, :32;
} SpvBuilder;

static uint32_t spv_id(SpvBuilder *b) { return b->next_id++; }

static uint32_t spv_const_u32(SpvBuilder *b, uint32_t value) {
    uint32_t id = spv_id(b);
    spv_op(&b->types, SpvOpConstant, 4);
    spv_word(&b->types, b->t_u32);
    spv_word(&b->types, id);
    spv_word(&b->types, value);
    return id;
}

static uint32_t spv_const_i32(SpvBuilder *b, int32_t value) {
    uint32_t id = spv_id(b);
    spv_op(&b->types, SpvOpConstant, 4);
    spv_word(&b->types, b->t_i32);
    spv_word(&b->types, id);
    uint32_t bits;
    memcpy(&bits, &value, 4);
    spv_word(&b->types, bits);
    return id;
}

static uint32_t spv_const_f32(SpvBuilder *b, float value) {
    uint32_t id = spv_id(b);
    spv_op(&b->types, SpvOpConstant, 4);
    spv_word(&b->types, b->t_f32);
    spv_word(&b->types, id);
    uint32_t bits;
    memcpy(&bits, &value, 4);
    spv_word(&b->types, bits);
    return id;
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
    return b->val[id];
}

static int get_id(val_ v) { return (int)v.id; }

// Resolve a pointer index: if negative, it's a deref reference.
static int resolve_ptr(SpvBuilder *b, struct bb_inst const *inst) {
    return inst->ptr < 0 ? b->deref_buf[~inst->ptr] : inst->ptr;
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

// fp16 <-> fp32 via bitwise manipulation (no Float16 capability needed).
// f32_from_f16: convert a u32 holding a fp16 bit pattern in low 16 bits to float.
static uint32_t spv_f16_to_f32(SpvBuilder *b, uint32_t h) {
    // Extract sign, exponent, mantissa from fp16.
    uint32_t c_0x8000 = spv_const_u32(b, 0x8000u);
    uint32_t c_0x7C00 = spv_const_u32(b, 0x7C00u);
    uint32_t c_0x03FF = spv_const_u32(b, 0x03FFu);
    uint32_t c_10     = spv_const_u32(b, 10u);
    uint32_t c_13     = spv_const_u32(b, 13u);
    uint32_t c_23     = spv_const_u32(b, 23u);
    (void)0;
    uint32_t c_112    = spv_const_u32(b, 112u);

    uint32_t sign = spv_binop(b, SpvOpBitwiseAnd, b->t_u32, h, c_0x8000);
    sign = spv_binop(b, SpvOpShiftLeftLogical, b->t_u32, sign, spv_const_u32(b, 16u));

    uint32_t exp = spv_binop(b, SpvOpBitwiseAnd, b->t_u32, h, c_0x7C00);
    uint32_t exp_shifted = spv_binop(b, SpvOpShiftRightLogical, b->t_u32, exp, c_10);

    uint32_t mant = spv_binop(b, SpvOpBitwiseAnd, b->t_u32, h, c_0x03FF);

    // Check for zero/denorm exponent.
    uint32_t exp_is_zero = spv_binop(b, SpvOpIEqual, b->t_bool, exp, b->c_0);
    // Check for inf/nan exponent (0x1F).
    uint32_t c_0x1F = spv_const_u32(b, 0x1Fu);
    uint32_t exp_is_max = spv_binop(b, SpvOpIEqual, b->t_bool, exp_shifted, c_0x1F);

    // Normal case: f32_exp = (exp_shifted - 15 + 127) = (exp_shifted + 112)
    uint32_t f32_exp_normal = spv_binop(b, SpvOpIAdd, b->t_u32, exp_shifted, c_112);
    uint32_t f32_exp_normal_shifted = spv_binop(b, SpvOpShiftLeftLogical, b->t_u32,
                                                 f32_exp_normal, c_23);
    uint32_t f32_mant = spv_binop(b, SpvOpShiftLeftLogical, b->t_u32, mant, c_13);
    uint32_t normal = spv_binop(b, SpvOpBitwiseOr, b->t_u32, f32_exp_normal_shifted, f32_mant);
    normal = spv_binop(b, SpvOpBitwiseOr, b->t_u32, normal, sign);

    // Inf/NaN case: f32_exp = 0xFF
    uint32_t c_0x7F800000 = spv_const_u32(b, 0x7F800000u);
    uint32_t infnan = spv_binop(b, SpvOpBitwiseOr, b->t_u32, c_0x7F800000, f32_mant);
    infnan = spv_binop(b, SpvOpBitwiseOr, b->t_u32, infnan, sign);

    // Zero/denorm case: just return signed zero (denorms flush to zero for simplicity).
    uint32_t zero = sign;

    // Select: exp_is_max ? infnan : (exp_is_zero ? zero : normal)
    uint32_t inner = spv_select(b, b->t_u32, exp_is_zero, zero, normal);
    uint32_t result_bits = spv_select(b, b->t_u32, exp_is_max, infnan, inner);

    return spv_bitcast(b, b->t_f32, result_bits);
}

// f16_from_f32: convert float to u32 holding fp16 bit pattern in low 16 bits.
static uint32_t spv_f32_to_f16(SpvBuilder *b, uint32_t f) {
    uint32_t bits = spv_bitcast(b, b->t_u32, f);

    uint32_t c_0x80000000 = spv_const_u32(b, 0x80000000u);
    uint32_t c_0x7F800000 = spv_const_u32(b, 0x7F800000u);
    uint32_t c_0x007FFFFF = spv_const_u32(b, 0x007FFFFFu);
    uint32_t c_13     = spv_const_u32(b, 13u);
    uint32_t c_16     = spv_const_u32(b, 16u);
    uint32_t c_23     = spv_const_u32(b, 23u);
    uint32_t c_112    = spv_const_u32(b, 112u);

    uint32_t sign = spv_binop(b, SpvOpBitwiseAnd, b->t_u32, bits, c_0x80000000);
    uint32_t sign16 = spv_binop(b, SpvOpShiftRightLogical, b->t_u32, sign, c_16);

    uint32_t exp = spv_binop(b, SpvOpBitwiseAnd, b->t_u32, bits, c_0x7F800000);
    uint32_t exp_shifted = spv_binop(b, SpvOpShiftRightLogical, b->t_u32, exp, c_23);

    uint32_t mant = spv_binop(b, SpvOpBitwiseAnd, b->t_u32, bits, c_0x007FFFFF);

    // round to nearest even: add rounding bias
    uint32_t c_0x00000FFF = spv_const_u32(b, 0x00000FFFu);
    uint32_t c_0x00001000 = spv_const_u32(b, 0x00001000u);
    uint32_t mant_low = spv_binop(b, SpvOpBitwiseAnd, b->t_u32, mant, c_0x00001000);
    uint32_t round_bias = spv_binop(b, SpvOpIAdd, b->t_u32, c_0x00000FFF, mant_low);
    // if rounding would overflow mantissa, need to bump exponent
    // simplified: add bias, then shift
    uint32_t mant_rounded = spv_binop(b, SpvOpIAdd, b->t_u32, mant, round_bias);

    // Check overflow of mantissa (bit 23 set after add).
    uint32_t c_0x00800000 = spv_const_u32(b, 0x00800000u);
    uint32_t mant_overflow = spv_binop(b, SpvOpBitwiseAnd, b->t_u32, mant_rounded, c_0x00800000);
    uint32_t did_overflow = spv_binop(b, SpvOpINotEqual, b->t_bool, mant_overflow, b->c_0);
    // If overflow, increment exponent and zero mantissa bits.
    uint32_t exp_bump = spv_select(b, b->t_u32, did_overflow, b->c_1, b->c_0);
    uint32_t exp_adjusted = spv_binop(b, SpvOpIAdd, b->t_u32, exp_shifted, exp_bump);
    uint32_t final_mant = spv_select(b, b->t_u32, did_overflow, b->c_0, mant_rounded);

    // h_mant = final_mant >> 13
    uint32_t h_mant = spv_binop(b, SpvOpShiftRightLogical, b->t_u32, final_mant, c_13);
    uint32_t c_0x03FF = spv_const_u32(b, 0x03FFu);
    h_mant = spv_binop(b, SpvOpBitwiseAnd, b->t_u32, h_mant, c_0x03FF);

    // Check: exp >= 143 (0x8F) → clamp to inf (0x7C00)
    // exp < 113 (0x71) → clamp to zero
    uint32_t c_113 = spv_const_u32(b, 113u);
    uint32_t c_143 = spv_const_u32(b, 143u);
    uint32_t c_0x7C00 = spv_const_u32(b, 0x7C00u);

    uint32_t is_too_big = spv_binop(b, SpvOpUGreaterThanEqual, b->t_bool, exp_adjusted, c_143);
    uint32_t is_too_small = spv_binop(b, SpvOpULessThan, b->t_bool, exp_adjusted, c_113);
    // Also check original exp for inf/nan (exp_shifted == 0xFF)
    uint32_t c_0xFF = spv_const_u32(b, 0xFFu);
    uint32_t is_infnan = spv_binop(b, SpvOpIEqual, b->t_bool, exp_shifted, c_0xFF);

    // h_exp = (exp_adjusted - 112) << 10
    uint32_t h_exp = spv_binop(b, SpvOpISub, b->t_u32, exp_adjusted, c_112);
    h_exp = spv_binop(b, SpvOpShiftLeftLogical, b->t_u32, h_exp,
                       spv_const_u32(b, 10u));

    // Normal: sign16 | h_exp | h_mant
    uint32_t normal = spv_binop(b, SpvOpBitwiseOr, b->t_u32, sign16, h_exp);
    normal = spv_binop(b, SpvOpBitwiseOr, b->t_u32, normal, h_mant);

    // Inf/NaN: sign16 | 0x7C00 | (mant ? 0x0200 : 0)
    uint32_t c_0x0200 = spv_const_u32(b, 0x0200u);
    uint32_t mant_nonzero = spv_binop(b, SpvOpINotEqual, b->t_bool, mant, b->c_0);
    uint32_t nan_mant = spv_select(b, b->t_u32, mant_nonzero, c_0x0200, b->c_0);
    uint32_t infnan_val = spv_binop(b, SpvOpBitwiseOr, b->t_u32, sign16, c_0x7C00);
    infnan_val = spv_binop(b, SpvOpBitwiseOr, b->t_u32, infnan_val, nan_mant);

    // Zero/underflow: sign16
    // Overflow: sign16 | 0x7C00

    uint32_t overflow_val = spv_binop(b, SpvOpBitwiseOr, b->t_u32, sign16, c_0x7C00);

    // Select chain: is_infnan ? infnan : (is_too_big ? overflow : (is_too_small ? sign16 : normal))
    uint32_t inner1 = spv_select(b, b->t_u32, is_too_small, sign16, normal);
    uint32_t inner2 = spv_select(b, b->t_u32, is_too_big, overflow_val, inner1);
    return spv_select(b, b->t_u32, is_infnan, infnan_val, inner2);
}

static _Bool produces_float(enum op op) {
    return op == op_add_f32     || op == op_sub_f32
        || op == op_mul_f32     || op == op_div_f32
        || op == op_min_f32     || op == op_max_f32
        || op == op_sqrt_f32    || op == op_abs_f32    || op == op_neg_f32
        || op == op_round_f32   || op == op_floor_f32  || op == op_ceil_f32
        || op == op_fma_f32     || op == op_fms_f32
        || op == op_add_f32_imm || op == op_sub_f32_imm
        || op == op_mul_f32_imm || op == op_div_f32_imm
        || op == op_min_f32_imm || op == op_max_f32_imm
        || op == op_f32_from_i32
        || op == op_f32_from_f16
        || op == op_load_fp16x4
        || op == op_load_fp16x4_planar;
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
    int n_deref = 0;
    for (int i = 0; i < bb->insts; i++) {
        if (bb->inst[i].op == op_deref_ptr) {
            deref_buf[i] = next_buf++;
            n_deref++;
        }
    }
    int total_bufs = next_buf;
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
                di[d].byte_off = bb->inst[i].imm;
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
         || op == op_load_fp16x4_planar || op == op_store_fp16x4_planar) {
            int p = bb->inst[i].ptr < 0 ? deref_buf[~bb->inst[i].ptr]
                                         : bb->inst[i].ptr;
            B.buf_is_16[p] = 1;
            B.has_16 = 1;
        }
    }

    // Push constant layout: w, x0, y0, buf_szs[total_bufs], buf_rbs[total_bufs]
    int push_words = 3 + 2 * total_bufs;
    *out_push_words = push_words;
    B.push_words = push_words;

    // --- Capabilities ---
    spv_op(&B.caps, SpvOpCapability, 2);
    spv_word(&B.caps, SpvCapabilityShader);

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
        B.t_u16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeInt, 4);
        spv_word(&B.types, B.t_u16);
        spv_word(&B.types, 16);
        spv_word(&B.types, 0); // unsigned

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

    // SSBO variables (one per buffer).
    B.v_ssbo = calloc((size_t)total_bufs, sizeof *B.v_ssbo);
    for (int i = 0; i < total_bufs; i++) {
        B.v_ssbo[i] = spv_id(&B);
        uint32_t ptr_type = B.buf_is_16[i] ? B.t_ptr_ssbo_struct_u16
                                            : B.t_ptr_ssbo_struct;
        spv_op(&B.globals, SpvOpVariable, 4);
        spv_word(&B.globals, ptr_type);
        spv_word(&B.globals, B.v_ssbo[i]);
        spv_word(&B.globals, SpvStorageClassUniform);

        // DescriptorSet = 0, Binding = i.
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
                    // Read buf[p] as u32 array at slot inst->imm.
                    uint32_t slot_id = spv_const_u32(&B, (uint32_t)inst->imm);
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
                    // Direct u16 load via 16-bit storage.
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr16 = compute_addr(&B, x_coord, y_coord, p, B.c_1);
                    B.val[i] = load_ssbo_u16(&B, p, addr16);
                } break;

                case op_store_16: {
                    // Direct u16 store via 16-bit storage.
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
                    // Direct u16 gather via 16-bit storage.
                    int p = resolve_ptr(&B, inst);
                    uint32_t ix_val = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t safe_idx, mask;
                    gather_safe(&B, ix_val, p, B.c_1, &safe_idx, &mask);
                    uint32_t raw = load_ssbo_u16(&B, p, safe_idx);
                    B.val[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, raw, mask);
                } break;

                case op_load_fp16x4: {
                    // Load 4 consecutive fp16 values (8 bytes = 2 u32 words).
                    int p = resolve_ptr(&B, inst);
                    // Base address in u32: y * (rb/4) + x*2
                    uint32_t rb_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t rb = load_push_u32(&B, rb_off);
                    uint32_t elems_per_row = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32,
                                                        rb, B.c_2);
                    uint32_t row_off = spv_binop(&B, SpvOpIMul, B.t_u32, y_coord, elems_per_row);
                    uint32_t x_off = spv_binop(&B, SpvOpIMul, B.t_u32, x_coord, B.c_2);
                    uint32_t base = spv_binop(&B, SpvOpIAdd, B.t_u32, row_off, x_off);

                    // Load word0 (contains h[0], h[1]) and word1 (h[2], h[3]).
                    uint32_t w0 = load_ssbo_u32(&B, p, base);
                    uint32_t base1 = spv_binop(&B, SpvOpIAdd, B.t_u32, base, B.c_1);
                    uint32_t w1 = load_ssbo_u32(&B, p, base1);

                    uint32_t c_0xFFFF = spv_const_u32(&B, 0xFFFFu);
                    uint32_t h0 = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, w0, c_0xFFFF);
                    uint32_t h1 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, w0, B.c_16);
                    uint32_t h2 = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, w1, c_0xFFFF);
                    uint32_t h3 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, w1, B.c_16);

                    B.val[i]   = spv_f16_to_f32(&B, h0);
                    B.val_1[i] = spv_f16_to_f32(&B, h1);
                    B.val_2[i] = spv_f16_to_f32(&B, h2);
                    B.val_3[i] = spv_f16_to_f32(&B, h3);
                } break;

                case op_load_fp16x4_planar: {
                    // Direct u16 load via 16-bit storage.
                    int p = resolve_ptr(&B, inst);
                    uint32_t sz_off = spv_const_u32(&B, (uint32_t)(3 + p));
                    uint32_t sz = load_push_u32(&B, sz_off);
                    // plane_size in bytes = sz / 4; in u16 elements = sz / 4 / 2 = sz / 8
                    uint32_t ps_u16 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, sz, B.c_3);

                    uint32_t rb_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t rb = load_push_u32(&B, rb_off);

                    // addr16 within plane: y * (rb/2) + x
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
                        uint32_t f_val = spv_f16_to_f32(&B, h_val);
                        switch (ch) {
                            case 0: B.val[i]   = f_val; break;
                            case 1: B.val_1[i] = f_val; break;
                            case 2: B.val_2[i] = f_val; break;
                            case 3: B.val_3[i] = f_val; break;
                        }
                    }
                } break;

                case op_store_fp16x4: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t rb_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t rb = load_push_u32(&B, rb_off);
                    uint32_t elems_per_row = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32,
                                                        rb, B.c_2);
                    uint32_t row_off = spv_binop(&B, SpvOpIMul, B.t_u32, y_coord, elems_per_row);
                    uint32_t x_off = spv_binop(&B, SpvOpIMul, B.t_u32, x_coord, B.c_2);
                    uint32_t base = spv_binop(&B, SpvOpIAdd, B.t_u32, row_off, x_off);

                    uint32_t h0 = spv_f32_to_f16(&B, as_f32(&B, get_val(&B, inst->x), xid));
                    uint32_t h1 = spv_f32_to_f16(&B, as_f32(&B, get_val(&B, inst->y), yid));
                    uint32_t h2 = spv_f32_to_f16(&B, as_f32(&B, get_val(&B, inst->z), get_id(inst->z)));
                    uint32_t h3 = spv_f32_to_f16(&B, as_f32(&B, get_val(&B, inst->w), get_id(inst->w)));

                    uint32_t h1_shifted = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, h1, B.c_16);
                    uint32_t w0 = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, h0, h1_shifted);
                    uint32_t h3_shifted = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, h3, B.c_16);
                    uint32_t w1 = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, h2, h3_shifted);

                    store_ssbo_u32(&B, p, base, w0);
                    uint32_t base1 = spv_binop(&B, SpvOpIAdd, B.t_u32, base, B.c_1);
                    store_ssbo_u32(&B, p, base1, w1);
                } break;

                case op_store_fp16x4_planar: {
                    // Direct u16 store via 16-bit storage.
                    int p = resolve_ptr(&B, inst);
                    uint32_t sz_off = spv_const_u32(&B, (uint32_t)(3 + p));
                    uint32_t sz = load_push_u32(&B, sz_off);
                    // plane_size in u16 elements = sz / 8
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

                        uint32_t h_val = spv_f32_to_f16(&B, as_f32(&B, get_val(&B, channels[ch]),
                                                                     channel_ids[ch]));
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
                case op_neg_f32:
                    B.val[i] = spv_unop(&B, SpvOpFNegate, B.t_f32,
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

                case op_pack: {
                    // pack(x, y, shift): x | (y << shift)
                    uint32_t shifted = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32,
                                                  as_u32(&B, get_val(&B, inst->y), yid),
                                                  spv_const_u32(&B, (uint32_t)inst->imm));
                    B.val[i] = spv_binop(&B, SpvOpBitwiseOr, B.t_u32,
                                          as_u32(&B, get_val(&B, inst->x), xid),
                                          shifted);
                } break;

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
    if (vkCreateBuffer(device, &ci, 0, &buf) != VK_SUCCESS) { return VK_NULL_HANDLE; }
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
    if (vkAllocateMemory(device, &ai, 0, &mem) != VK_SUCCESS) { return VK_NULL_HANDLE; }
    vkBindBufferMemory(device, buf, mem, 0);
    return mem;
}

// ---------------------------------------------------------------------------
//  Per-dispatch state (everything needed for one queue+flush cycle).
// ---------------------------------------------------------------------------

struct dispatch_state {
    VkBuffer       *bufs;
    VkDeviceMemory *mems;
    void          **maps;       // mapped pointers
    VkDeviceSize   *sizes;      // actual buffer sizes
    VkCommandBuffer cmd;
    VkFence         fence;
    VkDescriptorSet ds;
    struct copyback *copy;
    int              n_bufs;
    int              n_copy, copy_cap, :32;
};

// ---------------------------------------------------------------------------
//  Program implementation.
// ---------------------------------------------------------------------------

static void vk_program_queue(struct umbra_program *p, int l, int t, int r, int b,
                              umbra_buf buf[]) {
    struct vk_program *vp = (struct vk_program *)p;
    struct vk_backend *be = vp->be;

    int w = r - l, h = b - t;
    if (w <= 0 || h <= 0) { return; }

    // --- Create per-dispatch resources. ---
    struct dispatch_state *ds = calloc(1, sizeof *ds);
    ds->n_bufs = vp->total_bufs;
    ds->bufs  = calloc((size_t)ds->n_bufs, sizeof *ds->bufs);
    ds->mems  = calloc((size_t)ds->n_bufs, sizeof *ds->mems);
    ds->maps  = calloc((size_t)ds->n_bufs, sizeof *ds->maps);
    ds->sizes = calloc((size_t)ds->n_bufs, sizeof *ds->sizes);

    // Create buffers for direct pointers (0..max_ptr).
    for (int i = 0; i <= vp->max_ptr; i++) {
        if (!buf[i].ptr || !buf[i].sz) { continue; }
        VkDeviceSize sz = (VkDeviceSize)buf[i].sz;
        if (sz < 4) { sz = 4; } // Minimum buffer size.
        ds->bufs[i] = create_buffer(be->device, sz);
        ds->mems[i] = alloc_and_bind(be->device, ds->bufs[i], be->mem_type_host);
        ds->sizes[i] = sz;
        vkMapMemory(be->device, ds->mems[i], 0, sz, 0, &ds->maps[i]);
        memcpy(ds->maps[i], buf[i].ptr, buf[i].sz);
        if (!buf[i].read_only) {
            // Track copyback.
            if (ds->n_copy >= ds->copy_cap) {
                ds->copy_cap = ds->copy_cap ? 2 * ds->copy_cap : 16;
                ds->copy = realloc(ds->copy, (size_t)ds->copy_cap * sizeof *ds->copy);
            }
            ds->copy[ds->n_copy++] = (struct copyback){
                .host = buf[i].ptr,
                .mapped = ds->maps[i],
                .bytes = buf[i].sz,
            };
        }
    }

    // Resolve deref buffers.
    uint32_t *push_data = calloc((size_t)vp->push_words, sizeof *push_data);
    push_data[0] = (uint32_t)w;
    push_data[1] = (uint32_t)l;
    push_data[2] = (uint32_t)t;

    for (int i = 0; i <= vp->max_ptr; i++) {
        push_data[3 + i] = (uint32_t)buf[i].sz;
        push_data[3 + vp->total_bufs + i] = (uint32_t)buf[i].row_bytes;
    }

    for (int d = 0; d < vp->n_deref; d++) {
        char *base = (char *)buf[vp->deref[d].src_buf].ptr
                   + (size_t)0 * buf[vp->deref[d].src_buf].row_bytes;
        void *derived;
        ptrdiff_t ssz;
        size_t drb;
        memcpy(&derived, base + vp->deref[d].byte_off, sizeof derived);
        memcpy(&ssz,     base + vp->deref[d].byte_off + 8, sizeof ssz);
        memcpy(&drb,     base + vp->deref[d].byte_off + 16, sizeof drb);
        size_t bytes = ssz < 0 ? (size_t)-ssz : (size_t)ssz;
        _Bool deref_read_only = ssz < 0;
        int bi = vp->deref[d].buf_idx;

        VkDeviceSize sz = (VkDeviceSize)bytes;
        if (sz < 4) { sz = 4; }
        ds->bufs[bi] = create_buffer(be->device, sz);
        ds->mems[bi] = alloc_and_bind(be->device, ds->bufs[bi], be->mem_type_host);
        ds->sizes[bi] = sz;
        vkMapMemory(be->device, ds->mems[bi], 0, sz, 0, &ds->maps[bi]);
        memcpy(ds->maps[bi], derived, bytes);

        push_data[3 + bi] = (uint32_t)bytes;
        push_data[3 + vp->total_bufs + bi] = (uint32_t)drb;

        if (!deref_read_only) {
            if (ds->n_copy >= ds->copy_cap) {
                ds->copy_cap = ds->copy_cap ? 2 * ds->copy_cap : 16;
                ds->copy = realloc(ds->copy, (size_t)ds->copy_cap * sizeof *ds->copy);
            }
            ds->copy[ds->n_copy++] = (struct copyback){
                .host = derived,
                .mapped = ds->maps[bi],
                .bytes = bytes,
            };
        }
    }

    // --- Update descriptor set. ---
    VkDescriptorBufferInfo *buf_infos = calloc((size_t)vp->total_bufs, sizeof *buf_infos);
    VkWriteDescriptorSet *writes = calloc((size_t)vp->total_bufs, sizeof *writes);
    int n_writes = 0;
    for (int i = 0; i < vp->total_bufs; i++) {
        if (!ds->bufs[i]) {
            // Create a dummy buffer for unused slots.
            ds->bufs[i] = create_buffer(be->device, 4);
            ds->mems[i] = alloc_and_bind(be->device, ds->bufs[i], be->mem_type_host);
            ds->sizes[i] = 4;
        }
        buf_infos[i] = (VkDescriptorBufferInfo){
            .buffer = ds->bufs[i],
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        };
        writes[n_writes++] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds->ds,
            .dstBinding = (uint32_t)i,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buf_infos[i],
        };
    }

    // Allocate descriptor set.
    {
        VkDescriptorSetAllocateInfo ai = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vp->desc_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &vp->ds_layout,
        };
        vkAllocateDescriptorSets(be->device, &ai, &ds->ds);
    }

    // Update writes with the allocated set.
    for (int i = 0; i < n_writes; i++) {
        writes[i].dstSet = ds->ds;
    }
    vkUpdateDescriptorSets(be->device, (uint32_t)n_writes, writes, 0, 0);

    free(buf_infos);
    free(writes);

    // --- Record command buffer. ---
    {
        VkCommandBufferAllocateInfo ai = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = be->cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        vkAllocateCommandBuffers(be->device, &ai, &ds->cmd);
    }

    {
        VkCommandBufferBeginInfo bi = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(ds->cmd, &bi);
    }

    vkCmdBindPipeline(ds->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vp->pipeline);
    vkCmdBindDescriptorSets(ds->cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            vp->pipe_layout, 0, 1, &ds->ds, 0, 0);
    vkCmdPushConstants(ds->cmd, vp->pipe_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, (uint32_t)(vp->push_words * (int)sizeof(uint32_t)), push_data);

    uint32_t gx = ((uint32_t)w + WG_SIZE - 1) / WG_SIZE;
    vkCmdDispatch(ds->cmd, gx, (uint32_t)h, 1);

    vkEndCommandBuffer(ds->cmd);

    // --- Create fence and submit. ---
    {
        VkFenceCreateInfo fi = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        };
        vkCreateFence(be->device, &fi, 0, &ds->fence);
    }

    {
        VkSubmitInfo si = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &ds->cmd,
        };
        vkQueueSubmit(be->queue, 1, &si, ds->fence);
    }

    // Wait immediately, copy back, free everything.
    // (Simple synchronous model for now.)
    vkWaitForFences(be->device, 1, &ds->fence, VK_TRUE, UINT64_MAX);

    // Copyback.
    for (int i = 0; i < ds->n_copy; i++) {
        memcpy(ds->copy[i].host, ds->copy[i].mapped, ds->copy[i].bytes);
    }

    // Cleanup.
    vkDestroyFence(be->device, ds->fence, 0);
    vkFreeCommandBuffers(be->device, be->cmd_pool, 1, &ds->cmd);
    vkResetDescriptorPool(be->device, vp->desc_pool, 0);

    for (int i = 0; i < ds->n_bufs; i++) {
        if (ds->mems[i]) { vkFreeMemory(be->device, ds->mems[i], 0); }
        if (ds->bufs[i]) { vkDestroyBuffer(be->device, ds->bufs[i], 0); }
    }

    free(ds->bufs);
    free(ds->mems);
    free(ds->maps);
    free(ds->sizes);
    free(ds->copy);
    free(ds);
    free(push_data);
}

static void vk_program_dump(struct umbra_program const *p, FILE *f) {
    struct vk_program const *vp = (struct vk_program const *)p;
    fprintf(f, "vulkan SPIR-V (%d words, %d bufs, %d push words)\n",
            vp->spirv_len, vp->total_bufs, vp->push_words);
}

static void vk_program_free(struct umbra_program *p) {
    struct vk_program *vp = (struct vk_program *)p;
    struct vk_backend *be = vp->be;

    vkDestroyPipeline(be->device, vp->pipeline, 0);
    vkDestroyPipelineLayout(be->device, vp->pipe_layout, 0);
    vkDestroyDescriptorSetLayout(be->device, vp->ds_layout, 0);
    vkDestroyDescriptorPool(be->device, vp->desc_pool, 0);
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

    // Create shader module.
    VkShaderModule shader;
    {
        VkShaderModuleCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = (size_t)spirv_len * sizeof(uint32_t),
            .pCode = spirv,
        };
        if (vkCreateShaderModule(vbe->device, &ci, 0, &shader) != VK_SUCCESS) {
            free(spirv);
            free(deref);
            return 0;
        }
    }

    // Descriptor set layout: one storage buffer per buffer slot.
    VkDescriptorSetLayoutBinding *bindings = calloc((size_t)total_bufs, sizeof *bindings);
    for (int i = 0; i < total_bufs; i++) {
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
            .bindingCount = (uint32_t)total_bufs,
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
        if (vkCreateComputePipelines(vbe->device, VK_NULL_HANDLE, 1, &ci, 0, &pipeline) != VK_SUCCESS) {
            vkDestroyShaderModule(vbe->device, shader, 0);
            vkDestroyPipelineLayout(vbe->device, pipe_layout, 0);
            vkDestroyDescriptorSetLayout(vbe->device, ds_layout, 0);
            free(spirv);
            free(deref);
            return 0;
        }
    }

    // Descriptor pool (reusable per dispatch).
    VkDescriptorPool desc_pool;
    {
        VkDescriptorPoolSize ps = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = (uint32_t)total_bufs,
        };
        VkDescriptorPoolCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &ps,
        };
        vkCreateDescriptorPool(vbe->device, &ci, 0, &desc_pool);
    }

    struct vk_program *p = calloc(1, sizeof *p);
    p->be          = vbe;
    p->shader      = shader;
    p->ds_layout   = ds_layout;
    p->pipe_layout = pipe_layout;
    p->pipeline    = pipeline;
    p->desc_pool   = desc_pool;
    p->max_ptr     = max_ptr;
    p->total_bufs  = total_bufs;
    p->n_deref     = n_deref;
    p->push_words  = push_words;
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

static void vk_flush(struct umbra_backend *be) {
    struct vk_backend *v = (struct vk_backend *)be;
    vkQueueWaitIdle(v->queue);
}

static void vk_free(struct umbra_backend *be) {
    struct vk_backend *v = (struct vk_backend *)be;
    vkDestroyCommandPool(v->device, v->cmd_pool, 0);
    vkDestroyDevice(v->device, 0);
    vkDestroyInstance(v->instance, 0);
    free(v);
}

struct umbra_backend *umbra_backend_vulkan(void) {
    VkInstance instance;
    {
        VkApplicationInfo app = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = VK_MAKE_VERSION(1, 0, 0),
        };
        VkInstanceCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app,
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
        char const *exts[] = { "VK_KHR_16bit_storage" };
        VkPhysicalDevice16BitStorageFeatures f16 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
            .storageBuffer16BitAccess = VK_TRUE,
        };
        VkDeviceCreateInfo dci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &f16,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &qci,
            .enabledExtensionCount = 1,
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

    struct vk_backend *v = calloc(1, sizeof *v);
    v->instance     = instance;
    v->phys         = phys;
    v->device       = device;
    v->queue        = queue;
    v->cmd_pool     = cmd_pool;
    v->queue_family = (uint32_t)qf;
    v->mem_type_host = find_host_memory(phys);
    v->base.compile  = vk_compile;
    v->base.flush    = vk_flush;
    v->base.free     = vk_free;
    return &v->base;
}

#endif
