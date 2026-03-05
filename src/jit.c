#include "../umbra.h"
#include "bb.h"

#if !defined(__aarch64__)

struct umbra_jit { int dummy; };
struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) { (void)bb; return 0; }
void umbra_jit_run (struct umbra_jit *j, int n, void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    (void)j; (void)n; (void)p0; (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
}
void umbra_jit_free(struct umbra_jit *j) { (void)j; }
void umbra_jit_dump(struct umbra_jit const *j, FILE *f) { (void)j; (void)f; }

#else

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <libkern/OSCacheControl.h>

typedef struct { uint32_t *buf; int len,cap; } Buf;

static void put(Buf *b, uint32_t w) {
    if (b->len == b->cap) {
        b->cap = b->cap ? 2*b->cap : 1024;
        b->buf = realloc(b->buf, (size_t)b->cap * 4);
    }
    b->buf[b->len++] = w;
}

static uint32_t RET(void) { return 0xD65F03C0u; }

static uint32_t ADD_xi(int d, int n, int imm12) {
    return 0x91000000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t SUB_xi(int d, int n, int imm12) {
    return 0xD1000000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t SUBS_xi(int d, int n, int imm12) {
    return 0xF1000000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t MOVZ_x(int d, uint16_t imm) {
    return 0xD2800000u | ((uint32_t)imm<<5) | (uint32_t)d;
}
static uint32_t MOVZ_w(int d, uint16_t imm) {
    return 0x52800000u | ((uint32_t)imm<<5) | (uint32_t)d;
}
static uint32_t MOVK_w16(int d, uint16_t imm) {
    return 0x72A00000u | ((uint32_t)imm<<5) | (uint32_t)d;
}
static uint32_t STP_pre(int t1, int t2, int n, int imm7) {
    return 0xA9800000u | ((uint32_t)(imm7&0x7f)<<15) | ((uint32_t)t2<<10) | ((uint32_t)n<<5) | (uint32_t)t1;
}
static uint32_t LDP_post(int t1, int t2, int n, int imm7) {
    return 0xA8C00000u | ((uint32_t)(imm7&0x7f)<<15) | ((uint32_t)t2<<10) | ((uint32_t)n<<5) | (uint32_t)t1;
}
static uint32_t B(int off26) {
    return 0x14000000u | (uint32_t)(off26 & 0x3ffffff);
}
static uint32_t Bcond(int cond, int off19) {
    return 0x54000000u | ((uint32_t)(off19&0x7ffff)<<5) | (uint32_t)cond;
}

static uint32_t LDR_sx(int d, int n, int m) {  // LDR Sd,[Xn,Xm,LSL#2]
    return 0xBC607800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t STR_sx(int d, int n, int m) {
    return 0xBC207800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t LDR_hx(int d, int n, int m) {  // LDR Hd,[Xn,Xm,LSL#1]
    return 0x7C607800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t STR_hx(int d, int n, int m) {
    return 0x7C207800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}

static uint32_t LDR_q(int d, int n, int m) {  // LDR Qd,[Xn,Xm]
    return 0x3CE06800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t STR_q(int d, int n, int m) {
    return 0x3CA06800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t LDR_d(int d, int n, int m) {  // LDR Dd,[Xn,Xm]
    return 0xFC606800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t STR_d(int d, int n, int m) {
    return 0xFC206800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}

// LSL Xd, Xn, #imm  (UBFM alias)
static uint32_t LSL_xi(int d, int n, int shift) {
    int immr = (-shift) & 63, imms = 63 - shift;
    return 0xD3400000u | ((uint32_t)immr<<16) | ((uint32_t)imms<<10)
         | ((uint32_t)n<<5) | (uint32_t)d;
}

static uint32_t LDR_qi(int d, int n, int imm) { // LDR Qd,[Xn,#imm*16]
    return 0x3DC00000u | ((uint32_t)imm<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t STR_qi(int d, int n, int imm) {
    return 0x3D800000u | ((uint32_t)imm<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}

#define V3(enc) static uint32_t enc(int d,int n,int m){return enc##_ | ((uint32_t)m<<16)|((uint32_t)n<<5)|(uint32_t)d;}
#define V2(enc) static uint32_t enc(int d,int n){return enc##_ | ((uint32_t)n<<5)|(uint32_t)d;}

enum {
    // float 4S
    FADD_4s_  =0x4E20D400u, FSUB_4s_  =0x4EA0D400u, FMUL_4s_  =0x6E20DC00u,
    FDIV_4s_  =0x6E20FC00u, FMLA_4s_  =0x4E20CC00u,
    FMINNM_4s_=0x4EA0C400u, FMAXNM_4s_=0x4E20C400u,
    FSQRT_4s_ =0x6EA1F800u,
    SCVTF_4s_ =0x4E21D800u, FCVTZS_4s_=0x4EA1B800u,
    FCMEQ_4s_ =0x4E20E400u, FCMGT_4s_ =0x6EA0E400u, FCMGE_4s_ =0x6E20E400u,

    // int 4S
    ADD_4s_ =0x4EA08400u, SUB_4s_ =0x6EA08400u, MUL_4s_ =0x4EA09C00u,
    USHL_4s_=0x6EA04400u, SSHL_4s_=0x4EA04400u, NEG_4s_ =0x6EA0B800u,
    CMEQ_4s_=0x6EA08C00u, CMGT_4s_=0x4EA03400u, CMGE_4s_=0x4EA03C00u,
    CMHI_4s_=0x6EA03400u, CMHS_4s_=0x6EA03C00u,

    // bitwise 16B
    AND_16b_=0x4E201C00u, ORR_16b_=0x4EA01C00u, EOR_16b_=0x6E201C00u,
    BSL_16b_=0x6E601C00u, MVN_16b_=0x6E205800u,

    // float 4H (FEAT_FP16)
    FADD_4h_  =0x0E401400u, FSUB_4h_  =0x0EC01400u, FMUL_4h_  =0x2E401C00u,
    FDIV_4h_  =0x2E403C00u, FMLA_4h_  =0x0E400C00u,
    FMINNM_4h_=0x0EC00400u, FMAXNM_4h_=0x0E400400u,
    FSQRT_4h_ =0x2EF9F800u,
    FCMEQ_4h_ =0x0E402400u, FCMGT_4h_ =0x2EC02400u, FCMGE_4h_ =0x2E402400u,

    // int 4H
    ADD_4h_ =0x0E608400u, SUB_4h_ =0x2E608400u, MUL_4h_ =0x0E609C00u,
    USHL_4h_=0x2E604400u, SSHL_4h_=0x0E604400u, NEG_4h_ =0x2E60B800u,
    CMEQ_4h_=0x2E608C00u, CMGT_4h_=0x0E603400u, CMGE_4h_=0x0E603C00u,
    CMHI_4h_=0x2E603400u, CMHS_4h_=0x2E603C00u,

    // bitwise 8B
    AND_8b_=0x0E201C00u, ORR_8b_=0x0EA01C00u, EOR_8b_=0x2E201C00u,
    BSL_8b_=0x2E601C00u, MVN_8b_=0x2E205800u,

    // conversions
    FCVTN_4h_=0x0E216800u, FCVTL_4s_=0x0E217800u,
    SCVTF_4h_=0x0E79D800u, FCVTZS_4h_=0x0EF9B800u,
    XTN_4h_  =0x0E612800u, SXTL_4s_  =0x0F10A400u,
};

V3(FADD_4s)  V3(FSUB_4s)  V3(FMUL_4s) V3(FDIV_4s)  V3(FMLA_4s)
V3(FMINNM_4s) V3(FMAXNM_4s) V2(FSQRT_4s) V2(SCVTF_4s) V2(FCVTZS_4s)
V3(FCMEQ_4s) V3(FCMGT_4s) V3(FCMGE_4s)
V3(ADD_4s) V3(SUB_4s) V3(MUL_4s)
V3(USHL_4s) V3(SSHL_4s) V2(NEG_4s)
V3(CMEQ_4s) V3(CMGT_4s) V3(CMGE_4s) V3(CMHI_4s) V3(CMHS_4s)
V3(AND_16b) V3(ORR_16b) V3(EOR_16b) V3(BSL_16b) V2(MVN_16b)
V3(FADD_4h)  V3(FSUB_4h)  V3(FMUL_4h) V3(FDIV_4h)  V3(FMLA_4h)
V3(FMINNM_4h) V3(FMAXNM_4h) V2(FSQRT_4h)
V3(FCMEQ_4h) V3(FCMGT_4h) V3(FCMGE_4h)
V3(ADD_4h) V3(SUB_4h) V3(MUL_4h)
V3(USHL_4h) V3(SSHL_4h) V2(NEG_4h)
V3(CMEQ_4h) V3(CMGT_4h) V3(CMGE_4h) V3(CMHI_4h) V3(CMHS_4h)
V3(AND_8b) V3(ORR_8b) V3(EOR_8b) V3(BSL_8b) V2(MVN_8b)
V2(FCVTN_4h) V2(FCVTL_4s)
V2(SCVTF_4h) V2(FCVTZS_4h) V2(XTN_4h) V2(SXTL_4s)

#undef V3
#undef V2

static uint32_t MOVI_4s_0(int d) { return 0x4F000400u|(uint32_t)d; }
static uint32_t DUP_4s_w(int d, int n)  { return 0x4E040C00u|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t DUP_4h_w(int d, int n)  { return 0x0E020C00u|((uint32_t)n<<5)|(uint32_t)d; }

static uint32_t INS_s(int d, int idx, int n) {
    uint32_t imm5 = (uint32_t)(idx<<3)|4;
    return 0x4E001C00u|(imm5<<16)|((uint32_t)n<<5)|(uint32_t)d;
}

// x0=n, x1..x6=p0..p5, x9=loop i, x10=scratch, x11/x12=byte offsets, x15=stack base
enum { XI=9, XT=10, XH=11, XW=12, XS=15 };

static void load_imm_w(Buf *c, int rd, uint32_t v) {
    put(c, MOVZ_w(rd, (uint16_t)(v&0xffff)));
    if (v>>16) put(c, MOVK_w16(rd, (uint16_t)(v>>16)));
}

static void vld(Buf *c, int vd, int s) { put(c, LDR_qi(vd, XS, s)); }
static void vst(Buf *c, int vd, int s) { put(c, STR_qi(vd, XS, s)); }

// Register-to-register ALU emission (d,x,y,z are NEON register numbers).
// v0 is scratch for destructive ops (BSL, FMLA, shift-right).
static _Bool emit_alu_reg(Buf *c, enum op op, int d, int x, int y, int z, int imm) {
    switch (op) {
    case op_imm_32: {
        uint32_t v=(uint32_t)imm;
        if (v==0) { put(c, MOVI_4s_0(d)); }
        else { load_imm_w(c,XT,v); put(c, DUP_4s_w(d,XT)); }
    } return 1;
    case op_imm_16: case op_imm_half:
        put(c, MOVZ_w(XT,(uint16_t)imm));
        put(c, DUP_4h_w(d,XT));
        return 1;

    case op_add_f32: put(c, FADD_4s(d,x,y)); return 1;
    case op_sub_f32: put(c, FSUB_4s(d,x,y)); return 1;
    case op_mul_f32: put(c, FMUL_4s(d,x,y)); return 1;
    case op_div_f32: put(c, FDIV_4s(d,x,y)); return 1;
    case op_min_f32: put(c, FMINNM_4s(d,x,y)); return 1;
    case op_max_f32: put(c, FMAXNM_4s(d,x,y)); return 1;
    case op_sqrt_f32: put(c, FSQRT_4s(d,x)); return 1;
    case op_fma_f32:
        if (d==z) { put(c, FMLA_4s(d,x,y)); }
        else if (d!=x && d!=y) { put(c, ORR_16b(d,z,z)); put(c, FMLA_4s(d,x,y)); }
        else { put(c, ORR_16b(0,z,z)); put(c, FMLA_4s(0,x,y)); put(c, ORR_16b(d,0,0)); }
        return 1;

    case op_add_i32: put(c, ADD_4s(d,x,y)); return 1;
    case op_sub_i32: put(c, SUB_4s(d,x,y)); return 1;
    case op_mul_i32: put(c, MUL_4s(d,x,y)); return 1;
    case op_shl_i32: put(c, USHL_4s(d,x,y)); return 1;
    case op_shr_u32: put(c, NEG_4s(0,y)); put(c, USHL_4s(d,x,0)); return 1;
    case op_shr_s32: put(c, NEG_4s(0,y)); put(c, SSHL_4s(d,x,0)); return 1;

    case op_and_32: put(c, AND_16b(d,x,y)); return 1;
    case op_or_32:  put(c, ORR_16b(d,x,y)); return 1;
    case op_xor_32: put(c, EOR_16b(d,x,y)); return 1;
    case op_sel_32:
        if (d==x) { put(c, BSL_16b(d,y,z)); }
        else if (d!=y && d!=z) { put(c, ORR_16b(d,x,x)); put(c, BSL_16b(d,y,z)); }
        else { put(c, ORR_16b(0,x,x)); put(c, BSL_16b(0,y,z)); put(c, ORR_16b(d,0,0)); }
        return 1;

    case op_f32_from_i32: put(c, SCVTF_4s(d,x)); return 1;
    case op_i32_from_f32: put(c, FCVTZS_4s(d,x)); return 1;
    case op_half_from_f32: put(c, FCVTN_4h(d,x)); return 1;
    case op_half_from_i32: put(c, SCVTF_4s(0,x)); put(c, FCVTN_4h(d,0)); return 1;
    case op_f32_from_half: put(c, FCVTL_4s(d,x)); return 1;
    case op_i32_from_half: put(c, FCVTL_4s(0,x)); put(c, FCVTZS_4s(d,0)); return 1;
    case op_half_from_i16: put(c, SCVTF_4h(d,x)); return 1;
    case op_i16_from_half: put(c, FCVTZS_4h(d,x)); return 1;
    case op_i16_from_i32: put(c, XTN_4h(d,x)); return 1;
    case op_i32_from_i16: put(c, SXTL_4s(d,x)); return 1;

    case op_eq_f32: put(c, FCMEQ_4s(d,x,y)); return 1;
    case op_ne_f32: put(c, FCMEQ_4s(d,x,y)); put(c, MVN_16b(d,d)); return 1;
    case op_gt_f32: put(c, FCMGT_4s(d,x,y)); return 1;
    case op_ge_f32: put(c, FCMGE_4s(d,x,y)); return 1;
    case op_lt_f32: put(c, FCMGT_4s(d,y,x)); return 1;
    case op_le_f32: put(c, FCMGE_4s(d,y,x)); return 1;

    case op_eq_i32: put(c, CMEQ_4s(d,x,y)); return 1;
    case op_ne_i32: put(c, CMEQ_4s(d,x,y)); put(c, MVN_16b(d,d)); return 1;
    case op_gt_s32: put(c, CMGT_4s(d,x,y)); return 1;
    case op_ge_s32: put(c, CMGE_4s(d,x,y)); return 1;
    case op_lt_s32: put(c, CMGT_4s(d,y,x)); return 1;
    case op_le_s32: put(c, CMGE_4s(d,y,x)); return 1;
    case op_gt_u32: put(c, CMHI_4s(d,x,y)); return 1;
    case op_ge_u32: put(c, CMHS_4s(d,x,y)); return 1;
    case op_lt_u32: put(c, CMHI_4s(d,y,x)); return 1;
    case op_le_u32: put(c, CMHS_4s(d,y,x)); return 1;

    case op_add_i16: put(c, ADD_4h(d,x,y)); return 1;
    case op_sub_i16: put(c, SUB_4h(d,x,y)); return 1;
    case op_mul_i16: put(c, MUL_4h(d,x,y)); return 1;
    case op_shl_i16: put(c, USHL_4h(d,x,y)); return 1;
    case op_shr_u16: put(c, NEG_4h(0,y)); put(c, USHL_4h(d,x,0)); return 1;
    case op_shr_s16: put(c, NEG_4h(0,y)); put(c, SSHL_4h(d,x,0)); return 1;
    case op_and_16: put(c, AND_8b(d,x,y)); return 1;
    case op_or_16:  put(c, ORR_8b(d,x,y)); return 1;
    case op_xor_16: put(c, EOR_8b(d,x,y)); return 1;
    case op_sel_16:
        if (d==x) { put(c, BSL_8b(d,y,z)); }
        else if (d!=y && d!=z) { put(c, ORR_8b(d,x,x)); put(c, BSL_8b(d,y,z)); }
        else { put(c, ORR_8b(0,x,x)); put(c, BSL_8b(0,y,z)); put(c, ORR_8b(d,0,0)); }
        return 1;
    case op_eq_i16: put(c, CMEQ_4h(d,x,y)); return 1;
    case op_ne_i16: put(c, CMEQ_4h(d,x,y)); put(c, MVN_8b(d,d)); return 1;
    case op_gt_s16: put(c, CMGT_4h(d,x,y)); return 1;
    case op_ge_s16: put(c, CMGE_4h(d,x,y)); return 1;
    case op_lt_s16: put(c, CMGT_4h(d,y,x)); return 1;
    case op_le_s16: put(c, CMGE_4h(d,y,x)); return 1;
    case op_gt_u16: put(c, CMHI_4h(d,x,y)); return 1;
    case op_ge_u16: put(c, CMHS_4h(d,x,y)); return 1;
    case op_lt_u16: put(c, CMHI_4h(d,y,x)); return 1;
    case op_le_u16: put(c, CMHS_4h(d,y,x)); return 1;

    case op_add_half: put(c, FADD_4h(d,x,y)); return 1;
    case op_sub_half: put(c, FSUB_4h(d,x,y)); return 1;
    case op_mul_half: put(c, FMUL_4h(d,x,y)); return 1;
    case op_div_half: put(c, FDIV_4h(d,x,y)); return 1;
    case op_min_half: put(c, FMINNM_4h(d,x,y)); return 1;
    case op_max_half: put(c, FMAXNM_4h(d,x,y)); return 1;
    case op_sqrt_half: put(c, FSQRT_4h(d,x)); return 1;
    case op_fma_half:
        if (d==z) { put(c, FMLA_4h(d,x,y)); }
        else if (d!=x && d!=y) { put(c, ORR_8b(d,z,z)); put(c, FMLA_4h(d,x,y)); }
        else { put(c, ORR_8b(0,z,z)); put(c, FMLA_4h(0,x,y)); put(c, ORR_8b(d,0,0)); }
        return 1;
    case op_and_half: put(c, AND_8b(d,x,y)); return 1;
    case op_or_half:  put(c, ORR_8b(d,x,y)); return 1;
    case op_xor_half: put(c, EOR_8b(d,x,y)); return 1;
    case op_sel_half:
        if (d==x) { put(c, BSL_8b(d,y,z)); }
        else if (d!=y && d!=z) { put(c, ORR_8b(d,x,x)); put(c, BSL_8b(d,y,z)); }
        else { put(c, ORR_8b(0,x,x)); put(c, BSL_8b(0,y,z)); put(c, ORR_8b(d,0,0)); }
        return 1;
    case op_eq_half: put(c, FCMEQ_4h(d,x,y)); return 1;
    case op_ne_half: put(c, FCMEQ_4h(d,x,y)); put(c, MVN_8b(d,d)); return 1;
    case op_gt_half: put(c, FCMGT_4h(d,x,y)); return 1;
    case op_ge_half: put(c, FCMGE_4h(d,x,y)); return 1;
    case op_lt_half: put(c, FCMGT_4h(d,y,x)); return 1;
    case op_le_half: put(c, FCMGE_4h(d,y,x)); return 1;

    default: return 0;
    }
}

// Register allocator: v0=scratch, v5=iota, v1-v4,v6-v7,v16-v31 = 22 allocatable
static const int8_t ra_pool[] = {1,2,3,4,6,7,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
#define RA_NREGS 22

struct ra {
    int    *last_use;     // [insts] last varying op that reads each value
    int8_t *reg;          // [insts] NEON register for value i, or -1
    int     nfree;
    int     owner[32];    // owner[v] = inst whose value is in Vv, or -1
    int8_t  free_stack[RA_NREGS];
    int8_t  pad_[6];
};

static struct ra* ra_create(struct umbra_basic_block const *bb) {
    int n = bb->insts;
    struct ra *ra = malloc(sizeof *ra);
    ra->reg      = malloc((size_t)n * sizeof *ra->reg);
    ra->last_use = malloc((size_t)n * sizeof *ra->last_use);

    for (int i = 0; i < n; i++) ra->reg[i] = -1;
    for (int i = 0; i < 32; i++) ra->owner[i] = -1;

    for (int i = 0; i < n; i++) ra->last_use[i] = -1;
    for (int i = bb->preamble; i < n; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        ra->last_use[inst->x] = i;
        ra->last_use[inst->y] = i;
        ra->last_use[inst->z] = i;
    }

    ra->nfree = RA_NREGS;
    for (int i = 0; i < RA_NREGS; i++) ra->free_stack[i] = ra_pool[RA_NREGS - 1 - i];

    return ra;
}

static void ra_destroy(struct ra *ra) {
    free(ra->reg);
    free(ra->last_use);
    free(ra);
}

static void ra_free_reg(struct ra *ra, int val) {
    int8_t r = ra->reg[val];
    if (r < 0) return;
    ra->free_stack[ra->nfree++] = r;
    ra->owner[(int)r] = -1;
    ra->reg[val] = -1;
}

static int8_t ra_alloc(Buf *c, struct ra *ra, int *sl, int *ns) {
    if (ra->nfree > 0) return ra->free_stack[--ra->nfree];

    // Evict: find register whose owner has farthest last_use (Belady-like)
    int best_r = -1, best_lu = -1;
    for (int r = 0; r < 32; r++) {
        if (ra->owner[r] < 0) continue;
        int val = ra->owner[r];
        if (ra->last_use[val] > best_lu) { best_lu = ra->last_use[val]; best_r = r; }
    }
    int evicted = ra->owner[best_r];
    if (sl[evicted] < 0) sl[evicted] = (*ns)++;
    vst(c, best_r, sl[evicted]);
    ra->reg[evicted] = -1;
    ra->owner[best_r] = -1;
    return (int8_t)best_r;
}

static int8_t ra_ensure(Buf *c, struct ra *ra, int *sl, int *ns, int val) {
    if (ra->reg[val] >= 0) return ra->reg[val];
    int8_t r = ra_alloc(c, ra, sl, ns);
    if (sl[val] >= 0) vld(c, r, sl[val]);
    ra->reg[val] = r;
    ra->owner[(int)r] = val;
    return r;
}

// Claim a dying input's register for the output, transferring ownership without free/alloc.
static int8_t ra_claim(struct ra *ra, int old_val, int new_val) {
    int8_t r = ra->reg[old_val];
    ra->reg[old_val] = -1;
    ra->reg[new_val] = r;
    ra->owner[(int)r] = new_val;
    return r;
}

static void emit_varying_ops(Buf *c, struct umbra_basic_block const *bb,
                              int *sl, int *ns, struct ra *ra, _Bool scalar);

struct umbra_jit {
    void  *code;
    size_t code_size;
    void (*entry)(int, void*, void*, void*, void*, void*, void*);
};

struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) {
    int   *sl      = calloc((size_t)bb->insts, sizeof(int));

    int ns=0, ns_frame=0;
    for (int i=0; i<bb->insts; i++) {
        if (!is_store(bb->inst[i].op)) {
            if (i < bb->preamble) sl[i]=ns++;
            else sl[i]=-1;
            ns_frame++;
        }
        else sl[i]=-1;
    }

    int max_ptr=-1;
    for (int i=0; i<bb->insts; i++) {
        enum op op=bb->inst[i].op;
        if (op==op_load_16||op==op_load_32||op==op_load_half||
            op==op_store_16||op==op_store_32||op==op_store_half)
            if (bb->inst[i].ptr>max_ptr) max_ptr=bb->inst[i].ptr;
    }
    if (max_ptr>5) { free(sl); return 0; }

    Buf c={0};

    put(&c, STP_pre(29,30,31,-2));
    put(&c, ADD_xi(29,31,0));
    if (ns_frame>0) {
        int bytes = ns_frame*16;
        while (bytes>4095) { put(&c, SUB_xi(31,31,4095)); bytes-=4095; }
        if (bytes>0) put(&c, SUB_xi(31,31,bytes));
    }
    put(&c, ADD_xi(XS,31,0));

    for (int i=0; i<bb->preamble; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        int d=sl[i], x=sl[inst->x], y=sl[inst->y], z=sl[inst->z];

        if (x >= 0) vld(&c, 1, x);
        if (y >= 0) vld(&c, 2, y);
        if (z >= 0) vld(&c, 3, z);
        emit_alu_reg(&c, inst->op, 4, 1, 2, 3, inst->imm);
        vst(&c, 4, d);
    }

    put(&c, MOVZ_x(XI,0));
    put(&c, MOVI_4s_0(5));  // v5 = iota {0,1,2,3}
    put(&c, MOVZ_w(XT,1)); put(&c, INS_s(5,1,XT));
    put(&c, MOVZ_w(XT,2)); put(&c, INS_s(5,2,XT));
    put(&c, MOVZ_w(XT,3)); put(&c, INS_s(5,3,XT));

    int loop_top = c.len;

    put(&c, 0xCB090000u|(uint32_t)XT);  // SUB X10, X0, X9
    put(&c, SUBS_xi(31,XT,4));
    int br_tail = c.len;
    put(&c, Bcond(0xB,0));  // B.LT tail (patch later)
    put(&c, LSL_xi(XH, XI, 1));  // x11 = i*2  (half byte offset)
    put(&c, LSL_xi(XW, XI, 2));  // x12 = i*4  (32-bit byte offset)
    struct ra *ra = ra_create(bb);
    emit_varying_ops(&c, bb, sl, &ns, ra, 0);

    put(&c, ADD_xi(XI,XI,4));
    put(&c, B(loop_top - c.len));

    int tail_top = c.len;
    c.buf[br_tail] = Bcond(0xB, tail_top - br_tail);

    put(&c, 0xEB09001Fu);  // SUBS XZR, X0, X9
    int br_epi = c.len;
    put(&c, Bcond(0xD,0));  // B.LE (patch later)

    // Reset allocator for scalar tail loop
    for (int i = 0; i < bb->insts; i++) ra->reg[i] = -1;
    for (int i = 0; i < 32; i++) ra->owner[i] = -1;
    ra->nfree = RA_NREGS;
    for (int i = 0; i < RA_NREGS; i++) ra->free_stack[i] = ra_pool[RA_NREGS - 1 - i];

    emit_varying_ops(&c, bb, sl, &ns, ra, 1);

    put(&c, ADD_xi(XI,XI,1));
    put(&c, B(tail_top - c.len));

    int epi = c.len;
    c.buf[br_epi] = Bcond(0xD, epi - br_epi);

    put(&c, ADD_xi(31,29,0));
    put(&c, LDP_post(29,30,31,2));
    put(&c, RET());

    ra_destroy(ra); free(sl);

    size_t code_sz = (size_t)c.len * 4;
    size_t pg = 16384;
    size_t alloc = (code_sz + pg-1) & ~(pg-1);

    void *mem = mmap(NULL, alloc, PROT_READ|PROT_WRITE|PROT_EXEC,
                     MAP_PRIVATE|MAP_ANON|MAP_JIT, -1, 0);
    if (mem==MAP_FAILED) { free(c.buf); return 0; }

    pthread_jit_write_protect_np(0);
    __builtin_memcpy(mem, c.buf, code_sz);
    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(mem, code_sz);
    free(c.buf);

    struct umbra_jit *j = malloc(sizeof *j);
    j->code = mem;
    j->code_size = alloc;
    j->entry = (void(*)(int,void*,void*,void*,void*,void*,void*))mem;
    return j;
}

static void emit_varying_ops(Buf *c, struct umbra_basic_block const *bb,
                              int *sl, int *ns,
                              struct ra *ra, _Bool scalar)
{
    int *lu = ra->last_use;

    for (int i=bb->preamble; i<bb->insts; i++) {
        struct bb_inst const *inst = &bb->inst[i];

        switch (inst->op) {
        case op_lane: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            if (scalar) {
                put(c, DUP_4s_w(rd, XI));
            } else {
                put(c, DUP_4s_w(rd, XI));
                put(c, ADD_4s(rd, rd, 5));
            }
        } break;

        case op_load_32: case op_load_16: case op_load_half: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            if (bb->inst[inst->x].op == op_lane) {
                int p=inst->ptr;
                _Bool wide = (inst->op == op_load_32);
                if (scalar) {
                    put(c, wide ? LDR_sx(rd,1+p,XI) : LDR_hx(rd,1+p,XI));
                } else {
                    put(c, wide ? LDR_q(rd,1+p,XW) : LDR_d(rd,1+p,XH));
                }
            } else {
                put(c, MOVI_4s_0(rd));
            }
        } break;

        case op_store_32: case op_store_16: case op_store_half: {
            int8_t ry = ra_ensure(c, ra, sl, ns, inst->y);
            if (bb->inst[inst->x].op == op_lane) {
                int p=inst->ptr;
                _Bool wide = (inst->op == op_store_32);
                if (scalar) {
                    put(c, wide ? STR_sx(ry,1+p,XI) : STR_hx(ry,1+p,XI));
                } else {
                    put(c, wide ? STR_q(ry,1+p,XW) : STR_d(ry,1+p,XH));
                }
            }
            if (lu[inst->y] <= i) ra_free_reg(ra, inst->y);
        } break;

        default: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            int8_t ry = ra_ensure(c, ra, sl, ns, inst->y);
            int8_t rz = ra_ensure(c, ra, sl, ns, inst->z);

            _Bool x_dead = lu[inst->x] <= i;
            _Bool y_dead = lu[inst->y] <= i;
            _Bool z_dead = lu[inst->z] <= i;
            // Don't double-free when multiple args reference the same value
            if (inst->y == inst->x) y_dead = 0;
            if (inst->z == inst->x) z_dead = 0;
            if (inst->z == inst->y) z_dead = 0;

            // Try to claim a dying input's register for the output.
            // For destructive ops (FMA, BSL), prefer the accumulator/condition input
            // and avoid claiming the other inputs to prevent d aliasing them.
            int8_t rd = -1;
            enum op op = inst->op;
            _Bool destructive = op==op_fma_f32 || op==op_fma_half
                             || op==op_sel_32  || op==op_sel_16 || op==op_sel_half;
            if ((op==op_fma_f32 || op==op_fma_half) && z_dead)
                { rd = ra_claim(ra, inst->z, i); z_dead = 0; }
            else if ((op==op_sel_32 || op==op_sel_16 || op==op_sel_half) && x_dead)
                { rd = ra_claim(ra, inst->x, i); x_dead = 0; }

            // For non-destructive ops, fall back to any dead input's register.
            // For destructive ops, skip this to avoid d aliasing non-accumulator inputs.
            if (!destructive) {
                if (rd < 0 && x_dead) { rd = ra_claim(ra, inst->x, i); x_dead = 0; }
                if (rd < 0 && y_dead) { rd = ra_claim(ra, inst->y, i); y_dead = 0; }
                if (rd < 0 && z_dead) { rd = ra_claim(ra, inst->z, i); z_dead = 0; }
            }

            // Free remaining dead inputs
            if (x_dead) ra_free_reg(ra, inst->x);
            if (y_dead) ra_free_reg(ra, inst->y);
            if (z_dead) ra_free_reg(ra, inst->z);

            // If no dead input available, allocate fresh
            if (rd < 0) {
                rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
            }

            emit_alu_reg(c, inst->op, rd, rx, ry, rz, inst->imm);
        } break;
        }
    }
}

void umbra_jit_run(struct umbra_jit *j, int n, void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    if (j) j->entry(n, p0, p1, p2, p3, p4, p5);
}

void umbra_jit_free(struct umbra_jit *j) {
    if (!j) return;
    munmap(j->code, j->code_size);
    free(j);
}

void umbra_jit_dump(struct umbra_jit const *j, FILE *f) {
    if (!j) return;
    size_t code_bytes = j->code_size;
    // Count actual instructions (find last non-zero or just use code_size)
    uint32_t const *words = (uint32_t const *)j->code;
    size_t nwords = code_bytes / 4;

    // Write .s file with .inst directives, assemble, then objdump
    char tmp[] = "/tmp/umbra_jit_XXXXXX.s";
    int fd = mkstemps(tmp, 2);
    if (fd >= 0) {
        FILE *fp = fdopen(fd, "w");
        if (fp) {
            for (size_t i = 0; i < nwords; i++) {
                fprintf(fp, ".inst 0x%08x\n", words[i]);
            }
            fclose(fp);

            char opath[sizeof tmp + 2];
            snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof tmp - 3), tmp);

            char cmd[1024];
            snprintf(cmd, sizeof cmd,
                     "as -o %s %s 2>/dev/null && "
                     "/opt/homebrew/opt/llvm/bin/llvm-objdump -d --no-show-raw-insn --no-leading-addr %s 2>/dev/null",
                     opath, tmp, opath);
            FILE *p = popen(cmd, "r");
            if (p) {
                char line[256];
                _Bool ok = 0;
                while (fgets(line, (int)sizeof line, p)) {
                    // Skip the "file.o: file format ..." header line.
                    if (!ok && __builtin_strstr(line, "file format")) { ok = 1; continue; }
                    fputs(line, f);
                }
                int rc = pclose(p);
                remove(tmp);
                remove(opath);
                if (ok && rc == 0) return;
            }
            remove(tmp);
            remove(opath);
        } else {
            close(fd);
            remove(tmp);
        }
    }

    // Fallback: hex dump
    for (size_t i = 0; i < nwords; i++) {
        fprintf(f, "  %04zx: %08x\n", i * 4, words[i]);
    }
}

#endif
