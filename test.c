#include "srcover.h"
#include "test.h"
#include <stdint.h>

typedef struct {
    struct umbra_interpreter *interp;
    struct umbra_codegen     *cg;
    struct umbra_jit         *jit;
} backends;

static backends make_full(struct umbra_basic_block *bb, _Bool opt) {
    if (opt) { umbra_basic_block_optimize(bb); }
    backends B = {umbra_interpreter(bb), umbra_codegen(bb), umbra_jit(bb)};
    umbra_basic_block_free(bb);
    return B;
}
static backends make(struct umbra_basic_block *bb, _Bool opt) {
    if (opt) { umbra_basic_block_optimize(bb); }
    backends B = {umbra_interpreter(bb), NULL, umbra_jit(bb)};
    umbra_basic_block_free(bb);
    return B;
}
static _Bool run(backends *B, int b, int n,
                 void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    switch (b) {
    case 0: umbra_interpreter_run(B->interp, n, p0,p1,p2,p3,p4,p5); return 1;
    case 1: if (B->cg)  { umbra_codegen_run(B->cg,  n, p0,p1,p2,p3,p4,p5); return 1; } return 0;
    case 2: if (B->jit) { umbra_jit_run    (B->jit, n, p0,p1,p2,p3,p4,p5); return 1; } return 0;
    }
    return 0;
}
static void cleanup(backends *B) {
    umbra_interpreter_free(B->interp);
    if (B->cg)  umbra_codegen_free(B->cg);
    if (B->jit) umbra_jit_free(B->jit);
}

#define BINOP_32(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_v32 ix_ = umbra_lane(bb_), \
              a_  = umbra_load_32(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_32(bb_, (umbra_ptr){1}, ix_), \
              r_  = op(bb_, a_, b_); \
    umbra_store_32(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

#define BINOP_16(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_v32 ix_ = umbra_lane(bb_); \
    umbra_v16 a_  = umbra_load_16(bb_, (umbra_ptr){0}, ix_), \
              b_  = umbra_load_16(bb_, (umbra_ptr){1}, ix_), \
              r_  = op(bb_, a_, b_); \
    umbra_store_16(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

#define BINOP_HALF(op, B, opt) do { \
    struct umbra_basic_block *bb_ = umbra_basic_block(); \
    umbra_v32  ix_ = umbra_lane(bb_); \
    umbra_half a_  = umbra_load_half(bb_, (umbra_ptr){0}, ix_), \
               b_  = umbra_load_half(bb_, (umbra_ptr){1}, ix_), \
               r_  = op(bb_, a_, b_); \
    umbra_store_half(bb_, (umbra_ptr){2}, ix_, r_); \
    B = make(bb_, opt); \
} while(0)

static void test_f32_ops(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_32(umbra_mul_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1,2,3,4,5}, y[] = {6,7,8,9,0}, z[5] = {0};
            if (!run(&B, bi,5, x,y,z, 0,0,0)) continue;
            equiv(z[0],  6) here; equiv(z[1], 14) here; equiv(z[2], 24) here;
            equiv(z[3], 36) here; equiv(z[4],  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_add_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv(z[0], 11) here; equiv(z[1], 22) here; equiv(z[2], 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_sub_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv(z[0], 9) here; equiv(z[1], 18) here; equiv(z[2], 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_div_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv(z[0], 5) here; equiv(z[1], 5) here; equiv(z[2], 6) here;
        }
        cleanup(&B);
    }
  }
}

static void test_i32_ops(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_32(umbra_add_i32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == 11) here; (z[1] == 22) here; (z[2] == 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_sub_i32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == 9) here; (z[1] == 18) here; (z[2] == 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_mul_i32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == 10) here; (z[1] == 18) here; (z[2] == 28) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_shl_i32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1,3,7}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == 2) here; (z[1] == 12) here; (z[2] == 56) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_shr_u32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {-1, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == (int)(0xffffffffu >> 1)) here;
            (z[1] == 4) here; (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_shr_s32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {-8, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -4) here; (z[1] == 4) here; (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_and_32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {0xff, 0x0f}, y[] = {0x0f, 0xff}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == 0x0f) here; (z[1] == 0x0f) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_or_32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {0xf0, 0x0f}, y[] = {0x0f, 0xf0}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == 0xff) here; (z[1] == 0xff) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_xor_32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {0xff, 0xff}, y[] = {0x0f, 0xff}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == 0xf0) here; (z[1] == 0x00) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   c = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   a = umbra_load_32(bb, (umbra_ptr){1}, ix),
                   b = umbra_load_32(bb, (umbra_ptr){2}, ix),
                   r = umbra_sel_32(bb, c, a, b);
        umbra_store_32(bb, (umbra_ptr){3}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            int cond[] = {-1, 0, -1}, va[] = {10, 20, 30}, vb[] = {40, 50, 60}, z[3] = {0};
            if (!run(&B, bi,3, cond, va, vb, z, 0,0)) continue;
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
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv((float)z[0], 10) here; equiv((float)z[1], 18) here; equiv((float)z[2], 28) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_add_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv((float)z[0], 11) here; equiv((float)z[1], 22) here; equiv((float)z[2], 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_sub_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv((float)z[0], 9) here; equiv((float)z[1], 18) here; equiv((float)z[2], 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_div_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv((float)z[0], 5) here; equiv((float)z[1], 5) here; equiv((float)z[2], 6) here;
        }
        cleanup(&B);
    }
  }
}

static void test_half_bitwise(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_HALF(umbra_and_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[1], y[1], z[1] = {0};
            uint16_t xb = 0xff00, yb = 0x0ff0;
            __builtin_memcpy(x, &xb, 2);
            __builtin_memcpy(y, &yb, 2);
            if (!run(&B, bi,1, x,y,z, 0,0,0)) continue;
            uint16_t zb; __builtin_memcpy(&zb, z, 2);
            (zb == 0x0f00) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_or_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[1], y[1], z[1] = {0};
            uint16_t xb = 0xf000, yb = 0x0f00;
            __builtin_memcpy(x, &xb, 2);
            __builtin_memcpy(y, &yb, 2);
            if (!run(&B, bi,1, x,y,z, 0,0,0)) continue;
            uint16_t zb; __builtin_memcpy(&zb, z, 2);
            (zb == 0xff00) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_xor_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[1], y[1], z[1] = {0};
            uint16_t xb = 0xff00, yb = 0x0f00;
            __builtin_memcpy(x, &xb, 2);
            __builtin_memcpy(y, &yb, 2);
            if (!run(&B, bi,1, x,y,z, 0,0,0)) continue;
            uint16_t zb; __builtin_memcpy(&zb, z, 2);
            (zb == 0xf000) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32  ix = umbra_lane(bb);
        umbra_half c = umbra_load_half(bb, (umbra_ptr){0}, ix),
                   a = umbra_load_half(bb, (umbra_ptr){1}, ix),
                   b = umbra_load_half(bb, (umbra_ptr){2}, ix),
                   r = umbra_sel_half(bb, c, a, b);
        umbra_store_half(bb, (umbra_ptr){3}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 cond[2], va[2], vb[2], z[2] = {0};
            uint16_t cb[] = {0xffff, 0x0000};
            uint16_t ab[] = {0x1234, 0x1234};
            uint16_t bb_[] = {0x5678, 0x5678};
            __builtin_memcpy(cond, cb, 4);
            __builtin_memcpy(va, ab, 4);
            __builtin_memcpy(vb, bb_, 4);
            if (!run(&B, bi,2, cond, va, vb, z, 0,0)) continue;
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
        backends B; BINOP_16(umbra_add_i16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == 11) here; (z[1] == 22) here; (z[2] == 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_sub_i16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == 9) here; (z[1] == 18) here; (z[2] == 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_mul_i16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == 10) here; (z[1] == 18) here; (z[2] == 28) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_shl_i16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {1,3}, y[] = {4,2}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == 16) here; (z[1] == 12) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_shr_u16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {-1, 64}, y[] = {1, 3}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == (short)(0xffffu >> 1)) here; (z[1] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_shr_s16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {-8, 64}, y[] = {1, 3}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == -4) here; (z[1] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_and_16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {0xff}, y[] = {0x0f}, z[1] = {0};
            if (!run(&B, bi,1, x,y,z, 0,0,0)) continue;
            (z[0] == 0x0f) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_or_16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {0xf0}, y[] = {0x0f}, z[1] = {0};
            if (!run(&B, bi,1, x,y,z, 0,0,0)) continue;
            (z[0] == 0xff) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_xor_16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {0xff}, y[] = {0x0f}, z[1] = {0};
            if (!run(&B, bi,1, x,y,z, 0,0,0)) continue;
            (z[0] == 0xf0) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb);
        umbra_v16 c = umbra_load_16(bb, (umbra_ptr){0}, ix),
                  a = umbra_load_16(bb, (umbra_ptr){1}, ix),
                  b = umbra_load_16(bb, (umbra_ptr){2}, ix),
                  r = umbra_sel_16(bb, c, a, b);
        umbra_store_16(bb, (umbra_ptr){3}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            short cond[] = {-1, 0}, va[] = {10, 20}, vb[] = {30, 40}, z[2] = {0};
            if (!run(&B, bi,2, cond, va, vb, z, 0,0)) continue;
            (z[0] == 10) here; (z[1] == 40) here;
        }
        cleanup(&B);
    }
  }
}

static void test_cmp_i32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_32(umbra_eq_i32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_ne_i32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1,2}, y[] = {1,9}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == 0) here; (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_lt_s32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_le_s32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_gt_s32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_ge_s32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_lt_u32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1, -1}, y[] = {2, 1}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_le_u32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1, 2, -1}, y[] = {2, 2, 1}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_gt_u32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {2, -1, 1}, y[] = {1, 1, 2}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_ge_u32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {2, 2, 1}, y[] = {1, 2, -1}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
  }
}

static void test_cmp_i16(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_16(umbra_eq_i16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_ne_i16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {1,2}, y[] = {1,9}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == 0) here; (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_lt_s16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_le_s16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_gt_s16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_ge_s16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_lt_u16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {1, -1}, y[] = {2, 1}, z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_le_u16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {1, 2, -1}, y[] = {2, 2, 1}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_gt_u16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {2, -1, 1}, y[] = {1, 1, 2}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_16(umbra_ge_u16, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            short x[] = {2, 2, 1}, y[] = {1, 2, -1}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
  }
}

static void test_cmp_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_32(umbra_eq_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1,2,3}, y[] = {1,9,3}; int z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_ne_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1,2}, y[] = {1,9}; int z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == 0) here; (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_lt_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1,5,3}, y[] = {2,5,1}; int z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_le_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1,5,3}, y[] = {2,5,1}; int z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_gt_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {3,5,1}, y[] = {2,5,3}; int z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_ge_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {3,5,1}, y[] = {2,5,3}; int z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
  }
}

static void test_cmp_half(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_HALF(umbra_eq_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {1,2,3}, y[] = {1,9,3}; short z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_ne_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {1,2}, y[] = {1,9}; short z[2] = {0};
            if (!run(&B, bi,2, x,y,z, 0,0,0)) continue;
            (z[0] == 0) here; (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_lt_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {1,5,3}, y[] = {2,5,1}; short z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_le_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {1,5,3}, y[] = {2,5,1}; short z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == -1) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_gt_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {3,5,1}, y[] = {2,5,3}; short z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            (z[0] == -1) here; (z[1] == 0) here; (z[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_ge_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {3,5,1}, y[] = {2,5,3}; short z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
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
        umbra_v32 ix = umbra_lane(bb),
                   v = umbra_imm_32(bb, 42);
        umbra_store_32(bb, (umbra_ptr){0}, ix, v);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            int z[3] = {0};
            if (!run(&B, bi,3, z, 0,0,0,0,0)) continue;
            (z[0] == 42) here; (z[1] == 42) here; (z[2] == 42) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb);
        umbra_v16 v = umbra_imm_16(bb, 7);
        umbra_store_16(bb, (umbra_ptr){0}, ix, v);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            short z[3] = {0};
            if (!run(&B, bi,3, z, 0,0,0,0,0)) continue;
            (z[0] == 7) here; (z[1] == 7) here; (z[2] == 7) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32  ix = umbra_lane(bb);
        umbra_half v = umbra_imm_half(bb, 0x4200);  // 3.0 in fp16
        umbra_store_half(bb, (umbra_ptr){0}, ix, v);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 z[3] = {0};
            if (!run(&B, bi,3, z, 0,0,0,0,0)) continue;
            equiv((float)z[0], 3) here; equiv((float)z[1], 3) here; equiv((float)z[2], 3) here;
        }
        cleanup(&B);
    }
  }
}

static void test_fma_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 ix = umbra_lane(bb),
               x = umbra_load_32(bb, (umbra_ptr){0}, ix),
               y = umbra_load_32(bb, (umbra_ptr){1}, ix),
               w = umbra_load_32(bb, (umbra_ptr){2}, ix),
               m = umbra_mul_f32(bb, x, y),
               r = umbra_add_f32(bb, m, w);
    umbra_store_32(bb, (umbra_ptr){3}, ix, r);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 3; bi++) {
        float a[] = {2,3}, c[] = {4,5}, d[] = {10,20}, z[2] = {0};
        if (!run(&B, bi,2, a,c,d,z, 0,0)) continue;
        equiv(z[0], 18) here; equiv(z[1], 35) here;
    }
    cleanup(&B);
  }
}

static void test_fma_half(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32  ix = umbra_lane(bb);
    umbra_half x = umbra_load_half(bb, (umbra_ptr){0}, ix),
               y = umbra_load_half(bb, (umbra_ptr){1}, ix),
               w = umbra_load_half(bb, (umbra_ptr){2}, ix),
               m = umbra_mul_half(bb, x, y),
               r = umbra_add_half(bb, m, w);
    umbra_store_half(bb, (umbra_ptr){3}, ix, r);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 3; bi++) {
        __fp16 a[] = {2,3}, c[] = {4,5}, d[] = {10,20}, z[2] = {0};
        if (!run(&B, bi,2, a,c,d,z, 0,0)) continue;
        equiv((float)z[0], 18) here; equiv((float)z[1], 35) here;
    }
    cleanup(&B);
  }
}

static void test_min_max_sqrt_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_32(umbra_min_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv(z[0], 2) here; equiv(z[1], 1) here; equiv(z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_32(umbra_max_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv(z[0], 5) here; equiv(z[1], 4) here; equiv(z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   r = umbra_sqrt_f32(bb, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {4,9,16}, z[3] = {0};
            if (!run(&B, bi,3, a,z, 0,0,0,0)) continue;
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
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv((float)z[0], 2) here; equiv((float)z[1], 1) here; equiv((float)z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_HALF(umbra_max_half, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi,3, x,y,z, 0,0,0)) continue;
            equiv((float)z[0], 5) here; equiv((float)z[1], 4) here; equiv((float)z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32  ix = umbra_lane(bb);
        umbra_half x = umbra_load_half(bb, (umbra_ptr){0}, ix),
                   r = umbra_sqrt_half(bb, x);
        umbra_store_half(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 a[] = {4,9,16}, z[3] = {0};
            if (!run(&B, bi,3, a,z, 0,0,0,0)) continue;
            equiv((float)z[0], 2) here; equiv((float)z[1], 3) here; equiv((float)z[2], 4) here;
        }
        cleanup(&B);
    }
  }
}

static void test_large_n(void) {
  for (int opt = 0; opt < 2; opt++) {
    backends B; BINOP_32(umbra_add_f32, B, opt);
    for (int bi = 0; bi < 3; bi++) {
        float x[100], y[100], z[100];
        for (int i = 0; i < 100; i++) { x[i] = (float)i; y[i] = (float)(100-i); }
        if (!run(&B, bi,100, x,y,z, 0,0,0)) continue;
        for (int i = 0; i < 100; i++) { equiv(z[i], 100) here; }
    }
    cleanup(&B);
  }
}

static void test_convert(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   r = umbra_f32_from_i32(bb, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            int a[] = {1, 255, -3}; float z[3] = {0};
            if (!run(&B, bi,3, a,z, 0,0,0,0)) continue;
            equiv(z[0], 1) here; equiv(z[1], 255) here; equiv(z[2], -3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   r = umbra_i32_from_f32(bb, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {1.9f, 255.0f, -3.7f}; int z[3] = {0};
            if (!run(&B, bi,3, a,z, 0,0,0,0)) continue;
            (z[0] == 1) here; (z[1] == 255) here; (z[2] == -3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32  ix = umbra_lane(bb),
                    x = umbra_load_32(bb, (umbra_ptr){0}, ix);
        umbra_half h = umbra_half_from_f32(bb, x);
        umbra_v32  r = umbra_f32_from_half(bb, h);
        umbra_store_32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {1.0f, 0.5f, 100.0f}, z[3] = {0};
            if (!run(&B, bi,3, a,z, 0,0,0,0)) continue;
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
        umbra_v32  ix = umbra_lane(bb),
                    x = umbra_load_32(bb, (umbra_ptr){0}, ix);
        umbra_half h = umbra_half_from_i32(bb, x);
        umbra_store_half(bb, (umbra_ptr){1}, ix, h);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            int a[] = {0, 1, 100, 255}; __fp16 z[4] = {0};
            if (!run(&B, bi,4, a,z, 0,0,0,0)) continue;
            equiv((float)z[0],   0) here;
            equiv((float)z[1],   1) here;
            equiv((float)z[2], 100) here;
            equiv((float)z[3], 255) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32  ix = umbra_lane(bb);
        umbra_half x = umbra_load_half(bb, (umbra_ptr){0}, ix);
        umbra_v32  r = umbra_i32_from_half(bb, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, r);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 a[] = {0, 1, 100.5, 255}; int z[4] = {0};
            if (!run(&B, bi,4, a,z, 0,0,0,0)) continue;
            (z[0] ==   0) here;
            (z[1] ==   1) here;
            (z[2] == 100) here;
            (z[3] == 255) here;
        }
        cleanup(&B);
    }
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
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   a = umbra_imm_32(bb, 3),
                   b = umbra_imm_32(bb, 5),
                   s = umbra_add_i32(bb, a, b);
        umbra_store_32(bb, (umbra_ptr){0}, ix, s);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            int z[3] = {0};
            if (!run(&B, bi,3, z, 0,0,0,0,0)) continue;
            (z[0] == 8) here; (z[1] == 8) here; (z[2] == 8) here;
        }
        cleanup(&B);
    }
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
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            float z[3] = {0};
            if (!run(&B, bi,3, z, 0,0,0,0,0)) continue;
            equiv(z[0], 6) here; equiv(z[1], 6) here; equiv(z[2], 6) here;
        }
        cleanup(&B);
    }
  }
}

static void test_strength_reduction(void) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   z = umbra_imm_32(bb, 0),
                   s = umbra_add_i32(bb, x, z);
        (s.id == x.id) here;
        umbra_basic_block_free(bb);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix  = umbra_lane(bb),
                   x  = umbra_load_32(bb, (umbra_ptr){0}, ix),
                  one = umbra_imm_32(bb, 1),
                   s  = umbra_mul_i32(bb, x, one);
        (s.id == x.id) here;
        umbra_basic_block_free(bb);
    }
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix    = umbra_lane(bb),
                  x     = umbra_load_32(bb, (umbra_ptr){0}, ix),
                  eight = umbra_imm_32(bb, 8),
                  s     = umbra_mul_i32(bb, x, eight);
        umbra_store_32(bb, (umbra_ptr){1}, ix, s);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            int32_t a[] = {1,2,3,4,5}, c[5] = {0};
            if (!run(&B, bi,5, a,c, 0,0,0,0)) continue;
            for (int i = 0; i < 5; i++) { (c[i] == a[i] * 8) here; }
        }
        cleanup(&B);
    }
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb),
                   x = umbra_load_32(bb, (umbra_ptr){0}, ix),
                   s = umbra_sub_i32(bb, x, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, s);
        backends B = make(bb, opt);
        for (int bi = 0; bi < 3; bi++) {
            int32_t a[] = {1,2,3}, c[3] = {0};
            if (!run(&B, bi,3, a,c, 0,0,0,0)) continue;
            for (int i = 0; i < 3; i++) { (c[i] == 0) here; }
        }
        cleanup(&B);
    }
  }
}

static void test_zero_imm(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 zero = umbra_imm_32(bb, 0);
    (zero.id == 0) here;
    umbra_v32 ix = umbra_lane(bb),
               x = umbra_load_32(bb, (umbra_ptr){0}, ix),
               r = umbra_eq_i32(bb, x, zero);
    umbra_store_32(bb, (umbra_ptr){1}, ix, r);
    backends B = make(bb, opt);
    for (int bi = 0; bi < 3; bi++) {
        int a[] = {0, 1, 0}, z[3] = {0};
        if (!run(&B, bi,3, a, z, 0,0,0,0)) continue;
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
    }
    cleanup(&B);
  }
}


static void test_srcover(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_basic_block *bb = build_srcover();
    backends B = make_full(bb, opt);
    float const tol = 0.02f;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t src_px[] = {0x80402010u, 0x80402010u, 0x80402010u};
        __fp16 dst_r[] = {0.5, 0.5, 0.5},
               dst_g[] = {0.5, 0.5, 0.5},
               dst_b[] = {0.5, 0.5, 0.5},
               dst_a[] = {0.5, 0.5, 0.5};
        if (!run(&B, bi,3, src_px, dst_r, dst_g, dst_b, dst_a, 0)) continue;
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
        ids[i] = umbra_imm_32(bb, (uint32_t)i).id;
    }
    for (int i = 0; i < N; i++) {
        (umbra_imm_32(bb, (uint32_t)i).id == ids[i]) here;
    }
    for (int i = 1; i < N; i++) {
        (ids[i] != ids[i-1]) here;
    }
    umbra_v32 ix = umbra_lane(bb),
               x = umbra_load_32(bb, (umbra_ptr){0}, ix);
    for (int i = 0; i < N; i++) {
        umbra_v32 c    = umbra_imm_32(bb, (uint32_t)i);
        umbra_v32 sum  = umbra_add_i32(bb, x, c);
        umbra_v32 sum2 = umbra_add_i32(bb, x, c);
        (sum.id == sum2.id) here;
    }
    umbra_basic_block_free(bb);
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
    test_dedup();
    test_constprop();
    test_strength_reduction();
    test_zero_imm();
    test_srcover();
    test_hash_quality();
    return 0;
}
