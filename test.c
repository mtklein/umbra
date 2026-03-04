#include "len.h"
#include "test.h"
#include "umbra.h"
#include <stdint.h>

// Helper: build bb for binop(load(ptr0), load(ptr1)) → store(ptr2), assign interpreter to p_.
#define BB_BUILD_BINOP_32(op_fn, bb_) do { \
    bb_ = umbra_basic_block(); \
    umbra_v32 ix_ = umbra_lane(bb_), \
              a_  = umbra_load_32(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_32(bb_, (umbra_ptr){1}, ix_), \
              r_  = op_fn(bb_, a_, b_); \
    umbra_store_32(bb_, (umbra_ptr){2}, ix_, r_); \
} while(0)

#define BB_BUILD_BINOP_16(op_fn, bb_) do { \
    bb_ = umbra_basic_block(); \
    umbra_v32 ix_ = umbra_lane(bb_); \
    umbra_v16 a_  = umbra_load_16(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_16(bb_, (umbra_ptr){1}, ix_), \
              r_  = op_fn(bb_, a_, b_); \
    umbra_store_16(bb_, (umbra_ptr){2}, ix_, r_); \
} while(0)

#define BB_BINOP_32(op_fn, p_) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_v32 ix_ = umbra_lane(bb_), \
              a_  = umbra_load_32(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_32(bb_, (umbra_ptr){1}, ix_), \
              r_  = op_fn(bb_, a_, b_); \
    umbra_store_32(bb_, (umbra_ptr){2}, ix_, r_); \
    p_ = umbra_interpreter(bb_); \
    umbra_basic_block_free(bb_); \
} while(0)

#define BB_BINOP_16(op_fn, p_) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_v32 ix_ = umbra_lane(bb_); \
    umbra_v16 a_  = umbra_load_16(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_16(bb_, (umbra_ptr){1}, ix_), \
              r_  = op_fn(bb_, a_, b_); \
    umbra_store_16(bb_, (umbra_ptr){2}, ix_, r_); \
    p_ = umbra_interpreter(bb_); \
    umbra_basic_block_free(bb_); \
} while(0)

static void test_f32_ops(void) {
    // mul
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_mul_f32, p);
        float x[] = {1,2,3,4,5}, y[] = {6,7,8,9,0}, z[5] = {0};
        umbra_interpreter_run(p, 5, (void*[]){x,y,z});
        equiv(z[0],  6) here; equiv(z[1], 14) here; equiv(z[2], 24) here;
        equiv(z[3], 36) here; equiv(z[4],  0) here;
        umbra_interpreter_free(p);
    }
    // add
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_add_f32, p);
        float x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 11) here; equiv(z[1], 22) here; equiv(z[2], 33) here;
        umbra_interpreter_free(p);
    }
    // sub
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_sub_f32, p);
        float x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 9) here; equiv(z[1], 18) here; equiv(z[2], 27) here;
        umbra_interpreter_free(p);
    }
    // div
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_div_f32, p);
        float x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 5) here; equiv(z[1], 5) here; equiv(z[2], 6) here;
        umbra_interpreter_free(p);
    }
}

static void test_i32_ops(void) {
    // add
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_add_i32, p);
        int x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 11) here; (z[1] == 22) here; (z[2] == 33) here;
        umbra_interpreter_free(p);
    }
    // sub
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_sub_i32, p);
        int x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 9) here; (z[1] == 18) here; (z[2] == 27) here;
        umbra_interpreter_free(p);
    }
    // mul
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_mul_i32, p);
        int x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 10) here; (z[1] == 18) here; (z[2] == 28) here;
        umbra_interpreter_free(p);
    }
    // shl
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_shl_i32, p);
        int x[] = {1,3,7}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 2) here; (z[1] == 12) here; (z[2] == 56) here;
        umbra_interpreter_free(p);
    }
    // shr_u
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_shr_u32, p);
        int x[] = {-1, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == (int)(0xffffffffu >> 1)) here;
        (z[1] == 4) here; (z[2] == 8) here;
        umbra_interpreter_free(p);
    }
    // shr_s
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_shr_s32, p);
        int x[] = {-8, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -4) here; (z[1] == 4) here; (z[2] == 8) here;
        umbra_interpreter_free(p);
    }
    // and, or, xor
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_and_32, p);
        int x[] = {0xff, 0x0f}, y[] = {0x0f, 0xff}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0x0f) here; (z[1] == 0x0f) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_or_32, p);
        int x[] = {0xf0, 0x0f}, y[] = {0x0f, 0xf0}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0xff) here; (z[1] == 0xff) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_xor_32, p);
        int x[] = {0xff, 0xff}, y[] = {0x0f, 0xff}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0xf0) here; (z[1] == 0x00) here;
        umbra_interpreter_free(p);
    }
    // sel
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   c = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   a = umbra_load_32(bb, (umbra_ptr){1}, ix),
                   b = umbra_load_32(bb, (umbra_ptr){2}, ix),
                   r = umbra_sel_32(bb, c, a, b);
        umbra_store_32(bb, (umbra_ptr){3}, ix, r);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        int cond[] = {-1, 0, -1}, va[] = {10, 20, 30}, vb[] = {40, 50, 60}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){cond, va, vb, z});
        (z[0] == 10) here; (z[1] == 50) here; (z[2] == 30) here;
        umbra_interpreter_free(p);
    }
}

static void test_f16_ops(void) {
    // mul
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_mul_f16, p);
        __fp16 x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 10) here; equiv((float)z[1], 18) here; equiv((float)z[2], 28) here;
        umbra_interpreter_free(p);
    }
    // add
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_add_f16, p);
        __fp16 x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 11) here; equiv((float)z[1], 22) here; equiv((float)z[2], 33) here;
        umbra_interpreter_free(p);
    }
    // sub
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_sub_f16, p);
        __fp16 x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 9) here; equiv((float)z[1], 18) here; equiv((float)z[2], 27) here;
        umbra_interpreter_free(p);
    }
    // div
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_div_f16, p);
        __fp16 x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 5) here; equiv((float)z[1], 5) here; equiv((float)z[2], 6) here;
        umbra_interpreter_free(p);
    }
}

static void test_i16_ops(void) {
    // add
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_add_i16, p);
        short x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 11) here; (z[1] == 22) here; (z[2] == 33) here;
        umbra_interpreter_free(p);
    }
    // sub
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_sub_i16, p);
        short x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 9) here; (z[1] == 18) here; (z[2] == 27) here;
        umbra_interpreter_free(p);
    }
    // mul
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_mul_i16, p);
        short x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 10) here; (z[1] == 18) here; (z[2] == 28) here;
        umbra_interpreter_free(p);
    }
    // shl
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_shl_i16, p);
        short x[] = {1,3}, y[] = {4,2}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 16) here; (z[1] == 12) here;
        umbra_interpreter_free(p);
    }
    // shr_u
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_shr_u16, p);
        short x[] = {-1, 64}, y[] = {1, 3}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == (short)(0xffffu >> 1)) here; (z[1] == 8) here;
        umbra_interpreter_free(p);
    }
    // shr_s
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_shr_s16, p);
        short x[] = {-8, 64}, y[] = {1, 3}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == -4) here; (z[1] == 8) here;
        umbra_interpreter_free(p);
    }
    // and, or, xor
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_and_16, p);
        short x[] = {0xff}, y[] = {0x0f}, z[1] = {0};
        umbra_interpreter_run(p, 1, (void*[]){x,y,z});
        (z[0] == 0x0f) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_or_16, p);
        short x[] = {0xf0}, y[] = {0x0f}, z[1] = {0};
        umbra_interpreter_run(p, 1, (void*[]){x,y,z});
        (z[0] == 0xff) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_xor_16, p);
        short x[] = {0xff}, y[] = {0x0f}, z[1] = {0};
        umbra_interpreter_run(p, 1, (void*[]){x,y,z});
        (z[0] == 0xf0) here;
        umbra_interpreter_free(p);
    }
    // sel
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb);
        umbra_v16 c = umbra_load_16(bb, (umbra_ptr){0}, ix),
                  a = umbra_load_16(bb, (umbra_ptr){1}, ix),
                  b = umbra_load_16(bb, (umbra_ptr){2}, ix),
                  r = umbra_sel_16(bb, c, a, b);
        umbra_store_16(bb, (umbra_ptr){3}, ix, r);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        short cond[] = {-1, 0}, va[] = {10, 20}, vb[] = {30, 40}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){cond, va, vb, z});
        (z[0] == 10) here; (z[1] == 40) here;
        umbra_interpreter_free(p);
    }
}

static void test_cmp_i32(void) {
    // eq
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_eq_i32, p);
        int x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        umbra_interpreter_free(p);
    }
    // ne
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_ne_i32, p);
        int x[] = {1,2}, y[] = {1,9}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0) here; (z[1] == -1) here;
        umbra_interpreter_free(p);
    }
    // lt_s, le_s, gt_s, ge_s
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_lt_s32, p);
        int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_le_s32, p);
        int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_gt_s32, p);
        int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_ge_s32, p);
        int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        umbra_interpreter_free(p);
    }
    // lt_u (unsigned: -1 is large)
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_lt_u32, p);
        int x[] = {1, -1}, y[] = {2, 1}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here;
        umbra_interpreter_free(p);
    }
}

static void test_cmp_i16(void) {
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_eq_i16, p);
        short x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_lt_s16, p);
        short x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        umbra_interpreter_free(p);
    }
}

static void test_cmp_f32(void) {
    // eq
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_eq_f32, p);
        float x[] = {1,2,3}, y[] = {1,9,3}; int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        umbra_interpreter_free(p);
    }
    // lt
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_lt_f32, p);
        float x[] = {1,5,3}, y[] = {2,5,1}; int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        umbra_interpreter_free(p);
    }
}

static void test_cmp_f16(void) {
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_eq_f16, p);
        __fp16 x[] = {1,2,3}, y[] = {1,9,3}; short z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_lt_f16, p);
        __fp16 x[] = {1,5,3}, y[] = {2,5,1}; short z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        umbra_interpreter_free(p);
    }
}

static void test_imm(void) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   v = umbra_imm_32(bb, 42);
        umbra_store_32(bb, (umbra_ptr){0}, ix, v);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){z});
        (z[0] == 42) here; (z[1] == 42) here; (z[2] == 42) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb);
        umbra_v16 v = umbra_imm_16(bb, 7);
        umbra_store_16(bb, (umbra_ptr){0}, ix, v);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        short z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){z});
        (z[0] == 7) here; (z[1] == 7) here; (z[2] == 7) here;
        umbra_interpreter_free(p);
    }
}

static void test_fma_f32(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 ix = umbra_lane(bb),
               x = umbra_load_32(bb, (umbra_ptr){0}, ix),
               y = umbra_load_32(bb, (umbra_ptr){1}, ix),
               w = umbra_load_32(bb, (umbra_ptr){2}, ix),
               // add(mul(x,y), w) → fma(x,y,w)
               m = umbra_mul_f32(bb, x, y),
               r = umbra_add_f32(bb, m, w);
    umbra_store_32(bb, (umbra_ptr){3}, ix, r);
    struct umbra_interpreter *p = umbra_interpreter(bb);
    umbra_basic_block_free(bb);
    float a[] = {2,3}, b[] = {4,5}, c[] = {10,20}, z[2] = {0};
    umbra_interpreter_run(p, 2, (void*[]){a,b,c,z});
    equiv(z[0], 18) here; equiv(z[1], 35) here;
    umbra_interpreter_free(p);
}

static void test_fma_f16(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 ix = umbra_lane(bb);
    umbra_v16 x = umbra_load_16(bb, (umbra_ptr){0}, ix),
              y = umbra_load_16(bb, (umbra_ptr){1}, ix),
              w = umbra_load_16(bb, (umbra_ptr){2}, ix),
              m = umbra_mul_f16(bb, x, y),
              r = umbra_add_f16(bb, m, w);
    umbra_store_16(bb, (umbra_ptr){3}, ix, r);
    struct umbra_interpreter *p = umbra_interpreter(bb);
    umbra_basic_block_free(bb);
    __fp16 a[] = {2,3}, b[] = {4,5}, c[] = {10,20}, z[2] = {0};
    umbra_interpreter_run(p, 2, (void*[]){a,b,c,z});
    equiv((float)z[0], 18) here; equiv((float)z[1], 35) here;
    umbra_interpreter_free(p);
}

static void test_min_max_sqrt_f32(void) {
    // min
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_min_f32, p);
        float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 2) here; equiv(z[1], 1) here; equiv(z[2], 3) here;
        umbra_interpreter_free(p);
    }
    // max
    {
        struct umbra_interpreter *p; BB_BINOP_32(umbra_max_f32, p);
        float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 5) here; equiv(z[1], 4) here; equiv(z[2], 3) here;
        umbra_interpreter_free(p);
    }
    // sqrt
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   r = umbra_sqrt_f32(bb, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, r);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        float a[] = {4,9,16}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){a,z});
        equiv(z[0], 2) here; equiv(z[1], 3) here; equiv(z[2], 4) here;
        umbra_interpreter_free(p);
    }
}

static void test_min_max_sqrt_f16(void) {
    // min
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_min_f16, p);
        __fp16 x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 2) here; equiv((float)z[1], 1) here; equiv((float)z[2], 3) here;
        umbra_interpreter_free(p);
    }
    // max
    {
        struct umbra_interpreter *p; BB_BINOP_16(umbra_max_f16, p);
        __fp16 x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 5) here; equiv((float)z[1], 4) here; equiv((float)z[2], 3) here;
        umbra_interpreter_free(p);
    }
    // sqrt
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb);
        umbra_v16 x = umbra_load_16(bb, (umbra_ptr){0}, ix),
                  r = umbra_sqrt_f16(bb, x);
        umbra_store_16(bb, (umbra_ptr){1}, ix, r);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        __fp16 a[] = {4,9,16}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){a,z});
        equiv((float)z[0], 2) here; equiv((float)z[1], 3) here; equiv((float)z[2], 4) here;
        umbra_interpreter_free(p);
    }
}

static void test_large_n(void) {
    struct umbra_interpreter *p; BB_BINOP_32(umbra_add_f32, p);
    float x[11], y[11], z[11];
    for (int i = 0; i < 11; i++) { x[i] = (float)i; y[i] = (float)(10-i); }
    umbra_interpreter_run(p, 11, (void*[]){x,y,z});
    for (int i = 0; i < 11; i++) { equiv(z[i], 10) here; }
    umbra_interpreter_free(p);
}

static void test_convert(void) {
    // f32_from_i32
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   r = umbra_f32_from_i32(bb, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, r);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        int a[] = {1, 255, -3}; float z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){a,z});
        equiv(z[0], 1) here; equiv(z[1], 255) here; equiv(z[2], -3) here;
        umbra_interpreter_free(p);
    }
    // i32_from_f32
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   r = umbra_i32_from_f32(bb, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, r);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        float a[] = {1.9f, 255.0f, -3.7f}; int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){a,z});
        (z[0] == 1) here; (z[1] == 255) here; (z[2] == -3) here;
        umbra_interpreter_free(p);
    }
    // f16↔f32 roundtrip
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix);
        umbra_v16 h = umbra_f16_from_f32(bb, x);
        umbra_v32 r = umbra_f32_from_f16(bb, h);
        umbra_store_32(bb, (umbra_ptr){1}, ix, r);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        float a[] = {1.0f, 0.5f, 100.0f}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){a,z});
        equiv(z[0], 1.0f) here; equiv(z[1], 0.5f) here; equiv(z[2], 100.0f) here;
        umbra_interpreter_free(p);
    }
}

static void test_dedup(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 a = umbra_imm_32(bb, 42),
              b = umbra_imm_32(bb, 42);
    (a.id == b.id) here;
    umbra_v32 c = umbra_imm_32(bb, 99);
    (a.id != c.id) here;
    umbra_v32 ix = umbra_lane(bb),
               x = umbra_load_32(bb, (umbra_ptr){0}, ix),
              s1 = umbra_add_i32(bb, x, a),
              s2 = umbra_add_i32(bb, x, a);
    (s1.id == s2.id) here;
    umbra_basic_block_free(bb);
}

static void test_constprop(void) {
    // add_i32(imm(3), imm(5)) → 8
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   a = umbra_imm_32(bb, 3),
                   b = umbra_imm_32(bb, 5),
                   s = umbra_add_i32(bb, a, b);
        umbra_store_32(bb, (umbra_ptr){0}, ix, s);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){z});
        (z[0] == 8) here; (z[1] == 8) here; (z[2] == 8) here;
        umbra_interpreter_free(p);
    }
    // mul_f32(imm(2.0), imm(3.0)) → 6.0
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb);
        uint32_t two, three;
        { float f = 2.0f; __builtin_memcpy(&two, &f, 4); }
        { float f = 3.0f; __builtin_memcpy(&three, &f, 4); }
        umbra_v32 a = umbra_imm_32(bb, two),
                  b = umbra_imm_32(bb, three),
                  s = umbra_mul_f32(bb, a, b);
        umbra_store_32(bb, (umbra_ptr){0}, ix, s);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        float z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){z});
        equiv(z[0], 6) here; equiv(z[1], 6) here; equiv(z[2], 6) here;
        umbra_interpreter_free(p);
    }
}

static void test_strength_reduction(void) {
    // add(x, 0) → x
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   z = umbra_imm_32(bb, 0),
                   s = umbra_add_i32(bb, x, z);
        (s.id == x.id) here;
        umbra_basic_block_free(bb);
    }
    // mul(x, 1) → x
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix  = umbra_lane(bb),
                   x  = umbra_load_32(bb, (umbra_ptr){0}, ix),
                  one = umbra_imm_32(bb, 1),
                   s  = umbra_mul_i32(bb, x, one);
        (s.id == x.id) here;
        umbra_basic_block_free(bb);
    }
    // mul(x, 8) → shl(x, 3)
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix    = umbra_lane(bb),
                  x     = umbra_load_32(bb, (umbra_ptr){0}, ix),
                  eight = umbra_imm_32(bb, 8),
                  s     = umbra_mul_i32(bb, x, eight);
        umbra_store_32(bb, (umbra_ptr){1}, ix, s);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        int32_t a[] = {1,2,3,4,5}, b[5] = {0};
        umbra_interpreter_run(p, 5, (void*[]){a,b});
        for (int i = 0; i < 5; i++) { (b[i] == a[i] * 8) here; }
        umbra_interpreter_free(p);
    }
    // sub(x, x) → 0
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   s = umbra_sub_i32(bb, x, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, s);
        struct umbra_interpreter *p = umbra_interpreter(bb);
        umbra_basic_block_free(bb);
        int32_t a[] = {1,2,3}, b[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){a,b});
        for (int i = 0; i < 3; i++) { (b[i] == 0) here; }
        umbra_interpreter_free(p);
    }
}

static void test_zero_imm(void) {
    // imm_32(0) should dedup to the zero constant at index 0.
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 zero = umbra_imm_32(bb, 0);
    (zero.id == 0) here;  // deduplicates to the zero constant
    umbra_v32 ix = umbra_lane(bb),
               x = umbra_load_32(bb, (umbra_ptr){0}, ix),
               r = umbra_eq_i32(bb, x, zero);
    umbra_store_32(bb, (umbra_ptr){1}, ix, r);
    struct umbra_interpreter *p = umbra_interpreter(bb);
    umbra_basic_block_free(bb);
    int a[] = {0, 1, 0}, z[3] = {0};
    umbra_interpreter_run(p, 3, (void*[]){a, z});
    (z[0] == -1) here;  // 0 == 0 → true
    (z[1] ==  0) here;  // 1 == 0 → false
    (z[2] == -1) here;  // 0 == 0 → true
    umbra_interpreter_free(p);
}

static void test_srcover(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 ix     = umbra_lane(bb),
              src    = umbra_load_32(bb, (umbra_ptr){0}, ix),
              mask8  = umbra_imm_32(bb, 0xff),
              inv255 = umbra_imm_32(bb, 0x3b808081u), // 1.0f/255.0f
              ri     = umbra_and_32(bb, src, mask8),
              rf     = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ri), inv255);
    umbra_v16 sr = umbra_f16_from_f32(bb, rf);

    umbra_v32 sh8 = umbra_imm_32(bb, 8),
              gi  = umbra_and_32(bb, umbra_shr_u32(bb, src, sh8), mask8),
              gf  = umbra_mul_f32(bb, umbra_f32_from_i32(bb, gi), inv255);
    umbra_v16 sg = umbra_f16_from_f32(bb, gf);

    umbra_v32 sh16 = umbra_imm_32(bb, 16),
              bi   = umbra_and_32(bb, umbra_shr_u32(bb, src, sh16), mask8),
              bf   = umbra_mul_f32(bb, umbra_f32_from_i32(bb, bi), inv255);
    umbra_v16 sb = umbra_f16_from_f32(bb, bf);

    umbra_v32 sh24 = umbra_imm_32(bb, 24),
              ai   = umbra_and_32(bb, umbra_shr_u32(bb, src, sh24), mask8),
              af   = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ai), inv255);
    umbra_v16 sa    = umbra_f16_from_f32(bb, af),
              dr    = umbra_load_16(bb, (umbra_ptr){1}, ix),
              dg    = umbra_load_16(bb, (umbra_ptr){2}, ix),
              db    = umbra_load_16(bb, (umbra_ptr){3}, ix),
              da    = umbra_load_16(bb, (umbra_ptr){4}, ix),
              one   = umbra_imm_16(bb, 0x3c00),
              inv_a = umbra_sub_f16(bb, one, sa),
              rout  = umbra_add_f16(bb, sr, umbra_mul_f16(bb, dr, inv_a)),
              gout  = umbra_add_f16(bb, sg, umbra_mul_f16(bb, dg, inv_a)),
              bout  = umbra_add_f16(bb, sb, umbra_mul_f16(bb, db, inv_a)),
              aout  = umbra_add_f16(bb, sa, umbra_mul_f16(bb, da, inv_a));

    umbra_store_16(bb, (umbra_ptr){1}, ix, rout);
    umbra_store_16(bb, (umbra_ptr){2}, ix, gout);
    umbra_store_16(bb, (umbra_ptr){3}, ix, bout);
    umbra_store_16(bb, (umbra_ptr){4}, ix, aout);

    struct umbra_interpreter *p = umbra_interpreter(bb);
    umbra_basic_block_free(bb);

    uint32_t src_px[] = {0x80402010u, 0x80402010u, 0x80402010u};
    __fp16 dst_r[] = {0.5, 0.5, 0.5},
           dst_g[] = {0.5, 0.5, 0.5},
           dst_b[] = {0.5, 0.5, 0.5},
           dst_a[] = {0.5, 0.5, 0.5};

    umbra_interpreter_run(p, 3, (void*[]){src_px, dst_r, dst_g, dst_b, dst_a});

    float const tol = 0.02f;
    ((float)dst_r[0] > 0.28f - tol && (float)dst_r[0] < 0.34f + tol) here;
    umbra_interpreter_free(p);
}

static void test_codegen(void) {
    // f32 add
    {
        struct umbra_basic_block *bb;
        BB_BUILD_BINOP_32(umbra_add_f32, bb);
        struct umbra_codegen *cg = umbra_codegen(bb);
        umbra_basic_block_free(bb);
        if (!cg) { return; }
        float x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_codegen_run(cg, 3, (void*[]){x,y,z});
        equiv(z[0], 11) here; equiv(z[1], 22) here; equiv(z[2], 33) here;
        umbra_codegen_free(cg);
    }
    // f32 mul
    {
        struct umbra_basic_block *bb;
        BB_BUILD_BINOP_32(umbra_mul_f32, bb);
        struct umbra_codegen *cg = umbra_codegen(bb);
        umbra_basic_block_free(bb);
        if (!cg) { return; }
        float x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_codegen_run(cg, 3, (void*[]){x,y,z});
        equiv(z[0], 10) here; equiv(z[1], 18) here; equiv(z[2], 28) here;
        umbra_codegen_free(cg);
    }
    // i32 add
    {
        struct umbra_basic_block *bb;
        BB_BUILD_BINOP_32(umbra_add_i32, bb);
        struct umbra_codegen *cg = umbra_codegen(bb);
        umbra_basic_block_free(bb);
        if (!cg) { return; }
        int x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_codegen_run(cg, 3, (void*[]){x,y,z});
        (z[0] == 11) here; (z[1] == 22) here; (z[2] == 33) here;
        umbra_codegen_free(cg);
    }
    // i32 shr_u
    {
        struct umbra_basic_block *bb;
        BB_BUILD_BINOP_32(umbra_shr_u32, bb);
        struct umbra_codegen *cg = umbra_codegen(bb);
        umbra_basic_block_free(bb);
        if (!cg) { return; }
        int x[] = {-1, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
        umbra_codegen_run(cg, 3, (void*[]){x,y,z});
        (z[0] == (int)(0xffffffffu >> 1)) here;
        (z[1] == 4) here; (z[2] == 8) here;
        umbra_codegen_free(cg);
    }
    // f16 mul
    {
        struct umbra_basic_block *bb;
        BB_BUILD_BINOP_16(umbra_mul_f16, bb);
        struct umbra_codegen *cg = umbra_codegen(bb);
        umbra_basic_block_free(bb);
        if (!cg) { return; }
        __fp16 x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_codegen_run(cg, 3, (void*[]){x,y,z});
        equiv((float)z[0], 10) here; equiv((float)z[1], 18) here; equiv((float)z[2], 28) here;
        umbra_codegen_free(cg);
    }
    // srcover blend
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix     = umbra_lane(bb),
                  src    = umbra_load_32(bb, (umbra_ptr){0}, ix),
                  mask8  = umbra_imm_32(bb, 0xff),
                  inv255 = umbra_imm_32(bb, 0x3b808081u),
                  ri     = umbra_and_32(bb, src, mask8),
                  rf     = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ri), inv255);
        umbra_v16 sr = umbra_f16_from_f32(bb, rf);
        umbra_v32 sh8 = umbra_imm_32(bb, 8),
                  gi  = umbra_and_32(bb, umbra_shr_u32(bb, src, sh8), mask8),
                  gf  = umbra_mul_f32(bb, umbra_f32_from_i32(bb, gi), inv255);
        umbra_v16 sg = umbra_f16_from_f32(bb, gf);
        umbra_v32 sh16 = umbra_imm_32(bb, 16),
                  bi   = umbra_and_32(bb, umbra_shr_u32(bb, src, sh16), mask8),
                  bf   = umbra_mul_f32(bb, umbra_f32_from_i32(bb, bi), inv255);
        umbra_v16 sb = umbra_f16_from_f32(bb, bf);
        umbra_v32 sh24 = umbra_imm_32(bb, 24),
                  ai   = umbra_and_32(bb, umbra_shr_u32(bb, src, sh24), mask8),
                  af   = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ai), inv255);
        umbra_v16 sa    = umbra_f16_from_f32(bb, af),
                  dr    = umbra_load_16(bb, (umbra_ptr){1}, ix),
                  dg    = umbra_load_16(bb, (umbra_ptr){2}, ix),
                  db    = umbra_load_16(bb, (umbra_ptr){3}, ix),
                  da    = umbra_load_16(bb, (umbra_ptr){4}, ix),
                  one   = umbra_imm_16(bb, 0x3c00),
                  inv_a = umbra_sub_f16(bb, one, sa),
                  rout  = umbra_add_f16(bb, sr, umbra_mul_f16(bb, dr, inv_a)),
                  gout  = umbra_add_f16(bb, sg, umbra_mul_f16(bb, dg, inv_a)),
                  bout  = umbra_add_f16(bb, sb, umbra_mul_f16(bb, db, inv_a)),
                  aout  = umbra_add_f16(bb, sa, umbra_mul_f16(bb, da, inv_a));
        umbra_store_16(bb, (umbra_ptr){1}, ix, rout);
        umbra_store_16(bb, (umbra_ptr){2}, ix, gout);
        umbra_store_16(bb, (umbra_ptr){3}, ix, bout);
        umbra_store_16(bb, (umbra_ptr){4}, ix, aout);
        struct umbra_codegen *cg = umbra_codegen(bb);
        umbra_basic_block_free(bb);
        if (!cg) { return; }
        uint32_t src_px[] = {0x80402010u, 0x80402010u, 0x80402010u};
        __fp16 dst_r[] = {0.5, 0.5, 0.5},
               dst_g[] = {0.5, 0.5, 0.5},
               dst_b[] = {0.5, 0.5, 0.5},
               dst_a[] = {0.5, 0.5, 0.5};
        umbra_codegen_run(cg, 3, (void*[]){src_px, dst_r, dst_g, dst_b, dst_a});
        float const tol = 0.02f;
        ((float)dst_r[0] > 0.28f - tol && (float)dst_r[0] < 0.34f + tol) here;
        umbra_codegen_free(cg);
    }
    // large n
    {
        struct umbra_basic_block *bb;
        BB_BUILD_BINOP_32(umbra_add_f32, bb);
        struct umbra_codegen *cg = umbra_codegen(bb);
        umbra_basic_block_free(bb);
        if (!cg) { return; }
        float x[100], y[100], z[100];
        for (int j = 0; j < 100; j++) { x[j] = (float)j; y[j] = (float)(100-j); }
        umbra_codegen_run(cg, 100, (void*[]){x,y,z});
        for (int j = 0; j < 100; j++) { equiv(z[j], 100) here; }
        umbra_codegen_free(cg);
    }
}

int main(void) {
    test_f32_ops();
    test_i32_ops();
    test_f16_ops();
    test_i16_ops();
    test_cmp_i32();
    test_cmp_i16();
    test_cmp_f32();
    test_cmp_f16();
    test_imm();
    test_fma_f32();
    test_fma_f16();
    test_min_max_sqrt_f32();
    test_min_max_sqrt_f16();
    test_large_n();
    test_convert();
    test_dedup();
    test_constprop();
    test_strength_reduction();
    test_zero_imm();
    test_srcover();
    test_codegen();
    return 0;
}
