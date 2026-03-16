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
    umbra_f32 a_  = umbra_fload(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_fload(bb_, (umbra_ptr){1}, ix_), \
              r_  = op(bb_, a_, b_); \
    umbra_fstore(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

#define BINOP_I32(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_i32 ix_ = umbra_lane(bb_), \
              a_  = umbra_iload(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_iload(bb_, (umbra_ptr){1}, ix_), \
              r_  = op(bb_, a_, b_); \
    umbra_istore(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

#define BINOP_CMP_F32(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_i32 ix_ = umbra_lane(bb_); \
    umbra_f32 a_  = umbra_fload(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_fload(bb_, (umbra_ptr){1}, ix_); \
    umbra_i32 r_  = op(bb_, a_, b_); \
    umbra_istore(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

static void test_f32_ops(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_F32(umbra_fmul, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2,3,4,5}, y[] = {6,7,8,9,0}, z[5] = {0};
            if (!run(&B, bi, 5, (umbra_buf[]){{x,5*4},{y,5*4},{z,5*4}})) continue;
            equiv(z[0],  6) here; equiv(z[1], 14) here; equiv(z[2], 24) here;
            equiv(z[3], 36) here; equiv(z[4],  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_fadd, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            equiv(z[0], 11) here; equiv(z[1], 22) here; equiv(z[2], 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_fsub, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            equiv(z[0], 9) here; equiv(z[1], 18) here; equiv(z[2], 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_fdiv, B, opt);
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
        backends B; BINOP_I32(umbra_iadd, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == 11) here; (z[1] == 22) here; (z[2] == 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_isub, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == 9) here; (z[1] == 18) here; (z[2] == 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_imul, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == 10) here; (z[1] == 18) here; (z[2] == 28) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ishl, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,3,7}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == 2) here; (z[1] == 12) here; (z[2] == 56) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ushr, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {-1, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == (int)(0xffffffffu >> 1)) here;
            (z[1] == 4) here; (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_sshr, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {-8, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -4) here; (z[1] == 4) here; (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_and, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {0xff, 0x0f}, y[] = {0x0f, 0xff}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == 0x0f) here; (z[1] == 0x0f) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_or, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {0xf0, 0x0f}, y[] = {0x0f, 0xf0}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == 0xff) here; (z[1] == 0xff) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_xor, B, opt);
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
                   c = umbra_iload(bb, (umbra_ptr){0}, ix),
                   a = umbra_iload(bb, (umbra_ptr){1}, ix),
                   b = umbra_iload(bb, (umbra_ptr){2}, ix),
                   r = umbra_isel(bb, c, a, b);
        umbra_istore(bb, (umbra_ptr){3}, ix, r);
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

static void test_cmp_i32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_I32(umbra_ieq, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ine, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,2}, y[] = {1,9}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == 0) here; (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_slt, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_sle, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_sgt, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_sge, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ult, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1, -1}, y[] = {2, 1}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ule, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1, 2, -1}, y[] = {2, 2, 1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ugt, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {2, -1, 1}, y[] = {1, 1, 2}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_uge, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {2, 2, 1}, y[] = {1, 2, -1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
  }
}

static void test_cmp_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_CMP_F32(umbra_feq, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2,3}, y[] = {1,9,3}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_fne, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2}, y[] = {1,9}; int z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){{x,2*4},{y,2*4},{z,2*4}})) continue;
            (z[0] == 0) here; (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_flt, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,5,3}, y[] = {2,5,1}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_fle, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,5,3}, y[] = {2,5,1}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_fgt, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {3,5,1}, y[] = {2,5,3}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_CMP_F32(umbra_fge, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {3,5,1}, y[] = {2,5,3}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
  }
}

static void test_imm(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb),
                   v = umbra_iimm(bb, 42);
        umbra_istore(bb, (umbra_ptr){0}, ix, v);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{z,3*4}})) continue;
            (z[0] == 42) here; (z[1] == 42) here; (z[2] == 42) here;
        }
        cleanup(&B);
    }
  }
}

static void test_fma_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 ix = umbra_lane(bb);
    umbra_f32 x = umbra_fload(bb, (umbra_ptr){0}, ix),
              y = umbra_fload(bb, (umbra_ptr){1}, ix),
              w = umbra_fload(bb, (umbra_ptr){2}, ix),
              m = umbra_fmul(bb, x, y),
              r = umbra_fadd(bb, m, w);
    umbra_fstore(bb, (umbra_ptr){3}, ix, r);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        float a[] = {2,3}, c[] = {4,5}, d[] = {10,20}, z[2] = {0};
        if (!run(&B, bi, 2, (umbra_buf[]){{a,2*4},{c,2*4},{d,2*4},{z,2*4}})) continue;
        equiv(z[0], 18) here; equiv(z[1], 35) here;
    }
    cleanup(&B);
  }
}

static void test_min_max_sqrt_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_F32(umbra_fmin, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4},{z,3*4}})) continue;
            equiv(z[0], 2) here; equiv(z[1], 1) here; equiv(z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_fmax, B, opt);
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
        umbra_f32 x = umbra_fload(bb, (umbra_ptr){0}, ix),
                  r = umbra_fsqrt(bb, x);
        umbra_fstore(bb, (umbra_ptr){1}, ix, r);
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

static void test_large_n(void) {
  for (int opt = 0; opt < 2; opt++) {
    backends B; BINOP_F32(umbra_fadd, B, opt);
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
                   x = umbra_iload(bb, (umbra_ptr){0}, ix);
        umbra_f32 r  = umbra_itof(bb, x);
        umbra_fstore(bb, (umbra_ptr){1}, ix, r);
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
        umbra_f32 x  = umbra_fload(bb, (umbra_ptr){0}, ix);
        umbra_i32 r  = umbra_ftoi(bb, x);
        umbra_istore(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            float a[] = {1.9f, 255.0f, -3.7f}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{a,3*4},{z,3*4}})) continue;
            (z[0] == 1) here; (z[1] == 255) here; (z[2] == -3) here;
        }
        cleanup(&B);
    }
  }
}

static void test_dedup(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 a = umbra_iimm(bb, 42),
              b = umbra_iimm(bb, 42);
    (a.id == b.id) here;
    umbra_i32 c = umbra_iimm(bb, 99);
    (a.id != c.id) here;
    umbra_i32 ix = umbra_lane(bb),
               x = umbra_iload(bb, (umbra_ptr){0}, ix),
              s1 = umbra_iadd(bb, x, a),
              s2 = umbra_iadd(bb, x, a);
    (s1.id == s2.id) here;
    umbra_basic_block_free(bb);
}

static void test_constprop(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb),
                   a = umbra_iimm(bb, 3),
                   b = umbra_iimm(bb, 5),
                   s = umbra_iadd(bb, a, b);
        umbra_istore(bb, (umbra_ptr){0}, ix, s);
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
        umbra_f32 a = umbra_fimm(bb, 2.0f),
                  b = umbra_fimm(bb, 3.0f),
                  s = umbra_fmul(bb, a, b);
        umbra_fstore(bb, (umbra_ptr){0}, ix, s);
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
                   x = umbra_iload(bb, (umbra_ptr){0}, ix),
                   z = umbra_iimm(bb, 0),
                   s = umbra_iadd(bb, x, z);
        (s.id == x.id) here;
        umbra_basic_block_free(bb);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix  = umbra_lane(bb),
                   x  = umbra_iload(bb, (umbra_ptr){0}, ix),
                  one = umbra_iimm(bb, 1),
                   s  = umbra_imul(bb, x, one);
        (s.id == x.id) here;
        umbra_basic_block_free(bb);
    }
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix    = umbra_lane(bb),
                  x     = umbra_iload(bb, (umbra_ptr){0}, ix),
                  eight = umbra_iimm(bb, 8),
                  s     = umbra_imul(bb, x, eight);
        umbra_istore(bb, (umbra_ptr){1}, ix, s);
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
                   x = umbra_iload(bb, (umbra_ptr){0}, ix),
                   s = umbra_isub(bb, x, x);
        umbra_istore(bb, (umbra_ptr){1}, ix, s);
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
    umbra_i32 zero = umbra_iimm(bb, 0);
    (zero.id == 0) here;
    umbra_i32 ix = umbra_lane(bb),
               x = umbra_iload(bb, (umbra_ptr){0}, ix),
               r = umbra_ieq(bb, x, zero);
    umbra_istore(bb, (umbra_ptr){1}, ix, r);
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
    umbra_i32 ch[4];
    umbra_load8x4(bb, (umbra_ptr){0}, ix, ch);
    umbra_i32 r = ch[0],
              g = ch[1],
              b = ch[2],
              a = ch[3];
    umbra_istore(bb, (umbra_ptr){1}, ix, r);
    umbra_istore(bb, (umbra_ptr){2}, ix, g);
    umbra_istore(bb, (umbra_ptr){3}, ix, b);
    umbra_istore(bb, (umbra_ptr){4}, ix, a);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        uint32_t src[] = {0xAABBCCDD, 0x11223344, 0xFF00FF00};
        int32_t rr[3]={0}, gg[3]={0}, bb_[3]={0}, aa[3]={0};
        if (!run(&B, bi, 3, (umbra_buf[]){{src,3*4},{rr,3*4},{gg,3*4},{bb_,3*4},{aa,3*4}})) continue;
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
    umbra_i32 r = umbra_iload(bb, (umbra_ptr){0}, ix),
              g = umbra_iload(bb, (umbra_ptr){1}, ix),
              b = umbra_iload(bb, (umbra_ptr){2}, ix),
              a = umbra_iload(bb, (umbra_ptr){3}, ix);
    umbra_i32 ch[4] = {r, g, b, a};
    umbra_store8x4(bb, (umbra_ptr){4}, ix, ch);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 4; bi++) {
        int32_t rr[] = {0xDD, 0x44, 0x00};
        int32_t gg[] = {0xCC, 0x33, 0xFF};
        int32_t bb_[]= {0xBB, 0x22, 0x00};
        int32_t aa[] = {0xAA, 0x11, 0xFF};
        uint32_t dst[3] = {0};
        if (!run(&B, bi, 3, (umbra_buf[]){{rr,3*4},{gg,3*4},{bb_,3*4},{aa,3*4},{dst,3*4}})) continue;
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
        ids[i] = umbra_iimm(bb, (uint32_t)i).id;
    }
    for (int i = 0; i < N; i++) {
        (umbra_iimm(bb, (uint32_t)i).id == ids[i]) here;
    }
    for (int i = 1; i < N; i++) {
        (ids[i] != ids[i-1]) here;
    }
    umbra_i32 ix = umbra_lane(bb),
               x = umbra_iload(bb, (umbra_ptr){0}, ix);
    for (int i = 0; i < N; i++) {
        umbra_i32 c    = umbra_iimm(bb, (uint32_t)i);
        umbra_i32 sum  = umbra_iadd(bb, x, c);
        umbra_i32 sum2 = umbra_iadd(bb, x, c);
        (sum.id == sum2.id) here;
    }
    umbra_basic_block_free(bb);
}

static void test_mixed_ptr_sizes(void) {
  for (int opt = 0; opt < 2; opt++) {
    // Program 1: p0 is uint32_t*, p1 is uint32_t*
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_i32 a  = umbra_iload(bb, (umbra_ptr){0}, ix);
        umbra_i32 b  = umbra_iadd(bb, a, umbra_iimm(bb, 1));
        umbra_istore(bb, (umbra_ptr){1}, ix, b);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint32_t x[] = {10, 20, 30}, y[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){{x,3*4},{y,3*4}})) continue;
            (y[0] == 11) here; (y[1] == 21) here; (y[2] == 31) here;
        }
        cleanup(&B);
    }
    // Program 2: p0 is __fp16*, p1 is __fp16*  (load_half/store_half still exist)
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_f32 a  = umbra_load_f16(bb, (umbra_ptr){0}, ix);
        umbra_f32 b  = umbra_fadd(bb, a, umbra_fimm(bb, 1.0f));
        umbra_store_f16(bb, (umbra_ptr){1}, ix, b);
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

// N=9 tests: K=8, so N=9 exercises one full vector iteration + one scalar tail element.
static void test_n9(void) {
  for (int opt = 0; opt < 2; opt++) {
    // f32: add
    {
        backends B; BINOP_F32(umbra_fadd, B, opt);
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
        backends B; BINOP_I32(umbra_imul, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) { x[i] = i+1; y[i] = i+2; }
            if (!run(&B, bi, 9, (umbra_buf[]){{x,9*4},{y,9*4},{z,9*4}})) continue;
            for (int i = 0; i < 9; i++) (z[i] == x[i]*y[i]) here;
        }
        cleanup(&B);
    }
    // load_8x4 / store_8x4 at N=9
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix = umbra_lane(bb);
        umbra_i32 ch[4];
        umbra_load8x4(bb, (umbra_ptr){0}, ix, ch);
        umbra_i32 out[4] = {ch[0], ch[1], ch[2], ch[3]};
        umbra_store8x4(bb, (umbra_ptr){1}, ix, out);
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
static void test_preamble_pair_alias(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_i32 ix = umbra_lane(bb);

    enum { N_PRE = 24 };
    umbra_f32 pre[N_PRE];
    for (int i = 0; i < N_PRE; i++) {
        pre[i] = umbra_fimm(bb, (float)(i + 1));
    }

    umbra_f32 x = umbra_fload(bb, (umbra_ptr){0}, ix);

    umbra_f32 sum = umbra_fmul(bb, x, pre[0]);
    for (int i = 1; i < N_PRE; i++) {
        sum = umbra_fadd(bb, sum, umbra_fmul(bb, x, pre[i]));
    }

    umbra_fstore(bb, (umbra_ptr){1}, ix, sum);
    backends B = make(bb, opt);

    for (int bi = 0; bi < 4; bi++) {
        float in[16], out[16] = {0};
        for (int i = 0; i < 16; i++) { in[i] = (float)(i + 1); }
        if (!run(&B, bi, 16, (umbra_buf[]){{in, 16*4}, {out, 16*4}})) { continue; }
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
                  idx = umbra_iload(bb, (umbra_ptr){0}, ix),
                  val = umbra_iload(bb, (umbra_ptr){1}, idx);
        umbra_istore(bb, (umbra_ptr){2}, ix, val);
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
                  idx = umbra_iload(bb, (umbra_ptr){0}, ix);
        umbra_i32 val = umbra_i16load(bb, (umbra_ptr){1}, idx);
        umbra_istore(bb, (umbra_ptr){2}, ix, val);
        backends B = make_full(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t  indices[4] = {-1, 1, 3, 999};
            int16_t  src[3]     = {100, 200, 300};
            int32_t  dst[4]     = {0};
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
              idx = umbra_iload(bb, (umbra_ptr){0}, ix),
              val = umbra_iload(bb, (umbra_ptr){1}, ix);
    umbra_istore(bb, (umbra_ptr){2}, idx, val);
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
        umbra_i32 off = umbra_iload(bb, (umbra_ptr){1}, umbra_iimm(bb, 0));
        umbra_i32 ixo = umbra_iadd(bb, ix, off);
        umbra_i32 val = umbra_i16load(bb, (umbra_ptr){0}, ixo);
        umbra_istore(bb, (umbra_ptr){2}, ix, val);
        backends B = make_full(bb, opt);
        for (int bi = 0; bi < 4; bi++) {
            int16_t src[16] = {10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25};
            int32_t uni[1]  = {4};
            int32_t dst[8]  = {0};
            if (!run(&B, bi, 8, (umbra_buf[]){
                {src, (long)sizeof src},
                {uni, -(long)sizeof uni},
                {dst, (long)sizeof dst},
            })) { continue; }
            for (int k = 0; k < 8; k++) { (dst[k] == 14 + k) here; }
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_i32 ix  = umbra_lane(bb);
        umbra_i32 off = umbra_iload(bb, (umbra_ptr){1}, umbra_iimm(bb, 0));
        umbra_i32 ixo = umbra_iadd(bb, ix, off);
        umbra_i32 val = umbra_iload(bb, (umbra_ptr){0}, ixo);
        umbra_istore(bb, (umbra_ptr){2}, ix, val);
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
        umbra_i32 off = umbra_iload(bb, (umbra_ptr){1}, umbra_iimm(bb, 0));
        umbra_i32 ixo = umbra_iadd(bb, ix, off);
        umbra_i32 val = umbra_iload(bb, (umbra_ptr){0}, ix);
        umbra_istore(bb, (umbra_ptr){2}, ixo, val);
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
        umbra_i32 off = umbra_iload(bb, (umbra_ptr){1}, umbra_iimm(bb, 0));
        umbra_i32 ixo = umbra_iadd(bb, ix, off);
        umbra_f32 val = umbra_load_f16(bb, (umbra_ptr){0}, ixo);
        umbra_store_f16(bb, (umbra_ptr){2}, ix, val);
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
    test_cmp_i32();
    test_cmp_f32();
    test_imm();
    test_fma_f32();
    test_min_max_sqrt_f32();
    test_large_n();
    test_convert();
    test_dedup();
    test_constprop();
    test_strength_reduction();
    test_zero_imm();
    test_load_8x4();
    test_store_8x4();
    test_srcover();
    test_hash_quality();
    test_mixed_ptr_sizes();
    test_n9();
    test_preamble_pair_alias();
    test_gather_clamp();
    test_scatter_clamp();
    test_offset_load_store();
    return 0;
}
