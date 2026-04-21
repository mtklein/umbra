#include "spirv.h"
#include <stdlib.h>
#include <string.h>

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
    SpvOpNot                  = 200,
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
    SpvOpSelectionMerge       = 247,
    SpvOpLoopMerge            = 246,
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
    GLSLstd450FAbs = 4,
    GLSLstd450Floor = 8,
    GLSLstd450Ceil = 9,
    GLSLstd450Sqrt = 31,
    GLSLstd450InverseSqrt = 32,
    GLSLstd450FMin = 37,
    GLSLstd450FMax = 40,
    GLSLstd450FMix = 46,
    GLSLstd450Fma = 50,

};

typedef struct {
    uint32_t *word;
    int       words, cap;
} Spv;

static void spv_grow(Spv *s) {
    s->cap = s->cap ? 2 * s->cap : 1024;
    s->word = realloc(s->word, (size_t)s->cap * sizeof(uint32_t));
}

static void spv_word(Spv *s, uint32_t w) {
    if (s->words >= s->cap) { spv_grow(s); }
    s->word[s->words++] = w;
}

static void spv_op(Spv *s, int opcode, int word_count) {
    spv_word(s, (uint32_t)((word_count << 16) | (opcode & 0xFFFF)));
}

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
    uint32_t t_void, t_bool, t_u32, t_i32, t_f32, t_f16;
    uint32_t t_uvec2;
    uint32_t t_uvec3;
    uint32_t t_uvec4;
    uint32_t t_fvec2;
    uint32_t t_fn_void;
    uint32_t t_ptr_input_uvec3;
    uint32_t t_ptr_ssbo_u32;
    uint32_t t_ptr_ssbo_uvec2;
    uint32_t t_ptr_ssbo_f16;
    uint32_t t_ptr_uniform_u32;     // pointer (Uniform SC) to u32, leaf of vec4-packed uniform
    uint32_t t_ptr_push_u32;
    uint32_t t_rta_u32;              // RuntimeArray of u32
    uint32_t t_rta_uvec2;           // RuntimeArray of uvec2
    uint32_t t_rta_f16;             // RuntimeArray of f16
    uint32_t t_struct_rta_u32;      // struct { RuntimeArray<u32> }
    uint32_t t_struct_rta_uvec2;    // struct { RuntimeArray<uvec2> }
    uint32_t t_struct_rta_f16;      // struct { RuntimeArray<f16> }
    uint32_t t_ptr_ssbo_struct;      // pointer to struct { RuntimeArray<u32> }
    uint32_t t_ptr_ssbo_struct_uvec2;
    uint32_t t_ptr_ssbo_struct_f16; // pointer to struct { RuntimeArray<f16> }

    uint32_t ext_glsl;
    uint32_t t_ptr_func_u32; int :32;

    uint32_t *v_ssbo;      // one per buffer (total_bufs)
    uint32_t v_global_id;  // gl_GlobalInvocationID
    uint32_t v_push;       // push constant block variable

    // Metadata push constant layout (no user uniforms — those go through the
    // uniform ring as a storage buffer at binding 0):
    //   [0] = w, [1] = x0, [2] = y0,
    //   [3..3+total_bufs-1] = buf_count, [3+total_bufs..3+2*total_bufs-1] = buf_stride
    int total_bufs;
    int push_words;

    // Constant cache for small integer constants.
    uint32_t c_0, c_1, c_2, c_3, c_8, c_16;
    uint32_t c_0f;  // float 0.0
    uint32_t c_allones;  // 0xFFFFFFFF

    // Per-IR-instruction result IDs. Some instructions produce multiple results
    // (e.g. load_16x4 produces 4 channels).
    uint32_t *val;        // val[i] = SPIR-V ID for ir_inst i (channel 0)
    uint32_t *val_1;      // channel 1 for 16x4 loads
    uint32_t *val_2;      // channel 2
    uint32_t *val_3;      // channel 3

    // Track which values are float type (vs uint).
    _Bool *is_f;

    // Per-buffer flag: true if the buffer needs 16-bit typed access.
    _Bool *buf_is_16;
    _Bool *buf_is_16x4;
    int    has_16, has_16x4;

    // Per-buffer flag: true if the buffer is a umbra_bind_uniforms binding
    // (NULL .buf, fixed slot count known at IR build time).  Such bindings are
    // emitted as Uniform-storage blocks with vec4<u32>-packed arrays
    // (std140-compatible) instead of StorageBuffer + RuntimeArray.
    _Bool    *buf_is_uniform;
    int      *buf_uniform_slots;        // u32 slot count per uniform binding
    uint32_t *t_ptr_uniform_struct;     // per-binding pointer-to-struct type ID

    // Constant deduplication cache.
    struct { uint32_t type, value, id; } *const_cache;
    int consts, consts_cap;
} SpvBuilder;

static uint32_t spv_id(SpvBuilder *b) { return b->next_id++; }

static uint32_t spv_const(SpvBuilder *b, uint32_t type, uint32_t bits) {
    for (int i = 0; i < b->consts; i++) {
        if (b->const_cache[i].type == type && b->const_cache[i].value == bits) {
            return b->const_cache[i].id;
        }
    }
    uint32_t id = spv_id(b);
    spv_op(&b->types, SpvOpConstant, 4);
    spv_word(&b->types, type);
    spv_word(&b->types, id);
    spv_word(&b->types, bits);
    if (b->consts >= b->consts_cap) {
        b->consts_cap = b->consts_cap ? 2 * b->consts_cap : 64;
        b->const_cache = realloc(b->const_cache,
                                 (size_t)b->consts_cap * sizeof *b->const_cache);
    }
    b->const_cache[b->consts].type  = type;
    b->const_cache[b->consts].value = bits;
    b->const_cache[b->consts].id    = id;
    b->consts++;
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

// OpCompositeExtract: extract a scalar from a composite.
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

// OpCompositeConstruct: build a 2-element composite.
static uint32_t spv_composite_construct_2(SpvBuilder *b, uint32_t type,
                                           uint32_t a, uint32_t c_b) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpCompositeConstruct, 5);
    spv_word(&b->func, type);
    spv_word(&b->func, id);
    spv_word(&b->func, a);
    spv_word(&b->func, c_b);
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

// FP add/sub/mul that refuse to let the backend compiler contract them
// into an fma.  When `flags & SPIRV_NO_CONTRACT` we lower each op to an
// explicit GLSLstd450 Fma with a trivial factor, matching SPIRV-Cross's
// spvFAdd/spvFMul helpers -- needed for backends (notably naga) that
// don't honor ContractionOff.
static uint32_t spv_fadd(SpvBuilder *b, int flags, uint32_t xf, uint32_t yf) {
    if (flags & SPIRV_NO_CONTRACT) {
        return spv_glsl_3(b, b->t_f32, GLSLstd450Fma,
                          spv_const_f32(b, 1.0f), xf, yf);
    }
    return spv_binop(b, SpvOpFAdd, b->t_f32, xf, yf);
}
static uint32_t spv_fsub(SpvBuilder *b, int flags, uint32_t xf, uint32_t yf) {
    if (flags & SPIRV_NO_CONTRACT) {
        return spv_glsl_3(b, b->t_f32, GLSLstd450Fma,
                          spv_const_f32(b, -1.0f), yf, xf);
    }
    return spv_binop(b, SpvOpFSub, b->t_f32, xf, yf);
}
static uint32_t spv_fmul(SpvBuilder *b, int flags, uint32_t xf, uint32_t yf) {
    if (flags & SPIRV_NO_CONTRACT) {
        return spv_glsl_3(b, b->t_f32, GLSLstd450Fma,
                          xf, yf, spv_const_f32(b, 0.0f));
    }
    return spv_binop(b, SpvOpFMul, b->t_f32, xf, yf);
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

// Get the SPIR-V result ID for an ir_inst operand, handling channel extraction.
static uint32_t get_val(SpvBuilder *b, val v) {
    int const id = v.id,
              ch = (int)v.chan;
    switch (ch) {
        case 0: return b->val[id];
        case 1: return b->val_1[id];
        case 2: return b->val_2[id];
        case 3: return b->val_3[id];
    }
    __builtin_unreachable();
}

static int get_id(val v) { return v.id; }

static int resolve_ptr(SpvBuilder *b, struct ir_inst const *inst) {
    (void)b;
    return inst->ptr.bits;
}

// Load a metadata word from the push constant block at a given word offset.
// The push block carries only backend-side metadata (w, x0, y0, buf_count,
// buf_stride); user uniforms ride in storage buffer binding 0 via the ring.
// Push layout is struct { uint data[N]; }, so we need two indices:
// member 0, then array element.
static uint32_t load_meta_u32(SpvBuilder *b, uint32_t offset_id) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_push_u32, b->v_push, b->c_0, offset_id);
    return spv_load(b, b->t_u32, ptr);
}

static uint32_t spv_access_chain_3(SpvBuilder *b, uint32_t ptr_type,
                                    uint32_t base, uint32_t ix0, uint32_t ix1,
                                    uint32_t ix2) {
    uint32_t id = spv_id(b);
    spv_op(&b->func, SpvOpAccessChain, 7);
    spv_word(&b->func, ptr_type);
    spv_word(&b->func, id);
    spv_word(&b->func, base);
    spv_word(&b->func, ix0);
    spv_word(&b->func, ix1);
    spv_word(&b->func, ix2);
    return id;
}

// Load a u32 element by *flat slot index* (0-based, ignoring vec4 packing).
// For SSBO bindings the access is direct; for Uniform bindings the slot
// is split into (slot/4, slot%4) and indexed through the vec4<u32> array.
static uint32_t load_buf_u32(SpvBuilder *b, int buf_idx, uint32_t elem_idx) {
    if (b->buf_is_uniform[buf_idx]) {
        uint32_t const vec_ix = spv_binop(b, SpvOpShiftRightLogical, b->t_u32,
                                          elem_idx, b->c_2);
        uint32_t const lane   = spv_binop(b, SpvOpBitwiseAnd, b->t_u32,
                                          elem_idx, b->c_3);
        uint32_t const ptr    = spv_access_chain_3(b, b->t_ptr_uniform_u32,
                                                   b->v_ssbo[buf_idx],
                                                   b->c_0, vec_ix, lane);
        return spv_load(b, b->t_u32, ptr);
    }
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u32,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, elem_idx);
    return spv_load(b, b->t_u32, ptr);
}

// Load by a slot index known at emit time.  When the binding is a uniform
// block we split (slot/4, slot%4) into constants here so the access chain
// stays fully constant-indexed.
static uint32_t load_buf_u32_const_slot(SpvBuilder *b, int buf_idx, int slot) {
    if (b->buf_is_uniform[buf_idx]) {
        uint32_t const vec_ix = spv_const_u32(b, (uint32_t)(slot >> 2));
        uint32_t const lane   = spv_const_u32(b, (uint32_t)(slot & 3));
        uint32_t const ptr    = spv_access_chain_3(b, b->t_ptr_uniform_u32,
                                                   b->v_ssbo[buf_idx],
                                                   b->c_0, vec_ix, lane);
        return spv_load(b, b->t_u32, ptr);
    }
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u32,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, spv_const_u32(b, (uint32_t)slot));
    return spv_load(b, b->t_u32, ptr);
}

// Load a u32 element from an SSBO binding.  Use load_buf_u32 when the binding
// might be a uniform-class binding (op_uniform_32, op_gather_uniform_32).
static uint32_t load_ssbo_u32(SpvBuilder *b, int buf_idx, uint32_t elem_idx) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u32,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, elem_idx);
    return spv_load(b, b->t_u32, ptr);
}

// Store to SSBO[buf_idx] at element_index.
static void store_ssbo_u32(SpvBuilder *b, int buf_idx, uint32_t elem_idx, uint32_t value) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u32,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, elem_idx);
    spv_store(b, ptr, value);
}

static uint32_t load_ssbo_uvec2(SpvBuilder *b, int buf_idx, uint32_t elem_idx) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_uvec2,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, elem_idx);
    return spv_load(b, b->t_uvec2, ptr);
}

static void store_ssbo_uvec2(SpvBuilder *b, int buf_idx, uint32_t elem_idx, uint32_t value) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_uvec2,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, elem_idx);
    spv_store(b, ptr, value);
}

// Load u16 from SSBO[buf_idx] at element_index, zero-extended to u32.
// Buffer is RuntimeArray<f16>; smuggle integer bits through f16 storage
// via PackHalf2x16 to recover the raw 16-bit pattern as u32.
static uint32_t load_ssbo_u16(SpvBuilder *b, int buf_idx, uint32_t elem_idx) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_f16,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, elem_idx);
    uint32_t const h = spv_load(b, b->t_f16, ptr);
    uint32_t const f = spv_unop(b, SpvOpFConvert, b->t_f32, h);
    uint32_t const v2 = spv_composite_construct_2(b, b->t_fvec2, f, b->c_0f);
    return spv_glsl_1(b, b->t_u32, 58 /*PackHalf2x16*/, v2);
}

// Store u16 to SSBO[buf_idx] at element_index, truncating from u32.
// Smuggle integer bits through f16 storage via UnpackHalf2x16.
static void store_ssbo_u16(SpvBuilder *b, int buf_idx, uint32_t elem_idx, uint32_t value) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_f16,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, elem_idx);
    uint32_t const v2 = spv_glsl_1(b, b->t_fvec2, 62 /*UnpackHalf2x16*/, value);
    uint32_t const f  = spv_composite_extract(b, b->t_f32, v2, 0);
    uint32_t const h  = spv_unop(b, SpvOpFConvert, b->t_f16, f);
    spv_store(b, ptr, h);
}

// Compute linear address for row-structured buffer:
//   addr = y * buf_stride[buf_idx] + x
static uint32_t compute_addr(SpvBuilder *b, uint32_t x, uint32_t y, int buf_idx) {
    uint32_t const stride_off = spv_const_u32(b, (uint32_t)(3 + b->total_bufs + buf_idx));
    uint32_t const stride     = load_meta_u32(b, stride_off);
    uint32_t const row_off    = spv_binop(b, SpvOpIMul, b->t_u32, y, stride);
    return spv_binop(b, SpvOpIAdd, b->t_u32, row_off, x);
}

// buf_count[buf_idx] (element count) is at push offset 3 + buf_idx.
// Returns (clamped_index, oob_mask) where oob_mask is 0xFFFFFFFF for in-bounds, 0 for OOB.
static void gather_safe(SpvBuilder *b, uint32_t ix_val, int buf_idx,
                         uint32_t *out_idx, uint32_t *out_mask) {
    uint32_t const limit_off = spv_const_u32(b, (uint32_t)(3 + buf_idx));
    uint32_t const limit     = load_meta_u32(b, limit_off);

    uint32_t const limit_minus_1 = spv_binop(b, SpvOpISub, b->t_u32, limit, b->c_1);
    uint32_t const clamped = spv_glsl_2(b, b->t_u32, 38 /*UMin*/, ix_val, limit_minus_1);

    uint32_t const in_bounds = spv_binop(b, SpvOpULessThan, b->t_bool, ix_val, limit);
    *out_mask = spv_select(b, b->t_u32, in_bounds, b->c_allones, b->c_0);
    *out_idx = clamped;
}

// fp16 ↔ fp32 bit-level conversion for integer 16-bit values held as u32.
// Uses GLSL.std.450 UnpackHalf2x16/PackHalf2x16 to reinterpret bits.
static uint32_t spv_f16_to_f32(SpvBuilder *b, uint32_t u32_val) {
    uint32_t const v2 = spv_glsl_1(b, b->t_fvec2, 62 /*UnpackHalf2x16*/, u32_val);
    return spv_composite_extract(b, b->t_f32, v2, 0);
}

static uint32_t spv_f32_to_f16(SpvBuilder *b, uint32_t f32_val) {
    uint32_t const v2 = spv_composite_construct_2(b, b->t_fvec2, f32_val, b->c_0f);
    return spv_glsl_1(b, b->t_u32, 58 /*PackHalf2x16*/, v2);
}

static _Bool produces_float(enum op op) {
    return op == op_add_f32     || op == op_sub_f32
        || op == op_mul_f32     || op == op_div_f32
        || op == op_min_f32     || op == op_max_f32
        || op == op_sqrt_f32    || op == op_abs_f32    || op == op_square_f32
        || op == op_round_f32   || op == op_floor_f32  || op == op_ceil_f32
        || op == op_fma_f32     || op == op_fms_f32
        || op == op_square_add_f32 || op == op_square_sub_f32
        || op == op_add_f32_imm || op == op_sub_f32_imm
        || op == op_mul_f32_imm || op == op_div_f32_imm
        || op == op_min_f32_imm || op == op_max_f32_imm
        || op == op_f32_from_i32
        || op == op_f32_from_f16;
}

// Build the full SPIR-V binary for a flat IR.
struct spirv_result build_spirv(struct umbra_flat_ir const *ir,
                               int flags) {
    struct spirv_result result = {0};

    struct umbra_flat_ir *resolved = flat_ir_resolve(ir, JOIN_PREFER_IMM);
    ir = resolved;

    SpvBuilder B;
    memset(&B, 0, sizeof B);
    B.next_id = 1; // 0 is reserved

    int max_ptr = -1;
    for (int i = 0; i < ir->insts; i++) {
        if (op_has_ptr(ir->inst[i].op) && ir->inst[i].ptr.bits > max_ptr) {
            max_ptr = ir->inst[i].ptr.bits;
        }
    }
    result.max_ptr = max_ptr;

    int const total_bufs = max_ptr + 1;
    result.total_bufs = total_bufs;
    B.total_bufs = total_bufs;

    B.buf_is_16   = calloc((size_t)(total_bufs + 1), sizeof *B.buf_is_16);
    B.buf_is_16x4 = calloc((size_t)(total_bufs + 1), sizeof *B.buf_is_16x4);
    for (int i = 0; i < ir->insts; i++) {
        enum op op = ir->inst[i].op;
        int p = op_has_ptr(op) ? ir->inst[i].ptr.bits : -1;
        if (op == op_load_16 || op == op_store_16 || op == op_gather_16
         || op == op_load_16x4_planar || op == op_store_16x4_planar) {
            B.buf_is_16[p] = 1;
            B.has_16 = 1;
        }
        if (op == op_load_16x4 || op == op_store_16x4) {
            B.buf_is_16x4[p] = 1;
            B.has_16x4 = 1;
        }
    }

    // Classify umbra_bind_uniforms bindings: NULL .buf, fixed slot count.
    B.buf_is_uniform    = calloc((size_t)(total_bufs + 1), sizeof *B.buf_is_uniform);
    B.buf_uniform_slots = calloc((size_t)(total_bufs + 1), sizeof *B.buf_uniform_slots);
    for (int i = 0; i < ir->bindings; i++) {
        int const p = ir->binding[i].ix;
        if (0 <= p && p < total_bufs && ir->binding[i].buf == NULL) {
            B.buf_is_uniform   [p] = 1;
            B.buf_uniform_slots[p] = ir->binding[i].uniforms.count;
        }
    }

    uint8_t *buf_rw        = calloc((size_t)(total_bufs + 1), sizeof *buf_rw);
    uint8_t *buf_shift     = calloc((size_t)(total_bufs + 1), sizeof *buf_shift);
    uint8_t *buf_row_shift = calloc((size_t)(total_bufs + 1), sizeof *buf_row_shift);
    for (int i = 0; i < ir->insts; i++) {
        if (op_has_ptr(ir->inst[i].op)) {
            int p = ir->inst[i].ptr.bits;
            buf_rw[p] |= op_is_store(ir->inst[i].op) ? BUF_WRITTEN : BUF_READ;
            // TODO: gather_16 sets buf_shift but leaves buf_row_shift = 0,
            // while metal.c's equivalent table lumps gather_16 with load_16/
            // store_16 and sets row_shift = 1.  Probably a bug on one side or
            // the other -- if a buffer is reachable through both a gather_16
            // and a load_16/store_16 the two backends will disagree on the
            // buffer's 2D stride accounting.  Reconcile by picking one
            // behavior (likely row_shift = 1 for gather_16 too, matching
            // metal.c) and ideally factor this table into a shared helper
            // near op.h so it can't drift again.
            if      (ir->inst[i].op == op_gather_16)        { buf_shift[p] = 1; }
            else if (ir->inst[i].op == op_load_16x4_planar
                  || ir->inst[i].op == op_store_16x4_planar) { buf_shift[p] = 1; buf_row_shift[p] = 1; }
            else if (ir->inst[i].op == op_load_16x4
                  || ir->inst[i].op == op_store_16x4)        { buf_shift[p] = 3; buf_row_shift[p] = 3; }
            else if (ir->inst[i].op == op_load_16
                  || ir->inst[i].op == op_store_16)          { buf_shift[p] = 1; buf_row_shift[p] = 1; }
            else                                             { buf_shift[p] = 2; buf_row_shift[p] = 2; }
        }
    }
    uint8_t *buf_is_uniform_out = calloc((size_t)(total_bufs + 1),
                                         sizeof *buf_is_uniform_out);
    for (int p = 0; p < total_bufs; p++) {
        buf_is_uniform_out[p] = (uint8_t)B.buf_is_uniform[p];
    }
    result.buf_rw         = buf_rw;
    result.buf_shift      = buf_shift;
    result.buf_row_shift  = buf_row_shift;
    result.buf_is_uniform = buf_is_uniform_out;

    // Push constant layout: w, x0, y0, buf_count[total_bufs], buf_stride[total_bufs].
    // User uniforms (buf[0]) go through the per-batch uniform ring as a
    // storage buffer at descriptor binding 0; only this small backend-side
    // metadata rides in push constants.
    int push_words = 3 + 2 * total_bufs;
    result.push_words = push_words;
    B.push_words = push_words;

    spv_op(&B.caps, SpvOpCapability, 2);
    spv_word(&B.caps, SpvCapabilityShader);

    if (B.has_16) {
        spv_op(&B.caps, SpvOpCapability, 2);
        spv_word(&B.caps, SpvCapabilityFloat16);
    }

    if (flags & SPIRV_FLOAT_CONTROLS) {
        spv_op(&B.caps, SpvOpCapability, 2);
        spv_word(&B.caps, SpvCapabilitySignedZeroInfNanPreserve);

        // SPV_KHR_float_controls: SignedZeroInfNanPreserve for float32 makes
        // MoltenVK/SPIRV-Cross clear NotNaN|NotInf|NSZ from fpFastMathFlags,
        // which produces MTLMathModeSafe + precise:: transcendentals — matching
        // our Metal backend.
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

    // SPV_KHR_storage_buffer_storage_class: required for StorageBuffer SC.
    {
        char const name[] = "SPV_KHR_storage_buffer_storage_class";
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

    spv_op(&B.mem_model, SpvOpMemoryModel, 3);
    spv_word(&B.mem_model, SpvAddressingModelLogical);
    spv_word(&B.mem_model, SpvMemoryModelGLSL450);

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

    if (B.has_16) {
        B.t_f16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeFloat, 3);
        spv_word(&B.types, B.t_f16);
        spv_word(&B.types, 16);
    }

    B.t_uvec2 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeVector, 4);
    spv_word(&B.types, B.t_uvec2);
    spv_word(&B.types, B.t_u32);
    spv_word(&B.types, 2);

    B.t_uvec3 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeVector, 4);
    spv_word(&B.types, B.t_uvec3);
    spv_word(&B.types, B.t_u32);
    spv_word(&B.types, 3);

    B.t_uvec4 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeVector, 4);
    spv_word(&B.types, B.t_uvec4);
    spv_word(&B.types, B.t_u32);
    spv_word(&B.types, 4);

    B.t_fvec2 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeVector, 4);
    spv_word(&B.types, B.t_fvec2);
    spv_word(&B.types, B.t_f32);
    spv_word(&B.types, 2);

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

    // Decorate struct as Block with StorageBuffer storage class (SPIR-V 1.3).
    spv_op(&B.decor, SpvOpDecorate, 3);
    spv_word(&B.decor, B.t_struct_rta_u32);
    spv_word(&B.decor, SpvDecorationBlock);

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
    spv_word(&B.types, SpvStorageClassStorageBuffer);
    spv_word(&B.types, B.t_struct_rta_u32);

    // Pointer to u32 in SSBO.
    B.t_ptr_ssbo_u32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, B.t_ptr_ssbo_u32);
    spv_word(&B.types, SpvStorageClassStorageBuffer);
    spv_word(&B.types, B.t_u32);

    if (B.has_16x4) {
        B.t_rta_uvec2 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeRuntimeArray, 3);
        spv_word(&B.types, B.t_rta_uvec2);
        spv_word(&B.types, B.t_uvec2);

        spv_op(&B.decor, SpvOpDecorate, 4);
        spv_word(&B.decor, B.t_rta_uvec2);
        spv_word(&B.decor, SpvDecorationArrayStride);
        spv_word(&B.decor, 8);

        B.t_struct_rta_uvec2 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeStruct, 3);
        spv_word(&B.types, B.t_struct_rta_uvec2);
        spv_word(&B.types, B.t_rta_uvec2);

        spv_op(&B.decor, SpvOpDecorate, 3);
        spv_word(&B.decor, B.t_struct_rta_uvec2);
        spv_word(&B.decor, SpvDecorationBlock);

        spv_op(&B.decor, SpvOpMemberDecorate, 5);
        spv_word(&B.decor, B.t_struct_rta_uvec2);
        spv_word(&B.decor, 0);
        spv_word(&B.decor, SpvDecorationOffset);
        spv_word(&B.decor, 0);

        B.t_ptr_ssbo_struct_uvec2 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_ssbo_struct_uvec2);
        spv_word(&B.types, SpvStorageClassStorageBuffer);
        spv_word(&B.types, B.t_struct_rta_uvec2);

        B.t_ptr_ssbo_uvec2 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_ssbo_uvec2);
        spv_word(&B.types, SpvStorageClassStorageBuffer);
        spv_word(&B.types, B.t_uvec2);
    }

    // Per-binding Uniform-storage types for umbra_bind_uniforms bindings.
    // Layout is std140-compatible: struct { uvec4 arr[ceil(slots/4)]; }, with
    // ArrayStride 16.  Element s is addressed as arr[s>>2][s&3].
    B.t_ptr_uniform_struct = calloc((size_t)total_bufs, sizeof *B.t_ptr_uniform_struct);
    _Bool any_uniform = 0;
    for (int p = 0; p < total_bufs; p++) {
        if (B.buf_is_uniform[p]) {
            any_uniform = 1;
            int      const slots     = B.buf_uniform_slots[p];
            int      const vec_count = (slots + 3) / 4;
            uint32_t const c_count   = spv_const_u32(&B, (uint32_t)vec_count);

            uint32_t const t_arr = spv_id(&B);
            spv_op(&B.types, SpvOpTypeArray, 4);
            spv_word(&B.types, t_arr);
            spv_word(&B.types, B.t_uvec4);
            spv_word(&B.types, c_count);

            spv_op(&B.decor, SpvOpDecorate, 4);
            spv_word(&B.decor, t_arr);
            spv_word(&B.decor, SpvDecorationArrayStride);
            spv_word(&B.decor, 16);

            uint32_t const t_struct = spv_id(&B);
            spv_op(&B.types, SpvOpTypeStruct, 3);
            spv_word(&B.types, t_struct);
            spv_word(&B.types, t_arr);

            spv_op(&B.decor, SpvOpDecorate, 3);
            spv_word(&B.decor, t_struct);
            spv_word(&B.decor, SpvDecorationBlock);

            spv_op(&B.decor, SpvOpMemberDecorate, 5);
            spv_word(&B.decor, t_struct);
            spv_word(&B.decor, 0);
            spv_word(&B.decor, SpvDecorationOffset);
            spv_word(&B.decor, 0);

            B.t_ptr_uniform_struct[p] = spv_id(&B);
            spv_op(&B.types, SpvOpTypePointer, 4);
            spv_word(&B.types, B.t_ptr_uniform_struct[p]);
            spv_word(&B.types, SpvStorageClassUniform);
            spv_word(&B.types, t_struct);
        }
    }
    if (any_uniform) {
        B.t_ptr_uniform_u32 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_uniform_u32);
        spv_word(&B.types, SpvStorageClassUniform);
        spv_word(&B.types, B.t_u32);
    }

    if (B.has_16) {
        B.t_rta_f16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeRuntimeArray, 3);
        spv_word(&B.types, B.t_rta_f16);
        spv_word(&B.types, B.t_f16);

        spv_op(&B.decor, SpvOpDecorate, 4);
        spv_word(&B.decor, B.t_rta_f16);
        spv_word(&B.decor, SpvDecorationArrayStride);
        spv_word(&B.decor, 2);

        B.t_struct_rta_f16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeStruct, 3);
        spv_word(&B.types, B.t_struct_rta_f16);
        spv_word(&B.types, B.t_rta_f16);

        spv_op(&B.decor, SpvOpDecorate, 3);
        spv_word(&B.decor, B.t_struct_rta_f16);
        spv_word(&B.decor, SpvDecorationBlock);

        spv_op(&B.decor, SpvOpMemberDecorate, 5);
        spv_word(&B.decor, B.t_struct_rta_f16);
        spv_word(&B.decor, 0);
        spv_word(&B.decor, SpvDecorationOffset);
        spv_word(&B.decor, 0);

        B.t_ptr_ssbo_struct_f16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_ssbo_struct_f16);
        spv_word(&B.types, SpvStorageClassStorageBuffer);
        spv_word(&B.types, B.t_struct_rta_f16);

        B.t_ptr_ssbo_f16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_ssbo_f16);
        spv_word(&B.types, SpvStorageClassStorageBuffer);
        spv_word(&B.types, B.t_f16);
    }

    // Push constant block: struct { uint data[push_words]; }
    // We model it as struct { uint[push_words] }.
    uint32_t t_arr_push   = spv_id(&B);
    uint32_t c_push_count = spv_const_u32(&B, (uint32_t)push_words);
    spv_op(&B.types, SpvOpTypeArray, 4);
    spv_word(&B.types, t_arr_push);
    spv_word(&B.types, B.t_u32);
    spv_word(&B.types, c_push_count);

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

    uint32_t push_sc = (flags & SPIRV_PUSH_VIA_SSBO) ? (uint32_t)SpvStorageClassStorageBuffer
                                                       : (uint32_t)SpvStorageClassPushConstant;

    uint32_t t_ptr_push_struct = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, t_ptr_push_struct);
    spv_word(&B.types, push_sc);
    spv_word(&B.types, t_push_struct);

    B.t_ptr_push_u32 = spv_id(&B);
    spv_op(&B.types, SpvOpTypePointer, 4);
    spv_word(&B.types, B.t_ptr_push_u32);
    spv_word(&B.types, push_sc);
    spv_word(&B.types, B.t_u32);

    if (ir->vars > 0) {
        B.t_ptr_func_u32 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_func_u32);
        spv_word(&B.types, SpvStorageClassFunction);
        spv_word(&B.types, B.t_u32);
    }

    B.c_0 = spv_const_u32(&B, 0);
    B.c_1 = spv_const_u32(&B, 1);
    B.c_2 = spv_const_u32(&B, 2);
    B.c_3 = spv_const_u32(&B, 3);
    B.c_8 = spv_const_u32(&B, 8);
    B.c_16 = spv_const_u32(&B, 16);
    B.c_0f = spv_const_f32(&B, 0.0f);
    B.c_allones = spv_const_u32(&B, 0xFFFFFFFFu);

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

    // SSBO/Uniform variables: one per buffer, descriptor binding = buffer index.
    B.v_ssbo = calloc((size_t)total_bufs, sizeof *B.v_ssbo);
    for (int i = 0; i < total_bufs; i++) {
        B.v_ssbo[i] = spv_id(&B);
        uint32_t const sc = B.buf_is_uniform[i] ? (uint32_t)SpvStorageClassUniform
                                                : (uint32_t)SpvStorageClassStorageBuffer;
        uint32_t const ptr_type = B.buf_is_uniform[i] ? B.t_ptr_uniform_struct[i]
                                : B.buf_is_16     [i] ? B.t_ptr_ssbo_struct_f16
                                : B.buf_is_16x4   [i] ? B.t_ptr_ssbo_struct_uvec2
                                :                       B.t_ptr_ssbo_struct;
        spv_op(&B.globals, SpvOpVariable, 4);
        spv_word(&B.globals, ptr_type);
        spv_word(&B.globals, B.v_ssbo[i]);
        spv_word(&B.globals, sc);

        spv_op(&B.decor, SpvOpDecorate, 4);
        spv_word(&B.decor, B.v_ssbo[i]);
        spv_word(&B.decor, SpvDecorationDescriptorSet);
        spv_word(&B.decor, 0);

        spv_op(&B.decor, SpvOpDecorate, 4);
        spv_word(&B.decor, B.v_ssbo[i]);
        spv_word(&B.decor, SpvDecorationBinding);
        spv_word(&B.decor, (uint32_t)i);
    }

    // Push constant / push-via-SSBO variable.
    B.v_push = spv_id(&B);
    spv_op(&B.globals, SpvOpVariable, 4);
    spv_word(&B.globals, t_ptr_push_struct);
    spv_word(&B.globals, B.v_push);
    spv_word(&B.globals, push_sc);
    if (flags & SPIRV_PUSH_VIA_SSBO) {
        spv_op(&B.decor, SpvOpDecorate, 4);
        spv_word(&B.decor, B.v_push);
        spv_word(&B.decor, SpvDecorationDescriptorSet);
        spv_word(&B.decor, 0);

        spv_op(&B.decor, SpvOpDecorate, 4);
        spv_word(&B.decor, B.v_push);
        spv_word(&B.decor, SpvDecorationBinding);
        spv_word(&B.decor, (uint32_t)total_bufs);
    }

    uint32_t *v_vars = NULL;

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

        spv_op(&B.exec_mode, SpvOpExecutionMode, 6);
        spv_word(&B.exec_mode, fn_main);
        spv_word(&B.exec_mode, SpvExecutionModeLocalSize);
        spv_word(&B.exec_mode, SPIRV_WG_SIZE);
        spv_word(&B.exec_mode, 1);
        spv_word(&B.exec_mode, 1);

        if (flags & SPIRV_FLOAT_CONTROLS) {
            spv_op(&B.exec_mode, SpvOpExecutionMode, 4);
            spv_word(&B.exec_mode, fn_main);
            spv_word(&B.exec_mode, SpvExecutionModeSignedZeroInfNanPreserve);
            spv_word(&B.exec_mode, 32); // float32
        }

        // OpFunction
        spv_op(&B.func, SpvOpFunction, 5);
        spv_word(&B.func, B.t_void);
        spv_word(&B.func, fn_main);
        spv_word(&B.func, 0); // None function control
        spv_word(&B.func, B.t_fn_void);

        uint32_t label_entry = spv_id(&B);
        spv_op(&B.func, SpvOpLabel, 2);
        spv_word(&B.func, label_entry);

        // Function-scoped variables (including loop induction variable).
        if (ir->vars > 0) {
            v_vars = calloc((size_t)ir->vars, sizeof *v_vars);
            for (int vi = 0; vi < ir->vars; vi++) {
                v_vars[vi] = spv_id(&B);
                spv_op(&B.func, SpvOpVariable, 5);
                spv_word(&B.func, B.t_ptr_func_u32);
                spv_word(&B.func, v_vars[vi]);
                spv_word(&B.func, SpvStorageClassFunction);
                spv_word(&B.func, B.c_0);
            }
        }

        // Load gl_GlobalInvocationID.
        uint32_t gid = spv_load(&B, B.t_uvec3, B.v_global_id);
        uint32_t gid_x = spv_composite_extract(&B, B.t_u32, gid, 0);
        uint32_t gid_y = spv_composite_extract(&B, B.t_u32, gid, 1);

        // Load w from push constants [0].
        uint32_t w_val = load_meta_u32(&B, B.c_0);

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
        uint32_t x0 = load_meta_u32(&B, B.c_1);
        uint32_t y0 = load_meta_u32(&B, B.c_2);

        uint32_t x_coord = spv_binop(&B, SpvOpIAdd, B.t_u32, x0, gid_x);
        uint32_t y_coord = spv_binop(&B, SpvOpIAdd, B.t_u32, y0, gid_y);

        B.val   = calloc((size_t)(ir->insts + 1), sizeof *B.val);
        B.val_1 = calloc((size_t)(ir->insts + 1), sizeof *B.val_1);
        B.val_2 = calloc((size_t)(ir->insts + 1), sizeof *B.val_2);
        B.val_3 = calloc((size_t)(ir->insts + 1), sizeof *B.val_3);
        B.is_f  = calloc((size_t)(ir->insts + 1), sizeof *B.is_f);

        // Mark float-producing ops.
        for (int i = 0; i < ir->insts; i++) {
            B.is_f[i] = produces_float(ir->inst[i].op);
        }

        for (int i = 0; i < ir->insts; i++) {
            struct ir_inst const *inst = &ir->inst[i];

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
                case op_join: __builtin_unreachable();

                case op_uniform_32: {
                    int p = resolve_ptr(&B, inst);
                    B.val[i] = load_buf_u32_const_slot(&B, p, inst->imm);
                } break;

                case op_load_32: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p);
                    B.val[i] = load_ssbo_u32(&B, p, addr);
                } break;

                case op_store_32: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p);
                    uint32_t v = as_u32(&B, get_val(&B, inst->y), yid);
                    store_ssbo_u32(&B, p, addr, v);
                } break;

                case op_load_16: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr16 = compute_addr(&B, x_coord, y_coord, p);
                    B.val[i] = load_ssbo_u16(&B, p, addr16);
                } break;

                case op_store_16: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr16 = compute_addr(&B, x_coord, y_coord, p);
                    uint32_t v = B.is_f[yid] ? spv_f32_to_f16(&B, get_val(&B, inst->y))
                                             : as_u32(&B, get_val(&B, inst->y), yid);
                    store_ssbo_u16(&B, p, addr16, v);
                } break;

                case op_gather_uniform_32:
                case op_gather_32: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t ix_val = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t safe_idx, mask;
                    gather_safe(&B, ix_val, p, &safe_idx, &mask);
                    uint32_t raw = load_buf_u32(&B, p, safe_idx);
                    B.val[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, raw, mask);
                } break;

                case op_gather_16: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t ix_val = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t safe_idx, mask;
                    gather_safe(&B, ix_val, p, &safe_idx, &mask);
                    uint32_t raw = load_ssbo_u16(&B, p, safe_idx);
                    B.val[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, raw, mask);
                } break;

                case op_load_8x4: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p);
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
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p);
                    uint32_t r  = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t g  = as_u32(&B, get_val(&B, inst->y), yid);
                    uint32_t b  = as_u32(&B, get_val(&B, inst->z), get_id(inst->z));
                    uint32_t a  = as_u32(&B, get_val(&B, inst->w), get_id(inst->w));
                    uint32_t g_sh = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, g, B.c_8);
                    uint32_t b_sh = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, b, B.c_16);
                    uint32_t c_24 = spv_const_u32(&B, 24);
                    uint32_t a_sh = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, a, c_24);
                    uint32_t rg  = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, r, g_sh);
                    uint32_t rgb = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, rg, b_sh);
                    uint32_t px  = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, rgb, a_sh);
                    store_ssbo_u32(&B, p, addr, px);
                } break;

                case op_load_16x4: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p);
                    uint32_t px = load_ssbo_uvec2(&B, p, addr);
                    uint32_t w0 = spv_composite_extract(&B, B.t_u32, px, 0);
                    uint32_t w1 = spv_composite_extract(&B, B.t_u32, px, 1);

                    uint32_t c_0xFFFF = spv_const_u32(&B, 0xFFFFu);
                    B.val[i]   = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, w0, c_0xFFFF);
                    B.val_1[i] = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, w0, B.c_16);
                    B.val_2[i] = spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, w1, c_0xFFFF);
                    B.val_3[i] = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, w1, B.c_16);
                } break;

                case op_load_16x4_planar: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t lim_off = spv_const_u32(&B, (uint32_t)(3 + p));
                    uint32_t total  = load_meta_u32(&B, lim_off);
                    uint32_t c4     = spv_const_u32(&B, 4);
                    uint32_t ps_f16 = spv_binop(&B, SpvOpUDiv, B.t_u32, total, c4);

                    uint32_t stride_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t stride = load_meta_u32(&B, stride_off);
                    uint32_t addr = spv_binop(&B, SpvOpIMul, B.t_u32, y_coord, stride);
                    addr = spv_binop(&B, SpvOpIAdd, B.t_u32, addr, x_coord);

                    for (int ch = 0; ch < 4; ch++) {
                        uint32_t ch_id = ch == 0 ? B.c_0
                                       : ch == 1 ? B.c_1
                                       : ch == 2 ? B.c_2
                                       :           B.c_3;
                        uint32_t plane_off = spv_binop(&B, SpvOpIMul, B.t_u32, ch_id, ps_f16);
                        uint32_t final_addr = spv_binop(&B, SpvOpIAdd, B.t_u32, addr, plane_off);
                        uint32_t ptr = spv_access_chain_2(&B, B.t_ptr_ssbo_f16,
                                                          B.v_ssbo[p], B.c_0, final_addr);
                        uint32_t h = spv_load(&B, B.t_f16, ptr);
                        uint32_t f32_val = spv_unop(&B, SpvOpFConvert, B.t_f32, h);
                        switch (ch) {
                            case 0: B.val[i]   = f32_val; break;
                            case 1: B.val_1[i] = f32_val; break;
                            case 2: B.val_2[i] = f32_val; break;
                            case 3: B.val_3[i] = f32_val; break;
                        }
                    }
                    B.is_f[i] = 1;
                } break;

                case op_store_16x4: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t addr = compute_addr(&B, x_coord, y_coord, p);

                    uint32_t h0 = B.is_f[xid]             ? spv_f32_to_f16(&B, get_val(&B, inst->x))
                                                           : as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t h1 = B.is_f[yid]             ? spv_f32_to_f16(&B, get_val(&B, inst->y))
                                                           : as_u32(&B, get_val(&B, inst->y), yid);
                    uint32_t h2 = B.is_f[get_id(inst->z)] ? spv_f32_to_f16(&B, get_val(&B, inst->z))
                                                           : as_u32(&B, get_val(&B, inst->z),
                                                                    get_id(inst->z));
                    uint32_t h3 = B.is_f[get_id(inst->w)] ? spv_f32_to_f16(&B, get_val(&B, inst->w))
                                                           : as_u32(&B, get_val(&B, inst->w),
                                                                    get_id(inst->w));

                    uint32_t h1_shifted = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, h1, B.c_16);
                    uint32_t w0 = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, h0, h1_shifted);
                    uint32_t h3_shifted = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, h3, B.c_16);
                    uint32_t w1 = spv_binop(&B, SpvOpBitwiseOr, B.t_u32, h2, h3_shifted);

                    uint32_t px = spv_composite_construct_2(&B, B.t_uvec2, w0, w1);
                    store_ssbo_uvec2(&B, p, addr, px);
                } break;

                case op_store_16x4_planar: {
                    int p = resolve_ptr(&B, inst);
                    uint32_t lim_off = spv_const_u32(&B, (uint32_t)(3 + p));
                    uint32_t total  = load_meta_u32(&B, lim_off);
                    uint32_t c4     = spv_const_u32(&B, 4);
                    uint32_t ps_f16 = spv_binop(&B, SpvOpUDiv, B.t_u32, total, c4);

                    uint32_t stride_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t stride = load_meta_u32(&B, stride_off);
                    uint32_t addr = spv_binop(&B, SpvOpIMul, B.t_u32, y_coord, stride);
                    addr = spv_binop(&B, SpvOpIAdd, B.t_u32, addr, x_coord);

                    val channels[4] = { inst->x, inst->y, inst->z, inst->w };
                    int channel_ids[4] = { xid, yid, get_id(inst->z), get_id(inst->w) };

                    for (int ch = 0; ch < 4; ch++) {
                        uint32_t ch_id = ch == 0 ? B.c_0
                                       : ch == 1 ? B.c_1
                                       : ch == 2 ? B.c_2
                                       :           B.c_3;
                        uint32_t plane_off = spv_binop(&B, SpvOpIMul, B.t_u32, ch_id, ps_f16);
                        uint32_t final_addr = spv_binop(&B, SpvOpIAdd, B.t_u32, addr, plane_off);

                        uint32_t f32_val = as_f32(&B, get_val(&B, channels[ch]),
                                                       channel_ids[ch]);
                        uint32_t h = spv_unop(&B, SpvOpFConvert, B.t_f16, f32_val);
                        uint32_t ptr = spv_access_chain_2(&B, B.t_ptr_ssbo_f16,
                                                          B.v_ssbo[p], B.c_0, final_addr);
                        spv_store(&B, ptr, h);
                    }
                } break;

                case op_f32_from_f16: {
                    if (B.is_f[xid]) {
                        B.val[i] = get_val(&B, inst->x);
                    } else {
                        uint32_t h = as_u32(&B, get_val(&B, inst->x), xid);
                        B.val[i] = spv_f16_to_f32(&B, h);
                    }
                    B.is_f[i] = 1;
                } break;

                case op_f16_from_f32: {
                    B.val[i] = as_f32(&B, get_val(&B, inst->x), xid);
                    B.is_f[i] = 1;
                } break;

                case op_i32_from_s16: {
                    // Sign-extend 16-bit value to 32-bit.
                    uint32_t v = as_u32(&B, get_val(&B, inst->x), xid);
                    // Shift left 16, then arithmetic shift right 16.
                    uint32_t shifted = spv_binop(&B, SpvOpShiftLeftLogical, B.t_u32, v, B.c_16);
                    // Use signed shift right.
                    uint32_t signed_val = spv_bitcast(&B, B.t_i32, shifted);
                    uint32_t sext =
                        spv_binop(&B, SpvOpShiftRightArithmetic, B.t_i32, signed_val, B.c_16);
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

                case op_add_f32:
                    B.val[i] = spv_fadd(&B, flags,
                                        as_f32(&B, get_val(&B, inst->x), xid),
                                        as_f32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_sub_f32:
                    B.val[i] = spv_fsub(&B, flags,
                                        as_f32(&B, get_val(&B, inst->x), xid),
                                        as_f32(&B, get_val(&B, inst->y), yid));
                    break;
                case op_mul_f32:
                    B.val[i] = spv_fmul(&B, flags,
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
                case op_sqrt_f32: {
                    uint32_t const xf = as_f32(&B, get_val(&B, inst->x), xid);
                    if (flags & SPIRV_FLOAT_CONTROLS) {
                        B.val[i] = spv_glsl_1(&B, B.t_f32, GLSLstd450Sqrt, xf);
                    } else {
                        uint32_t const c_23   = spv_const_u32(&B, 23);
                        uint32_t const c_127  = spv_const_u32(&B, 127);
                        uint32_t const c_neg2 = spv_const_u32(&B, 0xfffffffeu);
                        uint32_t const c_mant = spv_const_u32(&B, 0x007fffff);
                        uint32_t const c_half = spv_const_f32(&B, 0.5f);

                        uint32_t const xi = spv_bitcast(&B, B.t_u32, xf);
                        uint32_t const raw_exp = spv_binop(&B, SpvOpShiftRightLogical,
                                                            B.t_u32, xi, c_23);
                        uint32_t const ex = spv_binop(&B, SpvOpISub, B.t_u32,
                                                       raw_exp, c_127);
                        uint32_t const ex_even = spv_binop(&B, SpvOpBitwiseAnd,
                                                            B.t_u32, ex, c_neg2);
                        uint32_t const half_ex_i = spv_binop(&B, SpvOpShiftRightArithmetic,
                                     B.t_i32,
                                     spv_bitcast(&B, B.t_i32, ex_even),
                                     B.c_1);
                        uint32_t const half_ex = spv_bitcast(&B, B.t_u32, half_ex_i);

                        uint32_t const ex_odd = spv_binop(&B, SpvOpBitwiseAnd,
                                                           B.t_u32, ex, B.c_1);
                        uint32_t const norm_exp = spv_binop(&B, SpvOpShiftLeftLogical,
                                     B.t_u32,
                                     spv_binop(&B, SpvOpIAdd, B.t_u32, c_127, ex_odd),
                                     c_23);
                        uint32_t const norm_bits = spv_binop(&B, SpvOpBitwiseOr, B.t_u32,
                                     spv_binop(&B, SpvOpBitwiseAnd, B.t_u32, xi, c_mant),
                                     norm_exp);
                        uint32_t const xn = spv_bitcast(&B, B.t_f32, norm_bits);

                        uint32_t const yn = spv_glsl_1(&B, B.t_f32,
                                                        GLSLstd450InverseSqrt, xn);
                        uint32_t sn = spv_binop(&B, SpvOpFMul, B.t_f32, xn, yn);

                        uint32_t const neg_sn = spv_unop(&B, SpvOpFNegate, B.t_f32, sn);
                        uint32_t const rn = spv_glsl_3(&B, B.t_f32, GLSLstd450Fma,
                                                        neg_sn, sn, xn);
                        uint32_t const hy = spv_binop(&B, SpvOpFMul, B.t_f32, yn, c_half);
                        sn = spv_glsl_3(&B, B.t_f32, GLSLstd450Fma, rn, hy, sn);

                        uint32_t const sn_bits = spv_bitcast(&B, B.t_u32, sn);
                        uint32_t const shift = spv_binop(&B, SpvOpShiftLeftLogical,
                                                          B.t_u32, half_ex, c_23);
                        uint32_t const res_bits = spv_binop(&B, SpvOpIAdd, B.t_u32,
                                                             sn_bits, shift);
                        uint32_t const res = spv_bitcast(&B, B.t_f32, res_bits);

                        uint32_t const eq0 = spv_binop(&B, SpvOpFOrdEqual,
                                                        B.t_bool, xf, B.c_0f);
                        B.val[i] = spv_select(&B, B.t_f32, eq0, B.c_0f, res);
                    }
                } break;
                case op_abs_f32:
                    B.val[i] = spv_glsl_1(&B, B.t_f32, GLSLstd450FAbs,
                                            as_f32(&B, get_val(&B, inst->x), xid));
                    break;
                case op_square_f32: {
                    uint32_t xf = as_f32(&B, get_val(&B, inst->x), xid);
                    B.val[i] = spv_fmul(&B, flags, xf, xf);
                } break;
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
                case op_square_add_f32: {
                    uint32_t xf = as_f32(&B, get_val(&B, inst->x), xid);
                    B.val[i] = spv_glsl_3(&B, B.t_f32, GLSLstd450Fma,
                                            xf, xf,
                                            as_f32(&B, get_val(&B, inst->y), yid));
                } break;
                case op_square_sub_f32: {
                    uint32_t xf = as_f32(&B, get_val(&B, inst->x), xid);
                    uint32_t neg_x = spv_unop(&B, SpvOpFNegate, B.t_f32, xf);
                    B.val[i] = spv_glsl_3(&B, B.t_f32, GLSLstd450Fma,
                                            neg_x, xf,
                                            as_f32(&B, get_val(&B, inst->y), yid));
                } break;

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
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    B.val[i] = spv_fadd(&B, flags,
                                        as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                } break;
                case op_sub_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    B.val[i] = spv_fsub(&B, flags,
                                        as_f32(&B, get_val(&B, inst->x), xid), imm_f);
                } break;
                case op_mul_f32_imm: {
                    float fval;
                    memcpy(&fval, &inst->imm, sizeof fval);
                    uint32_t imm_f = spv_const_f32(&B, fval);
                    B.val[i] = spv_fmul(&B, flags,
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

                case op_loop_begin: {
                    uint32_t label_header   = spv_id(&B);
                    uint32_t label_loop     = spv_id(&B);
                    uint32_t label_continue = spv_id(&B);
                    uint32_t label_merge    = spv_id(&B);

                    spv_op(&B.func, SpvOpBranch, 2);
                    spv_word(&B.func, label_header);

                    spv_op(&B.func, SpvOpLabel, 2);
                    spv_word(&B.func, label_header);

                    spv_op(&B.func, SpvOpLoopMerge, 4);
                    spv_word(&B.func, label_merge);
                    spv_word(&B.func, label_continue);
                    spv_word(&B.func, 0);

                    uint32_t cur_i = spv_load(&B, B.t_u32, v_vars[inst->imm]);
                    uint32_t trip = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t cond = spv_binop(&B, SpvOpULessThan, B.t_bool, cur_i, trip);
                    spv_op(&B.func, SpvOpBranchConditional, 4);
                    spv_word(&B.func, cond);
                    spv_word(&B.func, label_loop);
                    spv_word(&B.func, label_merge);

                    spv_op(&B.func, SpvOpLabel, 2);
                    spv_word(&B.func, label_loop);

                    B.val_1[i] = label_continue;
                    B.val_2[i] = label_merge;
                    B.val_3[i] = label_header;
                } break;
                case op_loop_end: {
                    int lb = ir->loop_begin;
                    uint32_t label_continue = B.val_1[lb];
                    uint32_t label_merge    = B.val_2[lb];
                    uint32_t label_header   = B.val_3[lb];

                    spv_op(&B.func, SpvOpBranch, 2);
                    spv_word(&B.func, label_continue);

                    spv_op(&B.func, SpvOpLabel, 2);
                    spv_word(&B.func, label_continue);

                    spv_op(&B.func, SpvOpBranch, 2);
                    spv_word(&B.func, label_header);

                    spv_op(&B.func, SpvOpLabel, 2);
                    spv_word(&B.func, label_merge);
                } break;
                case op_if_begin: {
                    uint32_t label_then  = spv_id(&B);
                    uint32_t label_merge = spv_id(&B);
                    uint32_t cond_u32  = as_u32(&B, get_val(&B, inst->x), xid);
                    uint32_t cond_bool = spv_binop(&B, SpvOpINotEqual, B.t_bool,
                                                   cond_u32, B.c_0);
                    spv_op(&B.func, SpvOpSelectionMerge, 3);
                    spv_word(&B.func, label_merge);
                    spv_word(&B.func, 0);
                    spv_op(&B.func, SpvOpBranchConditional, 4);
                    spv_word(&B.func, cond_bool);
                    spv_word(&B.func, label_then);
                    spv_word(&B.func, label_merge);
                    spv_op(&B.func, SpvOpLabel, 2);
                    spv_word(&B.func, label_then);
                    B.val_1[i] = label_merge;
                } break;
                case op_if_end: {
                    uint32_t label_merge = B.val_1[inst->x.id];
                    spv_op(&B.func, SpvOpBranch, 2);
                    spv_word(&B.func, label_merge);
                    spv_op(&B.func, SpvOpLabel, 2);
                    spv_word(&B.func, label_merge);
                } break;
                case op_load_var:
                    B.val[i] = spv_load(&B, B.t_u32, v_vars[inst->imm]);
                    break;
                case op_store_var:
                    spv_store(&B, v_vars[inst->imm],
                              as_u32(&B, get_val(&B, inst->y), yid));
                    break;
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

    int total_words = 5 // header
        + B.caps.words
        + B.exts.words
        + B.ext_import.words
        + B.mem_model.words
        + B.entry.words
        + B.exec_mode.words
        + B.decor.words
        + B.types.words
        + B.globals.words
        + B.func.words;

    uint32_t *spirv = malloc((size_t)total_words * sizeof(uint32_t));
    int off = 0;

    spirv[off++] = SpvMagic;
    spirv[off++] = SpvVersion;
    spirv[off++] = SpvGenerator;
    spirv[off++] = B.next_id; // bound
    spirv[off++] = SpvSchema;

    #define COPY_SECTION(s) do { \
        memcpy(spirv + off, (s).word, (size_t)(s).words * sizeof(uint32_t)); \
        off += (s).words; \
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

    result.spirv_words = total_words;

    free(B.caps.word);
    free(B.exts.word);
    free(B.ext_import.word);
    free(B.mem_model.word);
    free(B.entry.word);
    free(B.exec_mode.word);
    free(B.decor.word);
    free(B.types.word);
    free(B.globals.word);
    free(B.func.word);
    free(B.v_ssbo);
    free(B.val);
    free(B.val_1);
    free(B.val_2);
    free(B.val_3);
    free(B.is_f);
    free(B.buf_is_16);
    free(B.buf_is_16x4);
    free(B.buf_is_uniform);
    free(B.buf_uniform_slots);
    free(B.t_ptr_uniform_struct);
    free(B.const_cache);
    free(v_vars);

    umbra_flat_ir_free(resolved);

    result.spirv = spirv;
    return result;
}
