#include "spirv.h"
#include <stdlib.h>
#include <string.h>

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

};

// ---------------------------------------------------------------------------
//  SPIR-V binary buffer.
// ---------------------------------------------------------------------------

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
    uint32_t t_fvec2;
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
    uint32_t ext_glsl;

    // Global variable IDs.
    uint32_t *v_ssbo;      // one per buffer (total_bufs)
    uint32_t v_global_id;  // gl_GlobalInvocationID
    uint32_t v_push;       // push constant block variable

    // Metadata push constant layout (no user uniforms — those go through the
    // uniform ring as a storage buffer at binding 0):
    //   [0] = w, [1] = x0, [2] = y0,
    //   [3..3+total_bufs-1] = buf_szs, [3+total_bufs..3+2*total_bufs-1] = buf_rbs
    int total_bufs;
    int push_words;

    // Constant cache for small integer constants.
    uint32_t c_0, c_1, c_2, c_3, c_8, c_16;
    uint32_t c_0f;  // float 0.0
    uint32_t c_allones;  // 0xFFFFFFFF

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
    int    has_16;
    _Bool  emit_16; int :24;

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

// Get the SPIR-V result ID for a bb_inst operand, handling channel extraction.
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

// Resolve a pointer index: if negative, it's a deref reference.
static int resolve_ptr(SpvBuilder *b, struct bb_inst const *inst) {
    return inst->ptr.deref ? b->deref_buf[inst->ptr.ix] : inst->ptr.bits;
}

// Load a metadata word from the push constant block at a given word offset.
// The push block carries only backend-side metadata (w, x0, y0, buf_szs,
// buf_rbs); user uniforms ride in storage buffer binding 0 via the ring.
// Push layout is struct { uint data[N]; }, so we need two indices:
// member 0, then array element.
static uint32_t load_meta_u32(SpvBuilder *b, uint32_t offset_id) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_push_u32, b->v_push, b->c_0, offset_id);
    return spv_load(b, b->t_u32, ptr);
}

// Load from SSBO[buf_idx] at element_index.
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

// Load u16 from SSBO[buf_idx] at element_index, zero-extended to u32.
// When emit_16, the buffer is RuntimeArray<u16> and we use OpUConvert.
// Otherwise, it's RuntimeArray<f16> and we bitcast f16→f32→u32.
static uint32_t load_ssbo_u16(SpvBuilder *b, int buf_idx, uint32_t elem_idx) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u16,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, elem_idx);
    if (b->emit_16) {
        uint32_t const raw = spv_load(b, b->t_u16, ptr);
        return spv_unop(b, SpvOpUConvert, b->t_u32, raw);
    }
    // Load f16, convert to f32, pack back to u32 with raw bits via PackHalf2x16.
    uint32_t const h = spv_load(b, b->t_f16, ptr);
    uint32_t const f = spv_unop(b, SpvOpFConvert, b->t_f32, h);
    uint32_t const v2 = spv_composite_construct_2(b, b->t_fvec2, f, b->c_0f);
    return spv_glsl_1(b, b->t_u32, 58 /*PackHalf2x16*/, v2);
}

// Store u16 to SSBO[buf_idx] at element_index, truncating from u32.
static void store_ssbo_u16(SpvBuilder *b, int buf_idx, uint32_t elem_idx, uint32_t value) {
    uint32_t const ptr = spv_access_chain_2(b, b->t_ptr_ssbo_u16,
                                            b->v_ssbo[buf_idx],
                                            b->c_0, elem_idx);
    if (b->emit_16) {
        uint32_t const val16 = spv_unop(b, SpvOpUConvert, b->t_u16, value);
        spv_store(b, ptr, val16);
        return;
    }
    // Reinterpret the low 16 bits of value as f16 via UnpackHalf2x16→FConvert.
    uint32_t const v2 = spv_glsl_1(b, b->t_fvec2, 62 /*UnpackHalf2x16*/, value);
    uint32_t const f  = spv_composite_extract(b, b->t_f32, v2, 0);
    uint32_t const h  = spv_unop(b, SpvOpFConvert, b->t_f16, f);
    spv_store(b, ptr, h);
}

// Compute linear address for row-structured buffer:
//   addr = y * (row_bytes/stride) + x
// Where stride = 4 for u32, 2 for u16.
static uint32_t compute_addr(SpvBuilder *b, uint32_t x, uint32_t y,
                              int buf_idx, uint32_t stride_shift) {
    // buf_rbs[buf_idx] is at push offset 3 + total_bufs + buf_idx
    uint32_t const rb_off = spv_const_u32(b, (uint32_t)(3 + b->total_bufs + buf_idx));
    uint32_t const rb     = load_meta_u32(b, rb_off);
    // row_bytes >> stride_shift = number of elements per row
    uint32_t const elems_per_row = spv_binop(b, SpvOpShiftRightLogical, b->t_u32,
                                             rb, stride_shift);
    uint32_t const row_off = spv_binop(b, SpvOpIMul, b->t_u32, y, elems_per_row);
    return spv_binop(b, SpvOpIAdd, b->t_u32, row_off, x);
}

// Compute safe gather index: clamp to [0, count-1], return 0 for OOB.
// buf_szs[buf_idx] is at push offset 3 + buf_idx.
// elem_bytes = 4 for u32, 2 for u16.
// Returns (clamped_index, oob_mask) where oob_mask is 0xFFFFFFFF for in-bounds, 0 for OOB.
static void gather_safe(SpvBuilder *b, uint32_t ix_val, int buf_idx,
                         uint32_t elem_shift,
                         uint32_t *out_idx, uint32_t *out_mask) {
    uint32_t const sz_off = spv_const_u32(b, (uint32_t)(3 + buf_idx));
    uint32_t const sz     = load_meta_u32(b, sz_off);
    uint32_t const count  = spv_binop(b, SpvOpShiftRightLogical, b->t_u32, sz, elem_shift);

    // max_idx = max(count-1, 0)
    uint32_t const count_minus_1 = spv_binop(b, SpvOpISub, b->t_u32, count, b->c_1);
    // clamp: UMin(UMax(ix, 0), max_idx) — but ix is uint so UMax(ix,0)=ix
    // Actually we need to handle negative indices (signed). Use SMax then UMin.
    // Use GLSL.std.450 UMin and SMax:
    // clamped = UMin(ix, count_minus_1)  — this clamps the upper bound
    uint32_t const clamped = spv_glsl_2(b, b->t_u32, 38 /*UMin*/, ix_val, count_minus_1);

    // OOB check: ix >= 0 && ix < count  (signed comparison for < 0)
    // in_bounds = (ix >=s 0) && (ix <u count)
    uint32_t const ge_zero   = spv_binop(b, SpvOpSGreaterThanEqual, b->t_bool, ix_val,
                                         b->c_0);
    uint32_t const lt_count  = spv_binop(b, SpvOpULessThan, b->t_bool, ix_val, count);
    uint32_t const in_bounds = spv_binop(b, SpvOpLogicalAnd, b->t_bool, ge_zero, lt_count);
    // mask = in_bounds ? 0xFFFFFFFF : 0
    *out_mask = spv_select(b, b->t_u32, in_bounds, b->c_allones, b->c_0);
    *out_idx = clamped;
}

// fp16 <-> fp32 conversion.
// When emit_16 is true, uses native OpFConvert (Float16 capability).
// When emit_16 is false, uses GLSL.std.450 UnpackHalf2x16/PackHalf2x16.
static uint32_t spv_f16_to_f32(SpvBuilder *b, uint32_t u32_val) {
    if (b->emit_16) {
        uint32_t const u16_val = spv_unop(b, SpvOpUConvert, b->t_u16, u32_val);
        uint32_t const h       = spv_bitcast(b, b->t_f16, u16_val);
        return spv_unop(b, SpvOpFConvert, b->t_f32, h);
    }
    // UnpackHalf2x16(u32_val) → vec2<f32>, take component 0.
    uint32_t const v2 = spv_glsl_1(b, b->t_fvec2, 62 /*UnpackHalf2x16*/, u32_val);
    return spv_composite_extract(b, b->t_f32, v2, 0);
}

static uint32_t spv_f32_to_f16(SpvBuilder *b, uint32_t f32_val) {
    if (b->emit_16) {
        uint32_t const h   = spv_unop(b, SpvOpFConvert, b->t_f16, f32_val);
        uint32_t const u16 = spv_bitcast(b, b->t_u16, h);
        return spv_unop(b, SpvOpUConvert, b->t_u32, u16);
    }
    // PackHalf2x16(vec2(f32_val, 0.0)) → u32 with fp16 in low 16 bits.
    uint32_t const v2 = spv_composite_construct_2(b, b->t_fvec2, f32_val, b->c_0f);
    return spv_glsl_1(b, b->t_u32, 58 /*PackHalf2x16*/, v2);
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
uint32_t *build_spirv(struct umbra_basic_block const *bb,
                              int flags,
                              int *out_spirv_words,
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
        if (op_has_ptr(bb->inst[i].op) && bb->inst[i].ptr.bits >= 0) {
            if (bb->inst[i].ptr.bits > max_ptr) {
                max_ptr = bb->inst[i].ptr.bits;
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
    int const total_bufs = next_buf;
    int const n_deref    = total_bufs - max_ptr - 1;
    *out_total_bufs = total_bufs;
    *out_n_deref = n_deref;
    B.total_bufs = total_bufs;

    struct deref_info *di = calloc((size_t)(n_deref ? n_deref : 1), sizeof *di);
    {
        int d = 0;
        for (int i = 0; i < bb->insts; i++) {
            if (bb->inst[i].op == op_deref_ptr) {
                di[d].buf_idx  = deref_buf[i];
                di[d].src_buf  = bb->inst[i].ptr.bits;
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
            int p = bb->inst[i].ptr.deref ? deref_buf[bb->inst[i].ptr.ix]
                                         : bb->inst[i].ptr.bits;
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

    _Bool const emit_16 = !(flags & SPIRV_NO_16BIT_TYPES)
                        && (B.has_16 || (flags & SPIRV_ALWAYS_16BIT));
    B.emit_16 = emit_16;
    if (B.has_16 || (flags & SPIRV_ALWAYS_16BIT)) {
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

    if (emit_16) {
        B.t_u16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeInt, 4);
        spv_word(&B.types, B.t_u16);
        spv_word(&B.types, 16);
        spv_word(&B.types, 0); // unsigned
    }

    if (B.has_16 || (flags & SPIRV_ALWAYS_16BIT)) {
        B.t_f16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeFloat, 3);
        spv_word(&B.types, B.t_f16);
        spv_word(&B.types, 16);
    }

    B.t_uvec3 = spv_id(&B);
    spv_op(&B.types, SpvOpTypeVector, 4);
    spv_word(&B.types, B.t_uvec3);
    spv_word(&B.types, B.t_u32);
    spv_word(&B.types, 3);

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

    // 16-bit storage types.  When emit_16 is true, use native u16 element
    // type.  When false (wgpu/naga path), use f16 as the element type
    // since naga supports f16 via ShaderF16 but not u16.
    if (B.has_16) {
        uint32_t t_elem_16 = emit_16 ? B.t_u16 : B.t_f16;

        B.t_rta_u16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypeRuntimeArray, 3);
        spv_word(&B.types, B.t_rta_u16);
        spv_word(&B.types, t_elem_16);

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
        spv_word(&B.decor, SpvDecorationBlock);

        spv_op(&B.decor, SpvOpMemberDecorate, 5);
        spv_word(&B.decor, B.t_struct_rta_u16);
        spv_word(&B.decor, 0);
        spv_word(&B.decor, SpvDecorationOffset);
        spv_word(&B.decor, 0);

        B.t_ptr_ssbo_struct_u16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_ssbo_struct_u16);
        spv_word(&B.types, SpvStorageClassStorageBuffer);
        spv_word(&B.types, B.t_struct_rta_u16);

        B.t_ptr_ssbo_u16 = spv_id(&B);
        spv_op(&B.types, SpvOpTypePointer, 4);
        spv_word(&B.types, B.t_ptr_ssbo_u16);
        spv_word(&B.types, SpvStorageClassStorageBuffer);
        spv_word(&B.types, t_elem_16);
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
        spv_word(&B.globals, SpvStorageClassStorageBuffer);

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
        spv_word(&B.exec_mode, SPIRV_WG_SIZE);
        spv_word(&B.exec_mode, 1);
        spv_word(&B.exec_mode, 1);

        if (flags & SPIRV_FLOAT_CONTROLS) {
            spv_op(&B.exec_mode, SpvOpExecutionMode, 4);
            spv_word(&B.exec_mode, fn_main);
            spv_word(&B.exec_mode, SpvExecutionModeSignedZeroInfNanPreserve);
            spv_word(&B.exec_mode, 32); // float32
        }

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
                    uint32_t rb = load_meta_u32(&B, rb_off);
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
                    uint32_t sz = load_meta_u32(&B, sz_off);
                    uint32_t ps_u16 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, sz, B.c_3);

                    uint32_t rb_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t rb = load_meta_u32(&B, rb_off);

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
                    uint32_t rb = load_meta_u32(&B, rb_off);
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
                    uint32_t sz = load_meta_u32(&B, sz_off);
                    uint32_t ps_u16 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, sz, B.c_3);

                    uint32_t rb_off = spv_const_u32(&B, (uint32_t)(3 + B.total_bufs + p));
                    uint32_t rb = load_meta_u32(&B, rb_off);
                    uint32_t rb_u16 = spv_binop(&B, SpvOpShiftRightLogical, B.t_u32, rb, B.c_1);
                    uint32_t addr16 = spv_binop(&B, SpvOpIMul, B.t_u32, y_coord, rb_u16);
                    addr16 = spv_binop(&B, SpvOpIAdd, B.t_u32, addr16, x_coord);

                    val channels[4] = { inst->x, inst->y, inst->z, inst->w };
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

    // Header.
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

    *out_spirv_words = total_words;

    // Free temporary buffers.
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
    free(B.const_cache);
    free(deref_buf);

    return spirv;
}
