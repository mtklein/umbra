#include "../srcover.h"
#include "test.h"
#include <stdint.h>

typedef struct {
    struct umbra_interpreter *interp;
    struct umbra_codegen     *cg;
    struct umbra_jit         *jit;
    struct umbra_metal       *mtl;
} backends;

static void check_backends(backends *B, _Bool expect_cg) {
    (B->interp != 0) here;
#if !defined(__wasm__)
    (!expect_cg || B->cg != 0) here;
#else
    (void)expect_cg;
#endif
#if defined(__aarch64__) || defined(__AVX2__)
    (B->jit != 0) here;
#endif
#if defined(__APPLE__) && !defined(__wasm__)
    (B->mtl != 0) here;
#endif
}
static backends make_full(struct umbra_basic_block *bb, _Bool opt) {
    if (opt) { umbra_basic_block_optimize(bb); }
    backends B = {umbra_interpreter(bb), umbra_codegen(bb), umbra_jit(bb), umbra_metal(bb)};
    umbra_basic_block_free(bb);
    check_backends(&B, 1);
    return B;
}
static backends make(struct umbra_basic_block *bb, _Bool opt) {
    if (opt) { umbra_basic_block_optimize(bb); }
    backends B = {umbra_interpreter(bb), NULL, umbra_jit(bb), umbra_metal(bb)};
    umbra_basic_block_free(bb);
    check_backends(&B, 0);
    return B;
}
static _Bool run(backends *B, int b, int n, umbra_buf buf[]) {
    switch (b) {
    case 0: umbra_interpreter_run(B->interp, n, buf); return 1;
    case 1: if (B->cg)  { umbra_codegen_run(B->cg,  n, buf); return 1; } return 0;
    case 2: if (B->jit) { umbra_jit_run    (B->jit, n, buf); return 1; } return 0;
    case 3: if (B->mtl) { umbra_metal_run  (B->mtl, n, buf); return 1; } return 0;
    }
    return 0;
}
static void cleanup(backends *B) {
    umbra_interpreter_free(B->interp);
    if (B->cg)  umbra_codegen_free(B->cg);
    if (B->jit) umbra_jit_free(B->jit);
    if (B->mtl) umbra_metal_free(B->mtl);
}

#define BINOP_F32(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_i32 ix_ = umbra_lane(bb_); \
    umbra_f32 a_  = umbra_load_f32(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_f32(bb_, (umbra_ptr){1}, ix_), \
              r_  = op(bb_, a_, b_); \
    umbra_store_f32(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

#define BINOP_I32(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_i32 ix_ = umbra_lane(bb_), \
              a_  = umbra_load_i32(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_i32(bb_, (umbra_ptr){1}, ix_), \
              r_  = op(bb_, a_, b_); \
    umbra_store_i32(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

#define BINOP_CMP_F32(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_i32 ix_ = umbra_lane(bb_); \
    umbra_f32 a_  = umbra_load_f32(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_f32(bb_, (umbra_ptr){1}, ix_); \
    umbra_i32 r_  = op(bb_, a_, b_); \
    umbra_store_i32(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

#define BINOP_I16(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_i32 ix_ = umbra_lane(bb_); \
    umbra_i16 a_  = umbra_load_i16(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_i16(bb_, (umbra_ptr){1}, ix_), \
              r_  = op(bb_, a_, b_); \
    umbra_store_i16(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

#define BINOP_HALF(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_i32  ix_ = umbra_lane(bb_); \
    umbra_half a_  = umbra_load_half(bb_, (umbra_ptr){0}, ix_), \
               b_  = umbra_load_half(bb_, (umbra_ptr){1}, ix_), \
               r_  = op(bb_, a_, b_); \
    umbra_store_half(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

#define BINOP_HALF_MASK(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_i32       ix_ = umbra_lane(bb_); \
    umbra_half      ah_ = umbra_load_half(bb_, (umbra_ptr){0}, ix_), \
                    bh_ = umbra_load_half(bb_, (umbra_ptr){1}, ix_); \
    umbra_half_mask a_  = {ah_.id}, b_ = {bh_.id}, \
                    r_  = op(bb_, a_, b_); \
    umbra_store_half(bb_, (umbra_ptr){2}, ix_, (umbra_half){r_.id}); \
    B = make(bb_, opt); \
} while(0)

static void test_f32_ops(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_F32(umbra_mul_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2,3,4,5}, y[] = {6,7,8,9,0}, z[5] = {0};
            if (!run(&B, bi, 5, (umbra_buf[]){{x,5*4},{y,5*4},{z,5*4}})) continue;
            equiv(z[0],  6) here; equiv(z[1], 14) here; equiv(z[2], 24) here;
            equiv(z[3], 36) here; equiv(z[4],  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_add_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            equiv(z[0], 11) here; equiv(z[1], 22) here; equiv(z[2], 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_sub_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            equiv(z[0], 9) here; equiv(z[1], 18) here; equiv(z[2], 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_div_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            equiv(z[0], 5) here; equiv(z[1], 5) here; equiv(z[2], 6) here;
        }
        cleanup(&B);
    }
  }
}

static void test_i32_ops(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_I32(umbra_add_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == 11) here; (z[1] == 22) here; (z[2] == 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_sub_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == 9) here; (z[1] == 18) here; (z[2] == 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_mul_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == 10) here; (z[1] == 18) here; (z[2] == 28) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_shl_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,3,7}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == 2) here; (z[1] == 12) here; (z[2] == 56) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_shr_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {-1, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == (int)(0xffffffffu >> 1)) here;
            (z[1] == 4) here; (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_shr_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {-8, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -4) here; (z[1] == 4) here; (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_and_32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {0xff, 0x0f}, y[] = {0x0f, 0xff}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == 0x0f) here; (z[1] == 0x0f) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_or_32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {0xf0, 0x0f}, y[] = {0x0f, 0xf0}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == 0xff) here; (z[1] == 0xff) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_xor_32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {0xff, 0xff}, y[] = {0x0f, 0xff}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == 0xf0) here; (z[1] == 0x00) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb),
                   c = umbra_load_i32(bb, (umbra_ptr){0}, ix),
                   a = umbra_load_i32(bb, (umbra_ptr){1}, ix),
                   b = umbra_load_i32(bb, (umbra_ptr){2}, ix),
                   r = umbra_sel_i32(bb, c, a, b);
        umbra_store_i32(bb, (umbra_ptr){3}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int cond[] = {-1, 0, -1}, va[] = {10, 20, 30}, vb[] = {40, 50, 60}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{cond,3*4},{va,3*4},{vb,3*4},{z,3*4}})) continue;
            (z[0] == 10) here; (z[1] == 50) here; (z[2] == 30) here;
        }
        cleanup(&B);
    }
  }
}

static void test_half_ops(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_HALF(umbra_mul_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            equiv((float)z[0], 10) here; equiv((float)z[1], 18) here; equiv((float)z[2], 28) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_add_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            equiv((float)z[0], 11) here; equiv((float)z[1], 22) here; equiv((float)z[2], 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_sub_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            equiv((float)z[0], 9) here; equiv((float)z[1], 18) here; equiv((float)z[2], 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_div_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            equiv((float)z[0], 5) here; equiv((float)z[1], 5) here; equiv((float)z[2], 6) here;
        }
        cleanup(&B);
    }
  }
}

static void test_half_bitwise(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_HALF_MASK(umbra_and_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[1], y[1], z[1] = {0};
            uint16_t xb = 0xff00, yb = 0x0ff0;
            __builtin_memcpy(x, &xb, 2);
            __builtin_memcpy(y, &yb, 2);
            if (!run(&B, bi, 1, (umbra_buf[]){{x,1*2},{y,1*2},{z,1*2}})) continue;
            uint16_t zb; __builtin_memcpy(&zb, z, 2);
            (zb == 0x0f00) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF_MASK(umbra_or_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[1], y[1], z[1] = {0};
            uint16_t xb = 0xf000, yb = 0x0f00;
            __builtin_memcpy(x, &xb, 2);
            __builtin_memcpy(y, &yb, 2);
            if (!run(&B, bi, 1, (umbra_buf[]){{x,1*2},{y,1*2},{z,1*2}})) continue;
            uint16_t zb; __builtin_memcpy(&zb, z, 2);
            (zb == 0xff00) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF_MASK(umbra_xor_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[1], y[1], z[1] = {0};
            uint16_t xb = 0xff00, yb = 0x0f00;
            __builtin_memcpy(x, &xb, 2);
            __builtin_memcpy(y, &yb, 2);
            if (!run(&B, bi, 1, (umbra_buf[]){{x,1*2},{y,1*2},{z,1*2}})) continue;
            uint16_t zb; __builtin_memcpy(&zb, z, 2);
            (zb == 0xf000) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32       ix = umbra_lane(bb);
        umbra_half      ch = umbra_load_half(bb, (umbra_ptr){0}, ix);
        umbra_half_mask c  = {ch.id};
        umbra_half      a  = umbra_load_half(bb, (umbra_ptr){1}, ix),
                        b  = umbra_load_half(bb, (umbra_ptr){2}, ix),
                        r  = umbra_sel_half(bb, c, a, b);
        umbra_store_half(bb, (umbra_ptr){3}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 cond[2], va[2], vb[2], z[2] = {0};
            uint16_t cb[] = {0xffff, 0x0000};
            uint16_t ab[] = {0x1234, 0x1234};
            uint16_t bb_[] = {0x5678, 0x5678};
            __builtin_memcpy(cond, cb, 4);
            __builtin_memcpy(va, ab, 4);
            __builtin_memcpy(vb, bb_, 4);
            if (!run(&B, bi, 2, (umbra_buf[]){{cond,2*2},{va,2*2},{vb,2*2},{z,2*2}})) continue;
            uint16_t z0, z1; __builtin_memcpy(&z0, &z[0], 2); __builtin_memcpy(&z1, &z[1], 2);
            (z0 == 0x1234) here; (z1 == 0x5678) here;
        }
        cleanup(&B);
    }
  }
}

static void test_i16_ops(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_I16(umbra_add_i16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == 11) here; (z[1] == 22) here; (z[2] == 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_sub_i16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == 9) here; (z[1] == 18) here; (z[2] == 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_mul_i16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == 10) here; (z[1] == 18) here; (z[2] == 28) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_shl_i16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {1,3}, y[] = {4,2}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*2},{y,2*2},{z,2*2}})) continue;
            (z[0] == 16) here; (z[1] == 12) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_shr_u16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {-1, 64}, y[] = {1, 3}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*2},{y,2*2},{z,2*2}})) continue;
            (z[0] == (short)(0xffffu >> 1)) here; (z[1] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_shr_s16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {-8, 64}, y[] = {1, 3}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*2},{y,2*2},{z,2*2}})) continue;
            (z[0] == -4) here; (z[1] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_and_16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {0xff}, y[] = {0x0f}, z[1] = {0};
            if (!run(&B, bi, 1, (umbra_buf[]){{x,1*2},{y,1*2},{z,1*2}})) continue;
            (z[0] == 0x0f) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_or_16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {0xf0}, y[] = {0x0f}, z[1] = {0};
            if (!run(&B, bi, 1, (umbra_buf[]){{x,1*2},{y,1*2},{z,1*2}})) continue;
            (z[0] == 0xff) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_xor_16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {0xff}, y[] = {0x0f}, z[1] = {0};
            if (!run(&B, bi, 1, (umbra_buf[]){{x,1*2},{y,1*2},{z,1*2}})) continue;
            (z[0] == 0xf0) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_i16 c = umbra_load_i16(bb, (umbra_ptr){0}, ix),
                  a = umbra_load_i16(bb, (umbra_ptr){1}, ix),
                  b = umbra_load_i16(bb, (umbra_ptr){2}, ix),
                  r = umbra_sel_16(bb, c, a, b);
        umbra_store_i16(bb, (umbra_ptr){3}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            short cond[] = {-1, 0}, va[] = {10, 20}, vb[] = {30, 40}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{cond,2*2},{va,2*2},{vb,2*2},{z,2*2}})) continue;
            (z[0] == 10) here; (z[1] == 40) here;
        }
        cleanup(&B);
    }
  }
}

static void test_cmp_i32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_I32(umbra_eq_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ne_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,2}, y[] = {1,9}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == 0) here; (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_lt_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_le_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_gt_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ge_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_lt_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1, -1}, y[] = {2, 1}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_le_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1, 2, -1}, y[] = {2, 2, 1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_gt_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {2, -1, 1}, y[] = {1, 1, 2}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ge_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {2, 2, 1}, y[] = {1, 2, -1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
  }
}

static void test_cmp_i16(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_I16(umbra_eq_i16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_ne_i16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {1,2}, y[] = {1,9}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*2},{y,2*2},{z,2*2}})) continue;
            (z[0] == 0) here; (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_lt_s16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_le_s16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_gt_s16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_ge_s16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_lt_u16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {1, -1}, y[] = {2, 1}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*2},{y,2*2},{z,2*2}})) continue;
            (z[0] == -1) here; (z[1] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_le_u16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {1, 2, -1}, y[] = {2, 2, 1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_gt_u16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {2, -1, 1}, y[] = {1, 1, 2}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I16(umbra_ge_u16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            short x[] = {2, 2, 1}, y[] = {1, 2, -1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
  }
}

static void test_cmp_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_CMP_F32(umbra_eq_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2,3}, y[] = {1,9,3}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_ne_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2}, y[] = {1,9}; int z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == 0) here; (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_lt_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,5,3}, y[] = {2,5,1}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_le_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,5,3}, y[] = {2,5,1}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_gt_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {3,5,1}, y[] = {2,5,3}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_ge_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {3,5,1}, y[] = {2,5,3}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
  }
}

// Test half comparisons by using sel_half to select between known values.
// Comparison masks are intermediate values, not stored directly.
#define CMP_HALF_TEST(cmp_fn, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_i32       ix_   = umbra_lane(bb_); \
    umbra_half      a_    = umbra_load_half(bb_, (umbra_ptr){0}, ix_), \
                    b_    = umbra_load_half(bb_, (umbra_ptr){1}, ix_), \
                    one_  = umbra_imm_half(bb_, 0x3c00), \
                    zero_ = umbra_imm_half(bb_, 0x0000); \
    umbra_half_mask mask_ = cmp_fn(bb_, a_, b_); \
    umbra_half      r_    = umbra_sel_half(bb_, mask_, one_, zero_); \
    umbra_store_half(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

static void test_cmp_half(void) {
  for (int opt = 0; opt < 2; opt++) {
    // sel_half(cmp(a,b), 1.0h, 0.0h) -> check result as uint16_t (0x3c00 or 0x0000)
    #define H_TRUE  0x3c00  /* fp16 1.0 */
    #define H_FALSE 0x0000  /* fp16 0.0 */
    {
        backends B; CMP_HALF_TEST(umbra_eq_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {1,2,3}, y[] = {1,9,3}; uint16_t z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == H_TRUE) here; (z[1] == H_FALSE) here; (z[2] == H_TRUE) here;
        }
        cleanup(&B);
    }
    {
        backends B; CMP_HALF_TEST(umbra_ne_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {1,2}, y[] = {1,9}; uint16_t z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*2},{y,2*2},{z,2*2}})) continue;
            (z[0] == H_FALSE) here; (z[1] == H_TRUE) here;
        }
        cleanup(&B);
    }
    {
        backends B; CMP_HALF_TEST(umbra_lt_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {1,5,3}, y[] = {2,5,1}; uint16_t z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == H_TRUE) here; (z[1] == H_FALSE) here; (z[2] == H_FALSE) here;
        }
        cleanup(&B);
    }
    {
        backends B; CMP_HALF_TEST(umbra_le_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {1,5,3}, y[] = {2,5,1}; uint16_t z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == H_TRUE) here; (z[1] == H_TRUE) here; (z[2] == H_FALSE) here;
        }
        cleanup(&B);
    }
    {
        backends B; CMP_HALF_TEST(umbra_gt_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {3,5,1}, y[] = {2,5,3}; uint16_t z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == H_TRUE) here; (z[1] == H_FALSE) here; (z[2] == H_FALSE) here;
        }
        cleanup(&B);
    }
    {
        backends B; CMP_HALF_TEST(umbra_ge_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {3,5,1}, y[] = {2,5,3}; uint16_t z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            (z[0] == H_TRUE) here; (z[1] == H_TRUE) here; (z[2] == H_FALSE) here;
        }
        cleanup(&B);
    }
    #undef H_TRUE
    #undef H_FALSE
  }
}

static void test_imm(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb),
                   v = umbra_imm_i32(bb, 42);
        umbra_store_i32(bb, (umbra_ptr){0}, ix, v);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{z,3*4}})) continue;
            (z[0] == 42) here; (z[1] == 42) here; (z[2] == 42) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_i16 v = umbra_imm_i16(bb, 7);
        umbra_store_i16(bb, (umbra_ptr){0}, ix, v);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            short z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{z,3*2}})) continue;
            (z[0] == 7) here; (z[1] == 7) here; (z[2] == 7) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32  ix = umbra_lane(bb);
        umbra_half v = umbra_imm_half(bb, 0x4200);  // 3.0 in fp16
        umbra_store_half(bb, (umbra_ptr){0}, ix, v);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{z,3*2}})) continue;
            equiv((float)z[0], 3) here; equiv((float)z[1], 3) here; equiv((float)z[2], 3) here;
        }
        cleanup(&B);
    }
  }
}

static void test_fma_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 ix = umbra_lane(bb);
    umbra_f32 x = umbra_load_f32(bb, (umbra_ptr){0}, ix),
              y = umbra_load_f32(bb, (umbra_ptr){1}, ix),
              w = umbra_load_f32(bb, (umbra_ptr){2}, ix),
              m = umbra_mul_f32(bb, x, y),
              r = umbra_add_f32(bb, m, w);
    umbra_store_f32(bb, (umbra_ptr){3}, ix, r);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        float a[] = {2,3}, c[] = {4,5}, d[] = {10,20}, z[2] = {0};
        if (!run(&B, bi, 2, (umbra_buf[]){{a,2*4},{c,2*4},{d,2*4},{z,2*4}})) continue;
        equiv(z[0], 18) here; equiv(z[1], 35) here;
    }
    cleanup(&B);
  }
}

static void test_fma_half(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32  ix = umbra_lane(bb);
    umbra_half x = umbra_load_half(bb, (umbra_ptr){0}, ix),
               y = umbra_load_half(bb, (umbra_ptr){1}, ix),
               w = umbra_load_half(bb, (umbra_ptr){2}, ix),
               m = umbra_mul_half(bb, x, y),
               r = umbra_add_half(bb, m, w);
    umbra_store_half(bb, (umbra_ptr){3}, ix, r);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        __fp16 a[] = {2,3}, c[] = {4,5}, d[] = {10,20}, z[2] = {0};
        if (!run(&B, bi, 2, (umbra_buf[]){{a,2*2},{c,2*2},{d,2*2},{z,2*2}})) continue;
        equiv((float)z[0], 18) here; equiv((float)z[1], 35) here;
    }
    cleanup(&B);
  }
}

static void test_min_max_sqrt_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_F32(umbra_min_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            equiv(z[0], 2) here; equiv(z[1], 1) here; equiv(z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_max_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            equiv(z[0], 5) here; equiv(z[1], 4) here; equiv(z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_f32 x = umbra_load_f32(bb, (umbra_ptr){0}, ix),
                  r = umbra_sqrt_f32(bb, x);
        umbra_store_f32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            float a[] = {4,9,16}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{a,3*4},{z,3*4}})) continue;
            equiv(z[0], 2) here; equiv(z[1], 3) here; equiv(z[2], 4) here;
        }
        cleanup(&B);
    }
  }
}

static void test_min_max_sqrt_half(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_HALF(umbra_min_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            equiv((float)z[0], 2) here; equiv((float)z[1], 1) here; equiv((float)z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_max_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2},{z,3*2}})) continue;
            equiv((float)z[0], 5) here; equiv((float)z[1], 4) here; equiv((float)z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32  ix = umbra_lane(bb);
        umbra_half x = umbra_load_half(bb, (umbra_ptr){0}, ix),
                   r = umbra_sqrt_half(bb, x);
        umbra_store_half(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 a[] = {4,9,16}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{a,3*2},{z,3*2}})) continue;
            equiv((float)z[0], 2) here; equiv((float)z[1], 3) here; equiv((float)z[2], 4) here;
        }
        cleanup(&B);
    }
  }
}

static void test_large_n(void) {
  for (int opt = 0; opt < 2; opt++) {
    backends B; BINOP_F32(umbra_add_f32, B, opt);
    for (int bi = 0; bi < 4; bi++) {
        float x[100], y[100], z[100];
        for (int i = 0; i < 100; i++) { x[i] = (float)i; y[i] = (float)(100-i); }
        if (!run(&B, bi, 100, (umbra_buf[]){{x,100*4},{y,100*4},{z,100*4}})) continue;
        for (int i = 0; i < 100; i++) { equiv(z[i], 100) here; }
    }
    cleanup(&B);
  }
}

static void test_convert(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb),
                   x = umbra_load_i32(bb, (umbra_ptr){0}, ix);
        umbra_f32 r  = umbra_f32_from_i32(bb, x);
        umbra_store_f32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int a[] = {1, 255, -3}; float z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{a,3*4},{z,3*4}})) continue;
            equiv(z[0], 1) here; equiv(z[1], 255) here; equiv(z[2], -3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_f32 x  = umbra_load_f32(bb, (umbra_ptr){0}, ix);
        umbra_i32 r  = umbra_i32_from_f32(bb, x);
        umbra_store_i32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            float a[] = {1.9f, 255.0f, -3.7f}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{a,3*4},{z,3*4}})) continue;
            (z[0] == 1) here; (z[1] == 255) here; (z[2] == -3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32  ix = umbra_lane(bb);
        umbra_f32  x  = umbra_load_f32(bb, (umbra_ptr){0}, ix);
        umbra_half h  = umbra_half_from_f32(bb, x);
        umbra_f32  r  = umbra_f32_from_half(bb, h);
        umbra_store_f32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            float a[] = {1.0f, 0.5f, 100.0f}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{a,3*4},{z,3*4}})) continue;
            equiv(z[0], 1.0f) here; equiv(z[1], 0.5f) here; equiv(z[2], 100.0f) here;
        }
        cleanup(&B);
    }
  }
}

static void test_convert_half_i32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32  ix = umbra_lane(bb),
                    x = umbra_load_i32(bb, (umbra_ptr){0}, ix);
        umbra_half h = umbra_half_from_i32(bb, x);
        umbra_store_half(bb, (umbra_ptr){1}, ix, h);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int a[] = {0, 1, 100, 255}; __fp16 z[4] = {0};
            if (!run(&B, bi, 4, (umbra_buf[]){{a,4*4},{z,4*2}})) continue;
            equiv((float)z[0],   0) here;
            equiv((float)z[1],   1) here;
            equiv((float)z[2], 100) here;
            equiv((float)z[3], 255) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32  ix = umbra_lane(bb);
        umbra_half x = umbra_load_half(bb, (umbra_ptr){0}, ix);
        umbra_i32  r = umbra_i32_from_half(bb, x);
        umbra_store_i32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 a[] = {0, 1, 100.5, 255}; int z[4] = {0};
            if (!run(&B, bi, 4, (umbra_buf[]){{a,4*2},{z,4*4}})) continue;
            (z[0] ==   0) here;
            (z[1] ==   1) here;
            (z[2] == 100) here;
            (z[3] == 255) here;
        }
        cleanup(&B);
    }
  }
}

static void test_convert_i16_half(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32  ix = umbra_lane(bb);
        umbra_i16  x = umbra_load_i16(bb, (umbra_ptr){0}, ix);
        umbra_half h = umbra_half_from_i16(bb, x);
        umbra_store_half(bb, (umbra_ptr){1}, ix, h);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            short a[] = {0, 1, 100, 255}; __fp16 z[4] = {0};
            if (!run(&B, bi, 4, (umbra_buf[]){{a,4*2},{z,4*2}})) continue;
            equiv((float)z[0],   0) here;
            equiv((float)z[1],   1) here;
            equiv((float)z[2], 100) here;
            equiv((float)z[3], 255) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32  ix = umbra_lane(bb);
        umbra_half x = umbra_load_half(bb, (umbra_ptr){0}, ix);
        umbra_i16  r = umbra_i16_from_half(bb, x);
        umbra_store_i16(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 a[] = {0, 1, 100.5, 255}; short z[4] = {0};
            if (!run(&B, bi, 4, (umbra_buf[]){{a,4*2},{z,4*2}})) continue;
            (z[0] ==   0) here;
            (z[1] ==   1) here;
            (z[2] == 100) here;
            (z[3] == 255) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb),
                   x = umbra_load_i32(bb, (umbra_ptr){0}, ix);
        umbra_i16 r = umbra_i16_from_i32(bb, x);
        umbra_store_i16(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int a[] = {0, 1, 255, 1000}; short z[4] = {0};
            if (!run(&B, bi, 4, (umbra_buf[]){{a,4*4},{z,4*2}})) continue;
            (z[0] ==    0) here;
            (z[1] ==    1) here;
            (z[2] ==  255) here;
            (z[3] == 1000) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_i16 x = umbra_load_i16(bb, (umbra_ptr){0}, ix);
        umbra_i32 r = umbra_i32_from_i16(bb, x);
        umbra_store_i32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            short a[] = {0, 1, -1, 255}; int z[4] = {0};
            if (!run(&B, bi, 4, (umbra_buf[]){{a,4*2},{z,4*4}})) continue;
            (z[0] ==   0) here;
            (z[1] ==   1) here;
            (z[2] ==  -1) here;
            (z[3] == 255) here;
        }
        cleanup(&B);
    }
  }
}

static void test_dedup(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 a = umbra_imm_i32(bb, 42),
              b = umbra_imm_i32(bb, 42);
    (a.id == b.id) here;
    umbra_i32 c = umbra_imm_i32(bb, 99);
    (a.id != c.id) here;
    umbra_i32 ix = umbra_lane(bb),
               x = umbra_load_i32(bb, (umbra_ptr){0}, ix),
              s1 = umbra_add_i32(bb, x, a),
              s2 = umbra_add_i32(bb, x, a);
    (s1.id == s2.id) here;
    umbra_basic_block_free(bb);
}

static void test_constprop(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb),
                   a = umbra_imm_i32(bb, 3),
                   b = umbra_imm_i32(bb, 5),
                   s = umbra_add_i32(bb, a, b);
        umbra_store_i32(bb, (umbra_ptr){0}, ix, s);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{z,3*4}})) continue;
            (z[0] == 8) here; (z[1] == 8) here; (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        uint32_t two, three;
        { float f = 2.0f; __builtin_memcpy(&two, &f, 4); }
        { float f = 3.0f; __builtin_memcpy(&three, &f, 4); }
        umbra_f32 a = umbra_imm_f32(bb, two),
                  b = umbra_imm_f32(bb, three),
                  s = umbra_mul_f32(bb, a, b);
        umbra_store_f32(bb, (umbra_ptr){0}, ix, s);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            float z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{z,3*4}})) continue;
            equiv(z[0], 6) here; equiv(z[1], 6) here; equiv(z[2], 6) here;
        }
        cleanup(&B);
    }
  }
}

static void test_strength_reduction(void) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb),
                   x = umbra_load_i32(bb, (umbra_ptr){0}, ix),
                   z = umbra_imm_i32(bb, 0),
                   s = umbra_add_i32(bb, x, z);
        (s.id == x.id) here;
        umbra_basic_block_free(bb);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix  = umbra_lane(bb),
                   x  = umbra_load_i32(bb, (umbra_ptr){0}, ix),
                  one = umbra_imm_i32(bb, 1),
                   s  = umbra_mul_i32(bb, x, one);
        (s.id == x.id) here;
        umbra_basic_block_free(bb);
    }
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix    = umbra_lane(bb),
                  x     = umbra_load_i32(bb, (umbra_ptr){0}, ix),
                  eight = umbra_imm_i32(bb, 8),
                  s     = umbra_mul_i32(bb, x, eight);
        umbra_store_i32(bb, (umbra_ptr){1}, ix, s);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t a[] = {1,2,3,4,5}, c[5] = {0};
            if (!run(&B, bi, 5, (umbra_buf[]){{a,5*4},{c,5*4}})) continue;
            for (int i = 0; i < 5; i++) { (c[i] == a[i] * 8) here; }
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb),
                   x = umbra_load_i32(bb, (umbra_ptr){0}, ix),
                   s = umbra_sub_i32(bb, x, x);
        umbra_store_i32(bb, (umbra_ptr){1}, ix, s);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t a[] = {1,2,3}, c[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{a,3*4},{c,3*4}})) continue;
            for (int i = 0; i < 3; i++) { (c[i] == 0) here; }
        }
        cleanup(&B);
    }
  }
}

static void test_zero_imm(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 zero = umbra_imm_i32(bb, 0);
    (zero.id == 0) here;
    umbra_i32 ix = umbra_lane(bb),
               x = umbra_load_i32(bb, (umbra_ptr){0}, ix),
               r = umbra_eq_i32(bb, x, zero);
    umbra_store_i32(bb, (umbra_ptr){1}, ix, r);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        int a[] = {0, 1, 0}, z[3] = {0};
        if (!run(&B, bi, 3, (umbra_buf[]){{a,3*4},{z,3*4}})) continue;
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
    }
    cleanup(&B);
  }
}


static void test_load_8x4(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 ix = umbra_lane(bb);
    umbra_i16 ch[4];
    umbra_load_8x4(bb, (umbra_ptr){0}, ix, ch);
    umbra_i16 r = ch[0],
              g = ch[1],
              b = ch[2],
              a = ch[3];
    umbra_store_i16(bb, (umbra_ptr){1}, ix, r);
    umbra_store_i16(bb, (umbra_ptr){2}, ix, g);
    umbra_store_i16(bb, (umbra_ptr){3}, ix, b);
    umbra_store_i16(bb, (umbra_ptr){4}, ix, a);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        uint32_t src[] = {0xAABBCCDD, 0x11223344, 0xFF00FF00};
        int16_t rr[3]={0}, gg[3]={0}, bb_[3]={0}, aa[3]={0};
        if (!run(&B, bi, 3, (umbra_buf[]){{src,3*4},{rr,3*2},{gg,3*2},{bb_,3*2},{aa,3*2}})) continue;
        // RGBA: byte 0=R, 1=G, 2=B, 3=A  (little-endian: 0xAABBCCDD -> R=DD, G=CC, B=BB, A=AA)
        (rr[0] == 0xDD) here; (gg[0] == 0xCC) here; (bb_[0] == 0xBB) here; (aa[0] == 0xAA) here;
        (rr[1] == 0x44) here; (gg[1] == 0x33) here; (bb_[1] == 0x22) here; (aa[1] == 0x11) here;
        (rr[2] == 0x00) here; (gg[2] == 0xFF) here; (bb_[2] == 0x00) here; (aa[2] == 0xFF) here;
    }
    cleanup(&B);
  }
}

static void test_store_8x4(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 ix = umbra_lane(bb);
    // Load 4 channels as i16, convert to u8, store interleaved
    umbra_i16 r = umbra_load_i16(bb, (umbra_ptr){0}, ix),
              g = umbra_load_i16(bb, (umbra_ptr){1}, ix),
              b = umbra_load_i16(bb, (umbra_ptr){2}, ix),
              a = umbra_load_i16(bb, (umbra_ptr){3}, ix);
    umbra_i16 ch[4] = {r, g, b, a};
    umbra_store_8x4(bb, (umbra_ptr){4}, ix, ch);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        int16_t rr[] = {0xDD, 0x44, 0x00};
        int16_t gg[] = {0xCC, 0x33, 0xFF};
        int16_t bb_[]= {0xBB, 0x22, 0x00};
        int16_t aa[] = {0xAA, 0x11, 0xFF};
        uint32_t dst[3] = {0};
        if (!run(&B, bi, 3, (umbra_buf[]){{rr,3*2},{gg,3*2},{bb_,3*2},{aa,3*2},{dst,3*4}})) continue;
        (dst[0] == 0xAABBCCDDu) here;
        (dst[1] == 0x11223344u) here;
        (dst[2] == 0xFF00FF00u) here;
    }
    cleanup(&B);
  }
}

static void test_srcover(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = build_srcover();
    backends B = make_full(bb, opt);
    float const tol = 0.02f;
    for (int bi = 0; bi < 4; bi++) {
        uint32_t src_px[] = {0x80402010u, 0x80402010u, 0x80402010u};
        __fp16 dst_r[] = {0.5, 0.5, 0.5},
               dst_g[] = {0.5, 0.5, 0.5},
               dst_b[] = {0.5, 0.5, 0.5},
               dst_a[] = {0.5, 0.5, 0.5};
        if (!run(&B, bi, 3, (umbra_buf[]){{src_px,3*4},{dst_r,3*2},{dst_g,3*2},{dst_b,3*2},{dst_a,3*2}})) continue;
        ((float)dst_r[0] > 0.28f - tol && (float)dst_r[0] < 0.34f + tol) here;
    }
    cleanup(&B);
  }
}

static void test_hash_quality(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    enum { N = 1000 };
    int ids[N];
    for (int i = 0; i < N; i++) {
        ids[i] = umbra_imm_i32(bb, (uint32_t)i).id;
    }
    for (int i = 0; i < N; i++) {
        (umbra_imm_i32(bb, (uint32_t)i).id == ids[i]) here;
    }
    for (int i = 1; i < N; i++) {
        (ids[i] != ids[i-1]) here;
    }
    umbra_i32 ix = umbra_lane(bb),
               x = umbra_load_i32(bb, (umbra_ptr){0}, ix);
    for (int i = 0; i < N; i++) {
        umbra_i32 c    = umbra_imm_i32(bb, (uint32_t)i);
        umbra_i32 sum  = umbra_add_i32(bb, x, c);
        umbra_i32 sum2 = umbra_add_i32(bb, x, c);
        (sum.id == sum2.id) here;
    }
    umbra_basic_block_free(bb);
}

static void test_mixed_ptr_sizes(void) {
  // Same pointer slot (p0) used as uint32_t in one program, __fp16 in another.
  for (int opt = 0; opt < 2; opt++) {
    // Program 1: p0 is uint32_t*, p1 is uint32_t*
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_i32 a  = umbra_load_i32(bb, (umbra_ptr){0}, ix);
        umbra_i32 b  = umbra_add_i32(bb, a, umbra_imm_i32(bb, 1));
        umbra_store_i32(bb, (umbra_ptr){1}, ix, b);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint32_t x[] = {10, 20, 30}, y[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4}})) continue;
            (y[0] == 11) here; (y[1] == 21) here; (y[2] == 31) here;
        }
        cleanup(&B);
    }
    // Program 2: p0 is __fp16*, p1 is __fp16*
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32  ix = umbra_lane(bb);
        umbra_half a  = umbra_load_half(bb, (umbra_ptr){0}, ix);
        umbra_half b  = umbra_add_half(bb, a, umbra_imm_half(bb, 0x3c00));
        umbra_store_half(bb, (umbra_ptr){1}, ix, b);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {1, 2, 3}, y[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*2},{y,3*2}})) continue;
            equiv((float)y[0], 2) here; equiv((float)y[1], 3) here; equiv((float)y[2], 4) here;
        }
        cleanup(&B);
    }
  }
}

// Regression test: shr_narrow_u32 is (u16)(u32 >> imm), fused from i16_from_i32(shr_u32_imm).
// This op was missing from metal.m's emit_ops, silently producing wrong GPU results.
static void test_shr_narrow_u32(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 ix  = umbra_lane(bb);
    umbra_i32 x   = umbra_load_i32(bb, (umbra_ptr){0}, ix);
    umbra_i32 shr = umbra_shr_u32(bb, x, umbra_imm_i32(bb, 8));
    umbra_i16 r   = umbra_i16_from_i32(bb, shr);
    umbra_store_i16(bb, (umbra_ptr){1}, ix, r);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        // N=9: exercises vector path (8) + scalar tail (1).
        uint32_t a[9]; int16_t z[9];
        for (int i = 0; i < 9; i++) { a[i] = (uint32_t)(i * 256 + 42); z[i] = 0; }
        if (!run(&B, bi, 9, (umbra_buf[]){{a,9*4},{z,9*2}})) continue;
        for (int i = 0; i < 9; i++) {
            ((uint16_t)z[i] == (uint16_t)(a[i] >> 8)) here;
        }
    }
    cleanup(&B);
  }
}

// Regression: half_from_f32 and half_from_i32 under heavy register pressure.
// With 16+ preamble values, ra_alloc(tmp) could evict the just-allocated s.rd,
// spilling stale data.  Fixed by eliminating the tmp and using s.rd in-place.
static void test_half_convert_pressure(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_f32 x  = umbra_load_f32(bb, (umbra_ptr){0}, ix);
        // 16 distinct preamble half constants to exhaust the register file.
        umbra_half h[16];
        for (int i = 0; i < 16; i++) {
            h[i] = umbra_imm_half(bb, (unsigned short)(0x3c00 + i * 0x100));
        }
        // Sum them all to keep them live through the conversion.
        umbra_half sum = h[0];
        for (int i = 1; i < 16; i++) { sum = umbra_add_half(bb, sum, h[i]); }
        // half_from_f32 under pressure: this was the buggy path.
        umbra_half conv = umbra_half_from_f32(bb, x);
        umbra_half res  = umbra_add_half(bb, sum, conv);
        umbra_store_half(bb, (umbra_ptr){1}, ix, res);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            float a[] = {1.0f, 2.0f, 0.0f}; __fp16 z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{a,3*4},{z,3*2}})) { continue; }
            // All backends must agree with the interpreter.
            __fp16 ref[3] = {0};
            umbra_interpreter_run(B.interp, 3, (umbra_buf[]){{a,3*4},{ref,3*2}});
            for (int i = 0; i < 3; i++) { equiv((float)z[i], (float)ref[i]) here; }
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_i32 x  = umbra_load_i32(bb, (umbra_ptr){0}, ix);
        umbra_half h[16];
        for (int i = 0; i < 16; i++) {
            h[i] = umbra_imm_half(bb, (unsigned short)(0x3c00 + i * 0x100));
        }
        umbra_half sum = h[0];
        for (int i = 1; i < 16; i++) { sum = umbra_add_half(bb, sum, h[i]); }
        // half_from_i32 under pressure: this was the other buggy path.
        umbra_half conv = umbra_half_from_i32(bb, x);
        umbra_half res  = umbra_add_half(bb, sum, conv);
        umbra_store_half(bb, (umbra_ptr){1}, ix, res);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int a[] = {1, 2, 0}; __fp16 z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{a,3*4},{z,3*2}})) { continue; }
            __fp16 ref[3] = {0};
            umbra_interpreter_run(B.interp, 3, (umbra_buf[]){{a,3*4},{ref,3*2}});
            for (int i = 0; i < 3; i++) { equiv((float)z[i], (float)ref[i]) here; }
        }
        cleanup(&B);
    }
  }
}

// N=9 tests: K=8, so N=9 exercises one full vector iteration + one scalar tail element.
// Each test runs the same BB through all backends and checks that results match the interpreter.
static void test_n9(void) {
  for (int opt = 0; opt < 2; opt++) {
    // f32: add
    {
        backends B; BINOP_F32(umbra_add_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) { x[i] = (float)(i+1); y[i] = (float)(10*(i+1)); }
            if (!run(&B, bi, 9, (umbra_buf[]){{x,9*4},{y,9*4},{z,9*4}})) continue;
            for (int i = 0; i < 9; i++) equiv(z[i], x[i]+y[i]) here;
        }
        cleanup(&B);
    }
    // i32: mul
    {
        backends B; BINOP_I32(umbra_mul_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) { x[i] = i+1; y[i] = i+2; }
            if (!run(&B, bi, 9, (umbra_buf[]){{x,9*4},{y,9*4},{z,9*4}})) continue;
            for (int i = 0; i < 9; i++) (z[i] == x[i]*y[i]) here;
        }
        cleanup(&B);
    }
    // i16: add
    {
        backends B; BINOP_I16(umbra_add_i16, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int16_t x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) { x[i] = (int16_t)(i+1); y[i] = (int16_t)(100+i); }
            if (!run(&B, bi, 9, (umbra_buf[]){{x,9*2},{y,9*2},{z,9*2}})) continue;
            for (int i = 0; i < 9; i++) (z[i] == (int16_t)(x[i]+y[i])) here;
        }
        cleanup(&B);
    }
    // half: mul
    {
        backends B; BINOP_HALF(umbra_mul_half, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[9], y[9], z[9];
            for (int i = 0; i < 9; i++) { x[i] = (__fp16)(i+1); y[i] = (__fp16)2; z[i] = 0; }
            if (!run(&B, bi, 9, (umbra_buf[]){{x,9*2},{y,9*2},{z,9*2}})) continue;
            for (int i = 0; i < 9; i++) equiv((float)z[i], (float)(2*(i+1))) here;
        }
        cleanup(&B);
    }
    // load_8x4 / store_8x4 at N=9
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_i16 ch[4];
        umbra_load_8x4(bb, (umbra_ptr){0}, ix, ch);
        umbra_i16 out[4] = {ch[0], ch[1], ch[2], ch[3]};
        umbra_store_8x4(bb, (umbra_ptr){1}, ix, out);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint32_t src[9], dst[9] = {0};
            for (int i = 0; i < 9; i++) src[i] = 0xAABBCC00u + (uint32_t)i;
            if (!run(&B, bi, 9, (umbra_buf[]){{src,9*4},{dst,9*4}})) continue;
            for (int i = 0; i < 9; i++) (dst[i] == src[i]) here;
        }
        cleanup(&B);
    }
  }
}

// Regression: paired f32 op with non-paired preamble operand under register pressure.
// The RA could evict the preamble operand and reuse its register for the paired result.
// The lo-half emit reads the operand before writing (correct), but the hi-half emit
// then reads from the same register which now contains the lo result (wrong).
// This bug only manifested with K=8 (ARM64 JIT) and high preamble count.
static void test_preamble_pair_alias(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 ix = umbra_lane(bb);

    // Create many preamble f32 values to exhaust the register file.
    // ARM64 JIT has 20 registers; we need >20 preamble values.
    enum { N_PRE = 24 };
    umbra_f32 pre[N_PRE];
    for (int i = 0; i < N_PRE; i++) {
        // Each value is a different float: 1.0, 2.0, 3.0, ...
        union { float f; unsigned u; } v = { .f = (float)(i + 1) };
        pre[i] = umbra_imm_f32(bb, v.u);
    }

    // Varying: load x from memory (this is the first varying instruction).
    umbra_f32 x = umbra_load_f32(bb, (umbra_ptr){0}, ix);

    // Multiply x by each preamble value and sum the results.
    // This forces the RA to juggle preamble registers during paired f32 ops.
    umbra_f32 sum = umbra_mul_f32(bb, x, pre[0]);
    for (int i = 1; i < N_PRE; i++) {
        sum = umbra_add_f32(bb, sum, umbra_mul_f32(bb, x, pre[i]));
    }

    umbra_store_f32(bb, (umbra_ptr){1}, ix, sum);
    backends B = make(bb, opt);

    for (int bi = 0; bi < 4; bi++) {
        // N=16: two full vector iterations on K=8 JIT.
        float in[16], out[16] = {0};
        for (int i = 0; i < 16; i++) { in[i] = (float)(i + 1); }
        if (!run(&B, bi, 16, (umbra_buf[]){{in, 16*4}, {out, 16*4}})) { continue; }
        // Expected: sum = x * (1 + 2 + ... + 24) = x * 300
        float ref[16];
        umbra_interpreter_run(B.interp, 16, (umbra_buf[]){{in, 16*4}, {ref, 16*4}});
        for (int i = 0; i < 16; i++) { equiv(out[i], ref[i]) here; }
    }
    cleanup(&B);
  }
}

static void test_gather_clamp(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix  = umbra_lane(bb),
                  idx = umbra_load_i32(bb, (umbra_ptr){0}, ix),
                  val = umbra_load_i32(bb, (umbra_ptr){1}, idx);
        umbra_store_i32(bb, (umbra_ptr){2}, ix, val);
        backends B = make_full(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t indices[4] = {-5, 0, 2, 100};
            int32_t src[3]     = {10, 20, 30};
            int32_t dst[4]     = {0};
            if (!run(&B, bi, 4, (umbra_buf[]){
                {indices, (long)sizeof indices},
                {src,     (long)sizeof src},
                {dst,     (long)sizeof dst},
            })) { continue; }
            (dst[0] == 10) here;
            (dst[1] == 10) here;
            (dst[2] == 30) here;
            (dst[3] == 30) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix  = umbra_lane(bb),
                  idx = umbra_load_i32(bb, (umbra_ptr){0}, ix);
        umbra_i16 val = umbra_load_i16(bb, (umbra_ptr){1}, idx);
        umbra_store_i16(bb, (umbra_ptr){2}, ix, val);
        backends B = make_full(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t  indices[4] = {-1, 1, 3, 999};
            int16_t  src[3]     = {100, 200, 300};
            int16_t  dst[4]     = {0};
            if (!run(&B, bi, 4, (umbra_buf[]){
                {indices, (long)sizeof indices},
                {src,     (long)sizeof src},
                {dst,     (long)sizeof dst},
            })) { continue; }
            (dst[0] == 100) here;
            (dst[1] == 200) here;
            (dst[2] == 300) here;
            (dst[3] == 300) here;
        }
        cleanup(&B);
    }
  }
}

static void test_scatter_clamp(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 ix  = umbra_lane(bb),
              idx = umbra_load_i32(bb, (umbra_ptr){0}, ix),
              val = umbra_load_i32(bb, (umbra_ptr){1}, ix);
    umbra_store_i32(bb, (umbra_ptr){2}, idx, val);
    backends B = make_full(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        int32_t indices[3] = {-10, 1, 500};
        int32_t vals[3]    = {11, 22, 33};
        int32_t dst[3]     = {0};
        if (!run(&B, bi, 3, (umbra_buf[]){
            {indices, (long)sizeof indices},
            {vals,    (long)sizeof vals},
            {dst,     (long)sizeof dst},
        })) { continue; }
        (dst[0] == 11) here;
        (dst[1] == 22) here;
        (dst[2] == 33) here;
    }
    cleanup(&B);
  }
}

static void test_offset_load_store(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix  = umbra_lane(bb);
        umbra_i32 off = umbra_load_i32(bb, (umbra_ptr){1}, umbra_imm_i32(bb, 0));
        umbra_i32 ixo = umbra_add_i32(bb, ix, off);
        umbra_i16 val = umbra_load_i16(bb, (umbra_ptr){0}, ixo);
        umbra_store_i16(bb, (umbra_ptr){2}, ix, val);
        backends B = make_full(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int16_t src[16] = {10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25};
            int32_t uni[1]  = {4};
            int16_t dst[8]  = {0};
            if (!run(&B, bi, 8, (umbra_buf[]){
                {src, (long)sizeof src},
                {uni, -(long)sizeof uni},
                {dst, (long)sizeof dst},
            })) { continue; }
            for (int k = 0; k < 8; k++) { (dst[k] == (int16_t)(14 + k)) here; }
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix  = umbra_lane(bb);
        umbra_i32 off = umbra_load_i32(bb, (umbra_ptr){1}, umbra_imm_i32(bb, 0));
        umbra_i32 ixo = umbra_add_i32(bb, ix, off);
        umbra_i32 val = umbra_load_i32(bb, (umbra_ptr){0}, ixo);
        umbra_store_i32(bb, (umbra_ptr){2}, ix, val);
        backends B = make_full(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t src[16] = {100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115};
            int32_t uni[1]  = {3};
            int32_t dst[8]  = {0};
            if (!run(&B, bi, 8, (umbra_buf[]){
                {src, (long)sizeof src},
                {uni, -(long)sizeof uni},
                {dst, (long)sizeof dst},
            })) { continue; }
            for (int k = 0; k < 8; k++) { (dst[k] == 103 + k) here; }
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix  = umbra_lane(bb);
        umbra_i32 off = umbra_load_i32(bb, (umbra_ptr){1}, umbra_imm_i32(bb, 0));
        umbra_i32 ixo = umbra_add_i32(bb, ix, off);
        umbra_i32 val = umbra_load_i32(bb, (umbra_ptr){0}, ix);
        umbra_store_i32(bb, (umbra_ptr){2}, ixo, val);
        backends B = make_full(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t src[8]  = {10,11,12,13,14,15,16,17};
            int32_t uni[1]  = {2};
            int32_t dst[16] = {0};
            if (!run(&B, bi, 8, (umbra_buf[]){
                {src, (long)sizeof src},
                {uni, -(long)sizeof uni},
                {dst, (long)sizeof dst},
            })) { continue; }
            for (int k = 0; k < 8; k++) { (dst[k + 2] == 10 + k) here; }
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix  = umbra_lane(bb);
        umbra_i32 off = umbra_load_i32(bb, (umbra_ptr){1}, umbra_imm_i32(bb, 0));
        umbra_i32 ixo = umbra_add_i32(bb, ix, off);
        umbra_half val = umbra_load_half(bb, (umbra_ptr){0}, ixo);
        umbra_store_half(bb, (umbra_ptr){2}, ix, val);
        backends B = make_full(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint16_t src[16]; for (int k = 0; k < 16; k++) { __fp16 h = (__fp16)(k + 10); __builtin_memcpy(&src[k], &h, 2); }
            int32_t  uni[1]  = {5};
            uint16_t dst[8]  = {0};
            if (!run(&B, bi, 8, (umbra_buf[]){
                {src, (long)sizeof src},
                {uni, -(long)sizeof uni},
                {dst, (long)sizeof dst},
            })) { continue; }
            for (int k = 0; k < 8; k++) {
                __fp16 h; __builtin_memcpy(&h, &dst[k], 2);
                equiv((float)h, (float)(15 + k)) here;
            }
        }
        cleanup(&B);
    }
  }
}

int main(void) {
    test_f32_ops();
    test_i32_ops();
    test_half_ops();
    test_half_bitwise();
    test_i16_ops();
    test_cmp_i32();
    test_cmp_i16();
    test_cmp_f32();
    test_cmp_half();
    test_imm();
    test_fma_f32();
    test_fma_half();
    test_min_max_sqrt_f32();
    test_min_max_sqrt_half();
    test_large_n();
    test_convert();
    test_convert_half_i32();
    test_convert_i16_half();
    test_dedup();
    test_constprop();
    test_strength_reduction();
    test_zero_imm();
    test_load_8x4();
    test_store_8x4();
    test_srcover();
    test_hash_quality();
    test_mixed_ptr_sizes();
    test_shr_narrow_u32();
    test_half_convert_pressure();
    test_n9();
    test_preamble_pair_alias();
    test_gather_clamp();
    test_scatter_clamp();
    test_offset_load_store();
    return 0;
}
