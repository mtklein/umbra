#include "../umbra.h"
#include "bb.h"

#if !defined(__aarch64__)

struct umbra_jit { int dummy; };
struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) { (void)bb; return 0; }
void umbra_jit_run (struct umbra_jit *j, int n, void *ptr[]) { (void)j; (void)n; (void)ptr; }
void umbra_jit_free(struct umbra_jit *j) { (void)j; }

#else

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
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
static uint32_t LDR_xi(int d, int n, int imm12) {  // LDR Xd,[Xn,#imm*8]
    return 0xF9400000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
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

#undef V3
#undef V2

static uint32_t MOVI_4s_0(int d) { return 0x4F000400u|(uint32_t)d; }
static uint32_t DUP_4s_w(int d, int n)  { return 0x4E040C00u|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t DUP_4h_w(int d, int n)  { return 0x0E020C00u|((uint32_t)n<<5)|(uint32_t)d; }

static uint32_t INS_s(int d, int idx, int n) {
    uint32_t imm5 = (uint32_t)(idx<<3)|4;
    return 0x4E001C00u|(imm5<<16)|((uint32_t)n<<5)|(uint32_t)d;
}

// x0=n, x1=ptr[], x2..x8=ptr[0..6], x9=loop i, x10=scratch, x15=stack base
enum { XI=9, XT=10, XS=15 };

static void load_imm_w(Buf *c, int rd, uint32_t v) {
    put(c, MOVZ_w(rd, (uint16_t)(v&0xffff)));
    if (v>>16) put(c, MOVK_w16(rd, (uint16_t)(v>>16)));
}

static void vld(Buf *c, int vd, int s) { put(c, LDR_qi(vd, XS, s)); }
static void vst(Buf *c, int vd, int s) { put(c, STR_qi(vd, XS, s)); }

static void emit_varying_ops(Buf *c, struct umbra_basic_block const *bb,
                              _Bool *live, _Bool *varying, int *sl, _Bool scalar);

struct umbra_jit {
    void  *code;
    size_t code_size;
    void (*entry)(int n, void* ptr[]);
};

struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) {
    _Bool *live    = calloc((size_t)bb->insts, 1);
    _Bool *varying = calloc((size_t)bb->insts, 1);
    int   *sl      = calloc((size_t)bb->insts, sizeof(int));

    for (int i = bb->insts; i-->0;) {
        if (is_store(bb->inst[i].op)) live[i]=1;
        if (live[i]) {
            live[bb->inst[i].x]=1;
            live[bb->inst[i].y]=1;
            live[bb->inst[i].z]=1;
        }
    }
    for (int i=0; i<bb->insts; i++) {
        varying[i] = (bb->inst[i].op==op_lane)
                    | varying[bb->inst[i].x]
                    | varying[bb->inst[i].y]
                    | varying[bb->inst[i].z];
    }

    int ns=0;
    for (int i=0; i<bb->insts; i++) {
        if (live[i] && !is_store(bb->inst[i].op)) sl[i]=ns++;
        else sl[i]=-1;
    }

    int max_ptr=-1;
    for (int i=0; i<bb->insts; i++) {
        if (!live[i]) continue;
        enum op op=bb->inst[i].op;
        if (op==op_load_16||op==op_load_32||op==op_load_half||
            op==op_store_16||op==op_store_32||op==op_store_half)
            if (bb->inst[i].ptr>max_ptr) max_ptr=bb->inst[i].ptr;
    }
    if (max_ptr>6) { free(live); free(varying); free(sl); return 0; }

    Buf c={0};

    put(&c, STP_pre(29,30,31,-2));
    put(&c, ADD_xi(29,31,0));
    if (ns>0) {
        int bytes = ns*16;
        while (bytes>4095) { put(&c, SUB_xi(31,31,4095)); bytes-=4095; }
        if (bytes>0) put(&c, SUB_xi(31,31,bytes));
    }
    put(&c, ADD_xi(XS,31,0));
    for (int p=0; p<=max_ptr; p++) put(&c, LDR_xi(2+p,1,p));

    for (int i=0; i<bb->insts; i++) {
        if (!live[i] || varying[i] || is_store(bb->inst[i].op)) continue;
        struct bb_inst const *inst = &bb->inst[i];
        int d=sl[i], x=sl[inst->x], y=sl[inst->y], z=sl[inst->z];

        switch (inst->op) {
        case op_imm_32: {
            uint32_t imm=(uint32_t)inst->imm;
            if (imm==0) { put(&c, MOVI_4s_0(0)); }
            else { load_imm_w(&c,XT,imm); put(&c, DUP_4s_w(0,XT)); }
            vst(&c,0,d);
        } break;
        case op_imm_16: case op_imm_half: {
            uint16_t imm=(uint16_t)inst->imm;
            put(&c, MOVZ_w(XT,imm));
            put(&c, DUP_4h_w(0,XT));
            vst(&c,0,d);
        } break;

        case op_add_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FADD_4s(2,0,1));  vst(&c,2,d); break;
        case op_sub_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FSUB_4s(2,0,1));  vst(&c,2,d); break;
        case op_mul_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FMUL_4s(2,0,1));  vst(&c,2,d); break;
        case op_div_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FDIV_4s(2,0,1));  vst(&c,2,d); break;
        case op_min_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FMINNM_4s(2,0,1));vst(&c,2,d); break;
        case op_max_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FMAXNM_4s(2,0,1));vst(&c,2,d); break;
        case op_sqrt_f32: vld(&c,0,x); put(&c, FSQRT_4s(2,0)); vst(&c,2,d); break;
        case op_fma_f32:
            vld(&c,2,z); vld(&c,0,x); vld(&c,1,y);
            put(&c, FMLA_4s(2,0,1)); vst(&c,2,d); break;

        case op_add_i32: vld(&c,0,x); vld(&c,1,y); put(&c, ADD_4s(2,0,1)); vst(&c,2,d); break;
        case op_sub_i32: vld(&c,0,x); vld(&c,1,y); put(&c, SUB_4s(2,0,1)); vst(&c,2,d); break;
        case op_mul_i32: vld(&c,0,x); vld(&c,1,y); put(&c, MUL_4s(2,0,1)); vst(&c,2,d); break;
        case op_shl_i32: vld(&c,0,x); vld(&c,1,y); put(&c, USHL_4s(2,0,1)); vst(&c,2,d); break;
        case op_shr_u32:
            vld(&c,0,x); vld(&c,1,y); put(&c, NEG_4s(1,1));
            put(&c, USHL_4s(2,0,1)); vst(&c,2,d); break;
        case op_shr_s32:
            vld(&c,0,x); vld(&c,1,y); put(&c, NEG_4s(1,1));
            put(&c, SSHL_4s(2,0,1)); vst(&c,2,d); break;

        case op_and_32: vld(&c,0,x); vld(&c,1,y); put(&c, AND_16b(2,0,1)); vst(&c,2,d); break;
        case op_or_32:  vld(&c,0,x); vld(&c,1,y); put(&c, ORR_16b(2,0,1)); vst(&c,2,d); break;
        case op_xor_32: vld(&c,0,x); vld(&c,1,y); put(&c, EOR_16b(2,0,1)); vst(&c,2,d); break;
        case op_sel_32:
            vld(&c,3,x); vld(&c,1,y); vld(&c,2,z);
            put(&c, BSL_16b(3,1,2)); vst(&c,3,d); break;

        case op_f32_from_i32: vld(&c,0,x); put(&c, SCVTF_4s(2,0)); vst(&c,2,d); break;
        case op_i32_from_f32: vld(&c,0,x); put(&c, FCVTZS_4s(2,0)); vst(&c,2,d); break;
        case op_half_from_f32: vld(&c,0,x); put(&c, FCVTN_4h(2,0)); vst(&c,2,d); break;
        case op_f32_from_half: vld(&c,0,x); put(&c, FCVTL_4s(2,0)); vst(&c,2,d); break;

        case op_eq_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FCMEQ_4s(2,0,1)); vst(&c,2,d); break;
        case op_ne_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FCMEQ_4s(2,0,1)); put(&c, MVN_16b(2,2)); vst(&c,2,d); break;
        case op_gt_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FCMGT_4s(2,0,1)); vst(&c,2,d); break;
        case op_ge_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FCMGE_4s(2,0,1)); vst(&c,2,d); break;
        case op_lt_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FCMGT_4s(2,1,0)); vst(&c,2,d); break;
        case op_le_f32: vld(&c,0,x); vld(&c,1,y); put(&c, FCMGE_4s(2,1,0)); vst(&c,2,d); break;

        case op_eq_i32: vld(&c,0,x); vld(&c,1,y); put(&c, CMEQ_4s(2,0,1)); vst(&c,2,d); break;
        case op_ne_i32: vld(&c,0,x); vld(&c,1,y); put(&c, CMEQ_4s(2,0,1)); put(&c, MVN_16b(2,2)); vst(&c,2,d); break;
        case op_gt_s32: vld(&c,0,x); vld(&c,1,y); put(&c, CMGT_4s(2,0,1)); vst(&c,2,d); break;
        case op_ge_s32: vld(&c,0,x); vld(&c,1,y); put(&c, CMGE_4s(2,0,1)); vst(&c,2,d); break;
        case op_lt_s32: vld(&c,0,x); vld(&c,1,y); put(&c, CMGT_4s(2,1,0)); vst(&c,2,d); break;
        case op_le_s32: vld(&c,0,x); vld(&c,1,y); put(&c, CMGE_4s(2,1,0)); vst(&c,2,d); break;
        case op_gt_u32: vld(&c,0,x); vld(&c,1,y); put(&c, CMHI_4s(2,0,1)); vst(&c,2,d); break;
        case op_ge_u32: vld(&c,0,x); vld(&c,1,y); put(&c, CMHS_4s(2,0,1)); vst(&c,2,d); break;
        case op_lt_u32: vld(&c,0,x); vld(&c,1,y); put(&c, CMHI_4s(2,1,0)); vst(&c,2,d); break;
        case op_le_u32: vld(&c,0,x); vld(&c,1,y); put(&c, CMHS_4s(2,1,0)); vst(&c,2,d); break;

        case op_add_i16: vld(&c,0,x); vld(&c,1,y); put(&c, ADD_4h(2,0,1)); vst(&c,2,d); break;
        case op_sub_i16: vld(&c,0,x); vld(&c,1,y); put(&c, SUB_4h(2,0,1)); vst(&c,2,d); break;
        case op_mul_i16: vld(&c,0,x); vld(&c,1,y); put(&c, MUL_4h(2,0,1)); vst(&c,2,d); break;
        case op_shl_i16: vld(&c,0,x); vld(&c,1,y); put(&c, USHL_4h(2,0,1)); vst(&c,2,d); break;
        case op_shr_u16: vld(&c,0,x); vld(&c,1,y); put(&c, NEG_4h(1,1)); put(&c, USHL_4h(2,0,1)); vst(&c,2,d); break;
        case op_shr_s16: vld(&c,0,x); vld(&c,1,y); put(&c, NEG_4h(1,1)); put(&c, SSHL_4h(2,0,1)); vst(&c,2,d); break;
        case op_and_16: vld(&c,0,x); vld(&c,1,y); put(&c, AND_8b(2,0,1)); vst(&c,2,d); break;
        case op_or_16:  vld(&c,0,x); vld(&c,1,y); put(&c, ORR_8b(2,0,1)); vst(&c,2,d); break;
        case op_xor_16: vld(&c,0,x); vld(&c,1,y); put(&c, EOR_8b(2,0,1)); vst(&c,2,d); break;
        case op_sel_16:
            vld(&c,3,x); vld(&c,1,y); vld(&c,2,z);
            put(&c, BSL_8b(3,1,2)); vst(&c,3,d); break;
        case op_eq_i16: vld(&c,0,x); vld(&c,1,y); put(&c, CMEQ_4h(2,0,1)); vst(&c,2,d); break;
        case op_ne_i16: vld(&c,0,x); vld(&c,1,y); put(&c, CMEQ_4h(2,0,1)); put(&c, MVN_8b(2,2)); vst(&c,2,d); break;
        case op_gt_s16: vld(&c,0,x); vld(&c,1,y); put(&c, CMGT_4h(2,0,1)); vst(&c,2,d); break;
        case op_ge_s16: vld(&c,0,x); vld(&c,1,y); put(&c, CMGE_4h(2,0,1)); vst(&c,2,d); break;
        case op_lt_s16: vld(&c,0,x); vld(&c,1,y); put(&c, CMGT_4h(2,1,0)); vst(&c,2,d); break;
        case op_le_s16: vld(&c,0,x); vld(&c,1,y); put(&c, CMGE_4h(2,1,0)); vst(&c,2,d); break;
        case op_gt_u16: vld(&c,0,x); vld(&c,1,y); put(&c, CMHI_4h(2,0,1)); vst(&c,2,d); break;
        case op_ge_u16: vld(&c,0,x); vld(&c,1,y); put(&c, CMHS_4h(2,0,1)); vst(&c,2,d); break;
        case op_lt_u16: vld(&c,0,x); vld(&c,1,y); put(&c, CMHI_4h(2,1,0)); vst(&c,2,d); break;
        case op_le_u16: vld(&c,0,x); vld(&c,1,y); put(&c, CMHS_4h(2,1,0)); vst(&c,2,d); break;

        case op_add_half: vld(&c,0,x); vld(&c,1,y); put(&c, FADD_4h(2,0,1)); vst(&c,2,d); break;
        case op_sub_half: vld(&c,0,x); vld(&c,1,y); put(&c, FSUB_4h(2,0,1)); vst(&c,2,d); break;
        case op_mul_half: vld(&c,0,x); vld(&c,1,y); put(&c, FMUL_4h(2,0,1)); vst(&c,2,d); break;
        case op_div_half: vld(&c,0,x); vld(&c,1,y); put(&c, FDIV_4h(2,0,1)); vst(&c,2,d); break;
        case op_min_half: vld(&c,0,x); vld(&c,1,y); put(&c, FMINNM_4h(2,0,1)); vst(&c,2,d); break;
        case op_max_half: vld(&c,0,x); vld(&c,1,y); put(&c, FMAXNM_4h(2,0,1)); vst(&c,2,d); break;
        case op_sqrt_half: vld(&c,0,x); put(&c, FSQRT_4h(2,0)); vst(&c,2,d); break;
        case op_fma_half:
            vld(&c,2,z); vld(&c,0,x); vld(&c,1,y);
            put(&c, FMLA_4h(2,0,1)); vst(&c,2,d); break;
        case op_and_half: vld(&c,0,x); vld(&c,1,y); put(&c, AND_8b(2,0,1)); vst(&c,2,d); break;
        case op_or_half:  vld(&c,0,x); vld(&c,1,y); put(&c, ORR_8b(2,0,1)); vst(&c,2,d); break;
        case op_xor_half: vld(&c,0,x); vld(&c,1,y); put(&c, EOR_8b(2,0,1)); vst(&c,2,d); break;
        case op_sel_half:
            vld(&c,3,x); vld(&c,1,y); vld(&c,2,z);
            put(&c, BSL_8b(3,1,2)); vst(&c,3,d); break;
        case op_eq_half: vld(&c,0,x); vld(&c,1,y); put(&c, FCMEQ_4h(2,0,1)); vst(&c,2,d); break;
        case op_ne_half: vld(&c,0,x); vld(&c,1,y); put(&c, FCMEQ_4h(2,0,1)); put(&c, MVN_8b(2,2)); vst(&c,2,d); break;
        case op_gt_half: vld(&c,0,x); vld(&c,1,y); put(&c, FCMGT_4h(2,0,1)); vst(&c,2,d); break;
        case op_ge_half: vld(&c,0,x); vld(&c,1,y); put(&c, FCMGE_4h(2,0,1)); vst(&c,2,d); break;
        case op_lt_half: vld(&c,0,x); vld(&c,1,y); put(&c, FCMGT_4h(2,1,0)); vst(&c,2,d); break;
        case op_le_half: vld(&c,0,x); vld(&c,1,y); put(&c, FCMGE_4h(2,1,0)); vst(&c,2,d); break;

        case op_lane: case op_load_16: case op_load_32: case op_load_half:
        case op_store_16: case op_store_32: case op_store_half:
            break;
        }
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
    emit_varying_ops(&c, bb, live, varying, sl, 0);

    put(&c, ADD_xi(XI,XI,4));
    put(&c, B(loop_top - c.len));

    int tail_top = c.len;
    c.buf[br_tail] = Bcond(0xB, tail_top - br_tail);

    put(&c, 0xEB09001Fu);  // SUBS XZR, X0, X9
    int br_epi = c.len;
    put(&c, Bcond(0xD,0));  // B.LE (patch later)

    emit_varying_ops(&c, bb, live, varying, sl, 1);

    put(&c, ADD_xi(XI,XI,1));
    put(&c, B(tail_top - c.len));

    int epi = c.len;
    c.buf[br_epi] = Bcond(0xD, epi - br_epi);

    put(&c, ADD_xi(31,29,0));
    put(&c, LDP_post(29,30,31,2));
    put(&c, RET());

    free(live); free(varying); free(sl);

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
    j->entry = (void(*)(int,void*[]))mem;
    return j;
}

static void emit_varying_ops(Buf *c, struct umbra_basic_block const *bb,
                              _Bool *live, _Bool *varying, int *sl, _Bool scalar)
{
    for (int i=0; i<bb->insts; i++) {
        if (!live[i] || !varying[i]) continue;
        struct bb_inst const *inst = &bb->inst[i];
        int d = is_store(inst->op) ? -1 : sl[i];
        int x = sl[inst->x], y = sl[inst->y], z = sl[inst->z];

        switch (inst->op) {
        case op_lane:
            if (scalar) {
                put(c, DUP_4s_w(0, XI));
            } else {
                put(c, DUP_4s_w(0, XI));
                put(c, ADD_4s(0, 0, 5)); // add iota
            }
            vst(c,0,d); break;

        case op_load_32:
            if (bb->inst[inst->x].op == op_lane) {
                int p=inst->ptr;
                if (scalar) {
                    put(c, LDR_sx(0, 2+p, XI));
                } else {
                    put(c, LSL_xi(XT, XI, 2));  // byte offset = i*4
                    put(c, LDR_q(0, 2+p, XT));
                }
                vst(c,0,d);
            } else {
                put(c, MOVI_4s_0(0)); vst(c,0,d);
            } break;
        case op_load_16:
            if (bb->inst[inst->x].op == op_lane) {
                int p=inst->ptr;
                if (scalar) {
                    put(c, LDR_hx(0, 2+p, XI));
                } else {
                    put(c, LSL_xi(XT, XI, 1));  // byte offset = i*2
                    put(c, LDR_d(0, 2+p, XT));
                }
                vst(c,0,d);
            } else {
                put(c, MOVI_4s_0(0)); vst(c,0,d);
            } break;
        case op_load_half:
            if (bb->inst[inst->x].op == op_lane) {
                int p=inst->ptr;
                if (scalar) {
                    put(c, LDR_hx(0, 2+p, XI));
                } else {
                    put(c, LSL_xi(XT, XI, 1));  // byte offset = i*2
                    put(c, LDR_d(0, 2+p, XT));
                }
                vst(c,0,d);
            } else {
                put(c, MOVI_4s_0(0)); vst(c,0,d);
            } break;

        case op_store_32:
            if (bb->inst[inst->x].op == op_lane) {
                int p=inst->ptr;
                vld(c,0,y);
                if (scalar) {
                    put(c, STR_sx(0, 2+p, XI));
                } else {
                    put(c, LSL_xi(XT, XI, 2));
                    put(c, STR_q(0, 2+p, XT));
                }
            } break;
        case op_store_16:
            if (bb->inst[inst->x].op == op_lane) {
                int p=inst->ptr;
                vld(c,0,y);
                if (scalar) {
                    put(c, STR_hx(0, 2+p, XI));
                } else {
                    put(c, LSL_xi(XT, XI, 1));
                    put(c, STR_d(0, 2+p, XT));
                }
            } break;
        case op_store_half:
            if (bb->inst[inst->x].op == op_lane) {
                int p=inst->ptr;
                vld(c,0,y);
                if (scalar) {
                    put(c, STR_hx(0, 2+p, XI));
                } else {
                    put(c, LSL_xi(XT, XI, 1));
                    put(c, STR_d(0, 2+p, XT));
                }
            } break;

        case op_imm_32: {
            uint32_t imm=(uint32_t)inst->imm;
            if (imm==0) put(c, MOVI_4s_0(0));
            else { load_imm_w(c,XT,imm); put(c, DUP_4s_w(0,XT)); }
            vst(c,0,d);
        } break;
        case op_imm_16: case op_imm_half: {
            uint16_t imm=(uint16_t)inst->imm;
            put(c, MOVZ_w(XT,imm));
            put(c, DUP_4h_w(0,XT));
            vst(c,0,d);
        } break;

        case op_add_f32: vld(c,0,x); vld(c,1,y); put(c, FADD_4s(2,0,1));  vst(c,2,d); break;
        case op_sub_f32: vld(c,0,x); vld(c,1,y); put(c, FSUB_4s(2,0,1));  vst(c,2,d); break;
        case op_mul_f32: vld(c,0,x); vld(c,1,y); put(c, FMUL_4s(2,0,1));  vst(c,2,d); break;
        case op_div_f32: vld(c,0,x); vld(c,1,y); put(c, FDIV_4s(2,0,1));  vst(c,2,d); break;
        case op_min_f32: vld(c,0,x); vld(c,1,y); put(c, FMINNM_4s(2,0,1));vst(c,2,d); break;
        case op_max_f32: vld(c,0,x); vld(c,1,y); put(c, FMAXNM_4s(2,0,1));vst(c,2,d); break;
        case op_sqrt_f32: vld(c,0,x); put(c, FSQRT_4s(2,0)); vst(c,2,d); break;
        case op_fma_f32:
            vld(c,2,z); vld(c,0,x); vld(c,1,y);
            put(c, FMLA_4s(2,0,1)); vst(c,2,d); break;

        case op_add_i32: vld(c,0,x); vld(c,1,y); put(c, ADD_4s(2,0,1)); vst(c,2,d); break;
        case op_sub_i32: vld(c,0,x); vld(c,1,y); put(c, SUB_4s(2,0,1)); vst(c,2,d); break;
        case op_mul_i32: vld(c,0,x); vld(c,1,y); put(c, MUL_4s(2,0,1)); vst(c,2,d); break;
        case op_shl_i32: vld(c,0,x); vld(c,1,y); put(c, USHL_4s(2,0,1)); vst(c,2,d); break;
        case op_shr_u32:
            vld(c,0,x); vld(c,1,y); put(c, NEG_4s(1,1));
            put(c, USHL_4s(2,0,1)); vst(c,2,d); break;
        case op_shr_s32:
            vld(c,0,x); vld(c,1,y); put(c, NEG_4s(1,1));
            put(c, SSHL_4s(2,0,1)); vst(c,2,d); break;

        case op_and_32: vld(c,0,x); vld(c,1,y); put(c, AND_16b(2,0,1)); vst(c,2,d); break;
        case op_or_32:  vld(c,0,x); vld(c,1,y); put(c, ORR_16b(2,0,1)); vst(c,2,d); break;
        case op_xor_32: vld(c,0,x); vld(c,1,y); put(c, EOR_16b(2,0,1)); vst(c,2,d); break;
        case op_sel_32:
            vld(c,3,x); vld(c,1,y); vld(c,2,z);
            put(c, BSL_16b(3,1,2)); vst(c,3,d); break;

        case op_f32_from_i32: vld(c,0,x); put(c, SCVTF_4s(2,0)); vst(c,2,d); break;
        case op_i32_from_f32: vld(c,0,x); put(c, FCVTZS_4s(2,0)); vst(c,2,d); break;
        case op_half_from_f32: vld(c,0,x); put(c, FCVTN_4h(2,0)); vst(c,2,d); break;
        case op_f32_from_half: vld(c,0,x); put(c, FCVTL_4s(2,0)); vst(c,2,d); break;

        case op_eq_f32: vld(c,0,x); vld(c,1,y); put(c, FCMEQ_4s(2,0,1)); vst(c,2,d); break;
        case op_ne_f32: vld(c,0,x); vld(c,1,y); put(c, FCMEQ_4s(2,0,1)); put(c, MVN_16b(2,2)); vst(c,2,d); break;
        case op_gt_f32: vld(c,0,x); vld(c,1,y); put(c, FCMGT_4s(2,0,1)); vst(c,2,d); break;
        case op_ge_f32: vld(c,0,x); vld(c,1,y); put(c, FCMGE_4s(2,0,1)); vst(c,2,d); break;
        case op_lt_f32: vld(c,0,x); vld(c,1,y); put(c, FCMGT_4s(2,1,0)); vst(c,2,d); break;
        case op_le_f32: vld(c,0,x); vld(c,1,y); put(c, FCMGE_4s(2,1,0)); vst(c,2,d); break;

        case op_eq_i32: vld(c,0,x); vld(c,1,y); put(c, CMEQ_4s(2,0,1)); vst(c,2,d); break;
        case op_ne_i32: vld(c,0,x); vld(c,1,y); put(c, CMEQ_4s(2,0,1)); put(c, MVN_16b(2,2)); vst(c,2,d); break;
        case op_gt_s32: vld(c,0,x); vld(c,1,y); put(c, CMGT_4s(2,0,1)); vst(c,2,d); break;
        case op_ge_s32: vld(c,0,x); vld(c,1,y); put(c, CMGE_4s(2,0,1)); vst(c,2,d); break;
        case op_lt_s32: vld(c,0,x); vld(c,1,y); put(c, CMGT_4s(2,1,0)); vst(c,2,d); break;
        case op_le_s32: vld(c,0,x); vld(c,1,y); put(c, CMGE_4s(2,1,0)); vst(c,2,d); break;
        case op_gt_u32: vld(c,0,x); vld(c,1,y); put(c, CMHI_4s(2,0,1)); vst(c,2,d); break;
        case op_ge_u32: vld(c,0,x); vld(c,1,y); put(c, CMHS_4s(2,0,1)); vst(c,2,d); break;
        case op_lt_u32: vld(c,0,x); vld(c,1,y); put(c, CMHI_4s(2,1,0)); vst(c,2,d); break;
        case op_le_u32: vld(c,0,x); vld(c,1,y); put(c, CMHS_4s(2,1,0)); vst(c,2,d); break;

        case op_add_i16: vld(c,0,x); vld(c,1,y); put(c, ADD_4h(2,0,1)); vst(c,2,d); break;
        case op_sub_i16: vld(c,0,x); vld(c,1,y); put(c, SUB_4h(2,0,1)); vst(c,2,d); break;
        case op_mul_i16: vld(c,0,x); vld(c,1,y); put(c, MUL_4h(2,0,1)); vst(c,2,d); break;
        case op_shl_i16: vld(c,0,x); vld(c,1,y); put(c, USHL_4h(2,0,1)); vst(c,2,d); break;
        case op_shr_u16: vld(c,0,x); vld(c,1,y); put(c, NEG_4h(1,1)); put(c, USHL_4h(2,0,1)); vst(c,2,d); break;
        case op_shr_s16: vld(c,0,x); vld(c,1,y); put(c, NEG_4h(1,1)); put(c, SSHL_4h(2,0,1)); vst(c,2,d); break;
        case op_and_16: vld(c,0,x); vld(c,1,y); put(c, AND_8b(2,0,1)); vst(c,2,d); break;
        case op_or_16:  vld(c,0,x); vld(c,1,y); put(c, ORR_8b(2,0,1)); vst(c,2,d); break;
        case op_xor_16: vld(c,0,x); vld(c,1,y); put(c, EOR_8b(2,0,1)); vst(c,2,d); break;
        case op_sel_16:
            vld(c,3,x); vld(c,1,y); vld(c,2,z);
            put(c, BSL_8b(3,1,2)); vst(c,3,d); break;
        case op_eq_i16: vld(c,0,x); vld(c,1,y); put(c, CMEQ_4h(2,0,1)); vst(c,2,d); break;
        case op_ne_i16: vld(c,0,x); vld(c,1,y); put(c, CMEQ_4h(2,0,1)); put(c, MVN_8b(2,2)); vst(c,2,d); break;
        case op_gt_s16: vld(c,0,x); vld(c,1,y); put(c, CMGT_4h(2,0,1)); vst(c,2,d); break;
        case op_ge_s16: vld(c,0,x); vld(c,1,y); put(c, CMGE_4h(2,0,1)); vst(c,2,d); break;
        case op_lt_s16: vld(c,0,x); vld(c,1,y); put(c, CMGT_4h(2,1,0)); vst(c,2,d); break;
        case op_le_s16: vld(c,0,x); vld(c,1,y); put(c, CMGE_4h(2,1,0)); vst(c,2,d); break;
        case op_gt_u16: vld(c,0,x); vld(c,1,y); put(c, CMHI_4h(2,0,1)); vst(c,2,d); break;
        case op_ge_u16: vld(c,0,x); vld(c,1,y); put(c, CMHS_4h(2,0,1)); vst(c,2,d); break;
        case op_lt_u16: vld(c,0,x); vld(c,1,y); put(c, CMHI_4h(2,1,0)); vst(c,2,d); break;
        case op_le_u16: vld(c,0,x); vld(c,1,y); put(c, CMHS_4h(2,1,0)); vst(c,2,d); break;

        case op_add_half: vld(c,0,x); vld(c,1,y); put(c, FADD_4h(2,0,1)); vst(c,2,d); break;
        case op_sub_half: vld(c,0,x); vld(c,1,y); put(c, FSUB_4h(2,0,1)); vst(c,2,d); break;
        case op_mul_half: vld(c,0,x); vld(c,1,y); put(c, FMUL_4h(2,0,1)); vst(c,2,d); break;
        case op_div_half: vld(c,0,x); vld(c,1,y); put(c, FDIV_4h(2,0,1)); vst(c,2,d); break;
        case op_min_half: vld(c,0,x); vld(c,1,y); put(c, FMINNM_4h(2,0,1)); vst(c,2,d); break;
        case op_max_half: vld(c,0,x); vld(c,1,y); put(c, FMAXNM_4h(2,0,1)); vst(c,2,d); break;
        case op_sqrt_half: vld(c,0,x); put(c, FSQRT_4h(2,0)); vst(c,2,d); break;
        case op_fma_half:
            vld(c,2,z); vld(c,0,x); vld(c,1,y);
            put(c, FMLA_4h(2,0,1)); vst(c,2,d); break;
        case op_and_half: vld(c,0,x); vld(c,1,y); put(c, AND_8b(2,0,1)); vst(c,2,d); break;
        case op_or_half:  vld(c,0,x); vld(c,1,y); put(c, ORR_8b(2,0,1)); vst(c,2,d); break;
        case op_xor_half: vld(c,0,x); vld(c,1,y); put(c, EOR_8b(2,0,1)); vst(c,2,d); break;
        case op_sel_half:
            vld(c,3,x); vld(c,1,y); vld(c,2,z);
            put(c, BSL_8b(3,1,2)); vst(c,3,d); break;
        case op_eq_half: vld(c,0,x); vld(c,1,y); put(c, FCMEQ_4h(2,0,1)); vst(c,2,d); break;
        case op_ne_half: vld(c,0,x); vld(c,1,y); put(c, FCMEQ_4h(2,0,1)); put(c, MVN_8b(2,2)); vst(c,2,d); break;
        case op_gt_half: vld(c,0,x); vld(c,1,y); put(c, FCMGT_4h(2,0,1)); vst(c,2,d); break;
        case op_ge_half: vld(c,0,x); vld(c,1,y); put(c, FCMGE_4h(2,0,1)); vst(c,2,d); break;
        case op_lt_half: vld(c,0,x); vld(c,1,y); put(c, FCMGT_4h(2,1,0)); vst(c,2,d); break;
        case op_le_half: vld(c,0,x); vld(c,1,y); put(c, FCMGE_4h(2,1,0)); vst(c,2,d); break;
        }
    }
}

void umbra_jit_run(struct umbra_jit *j, int n, void *ptr[]) {
    if (j) j->entry(n, ptr);
}

void umbra_jit_free(struct umbra_jit *j) {
    if (!j) return;
    munmap(j->code, j->code_size);
    free(j);
}

#endif
