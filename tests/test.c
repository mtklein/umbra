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
static backends make_full(struct umbra_builder *builder,
                          _Bool opt) {
    (void)opt;
    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);
    backends B = {
        umbra_interpreter(bb),
        umbra_codegen(bb),
        umbra_jit(bb),
        umbra_metal(bb),
    };
    umbra_basic_block_free(bb);
    check_backends(&B, 1);
    return B;
}
static backends make(struct umbra_builder *builder,
                     _Bool opt) {
    (void)opt;
    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);
    backends B = {
        umbra_interpreter(bb),
        NULL,
        umbra_jit(bb),
        umbra_metal(bb),
    };
    umbra_basic_block_free(bb);
    check_backends(&B, 0);
    return B;
}
static _Bool run(backends *B, int b,
                 int n, umbra_buf buf[]) {
    switch (b) {
    case 0:
        umbra_interpreter_run(B->interp, n, buf);
        return 1;
    case 1:
        if (B->cg) {
            umbra_codegen_run(B->cg, n, buf);
            return 1;
        }
        return 0;
    case 2:
        if (B->jit) {
            umbra_jit_run(B->jit, n, buf);
            return 1;
        }
        return 0;
    case 3:
        if (B->mtl) {
            umbra_metal_run(B->mtl, n, buf);
            return 1;
        }
        return 0;
    }
    return 0;
}
static void cleanup(backends *B) {
    umbra_interpreter_free(B->interp);
    if (B->cg)  umbra_codegen_free(B->cg);
    if (B->jit) umbra_jit_free(B->jit);
    if (B->mtl) umbra_metal_free(B->mtl);
}

#define BINOP_F32(op, B, opt) do {                   \
    struct umbra_builder *b_ =                       \
        umbra_builder();                         \
    umbra_val ix_ = umbra_lane(b_);                 \
    umbra_val x_ = umbra_load_i32(b_,                \
                       (umbra_ptr){0}, ix_),         \
              y_ = umbra_load_i32(b_,                \
                       (umbra_ptr){1}, ix_),         \
              r_ = op(b_, x_, y_);                  \
    umbra_store_i32(b_, (umbra_ptr){2}, ix_, r_);    \
    B = make(b_, opt);                              \
} while(0)

#define BINOP_I32(op, B, opt) do {                   \
    struct umbra_builder *b_ =                       \
        umbra_builder();                         \
    umbra_val ix_ = umbra_lane(b_),                 \
              x_ = umbra_load_i32(b_,                \
                       (umbra_ptr){0}, ix_),         \
              y_ = umbra_load_i32(b_,                \
                       (umbra_ptr){1}, ix_),         \
              r_ = op(b_, x_, y_);                  \
    umbra_store_i32(b_, (umbra_ptr){2}, ix_, r_);    \
    B = make(b_, opt);                              \
} while(0)

#define BINOP_CMP_F32(op, B, opt) do {               \
    struct umbra_builder *b_ =                       \
        umbra_builder();                         \
    umbra_val ix_ = umbra_lane(b_);                 \
    umbra_val x_ = umbra_load_i32(b_,                \
                       (umbra_ptr){0}, ix_),         \
              y_ = umbra_load_i32(b_,                \
                       (umbra_ptr){1}, ix_);         \
    umbra_val r_ = op(b_, x_, y_);                  \
    umbra_store_i32(b_, (umbra_ptr){2}, ix_, r_);    \
    B = make(b_, opt);                              \
} while(0)

static void test_f32_ops(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B; BINOP_F32(umbra_mul_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2,3,4,5};
            float y[] = {6,7,8,9,0}, z[5] = {0};
            if (!run(&B, bi, 5, (umbra_buf[]){
                {x,5*4},{y,5*4},{z,5*4},
            })) { continue; }
            equiv(z[0],  6) here;
            equiv(z[1], 14) here;
            equiv(z[2], 24) here;
            equiv(z[3], 36) here;
            equiv(z[4],  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_add_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2,3};
            float y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            equiv(z[0], 11) here;
            equiv(z[1], 22) here;
            equiv(z[2], 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_sub_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {10,20,30};
            float y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            equiv(z[0], 9) here;
            equiv(z[1], 18) here;
            equiv(z[2], 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_F32(umbra_div_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {10,20,30};
            float y[] = {2,4,5}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            equiv(z[0], 5) here;
            equiv(z[1], 5) here;
            equiv(z[2], 6) here;
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
            int x[] = {1,2,3};
            int y[] = {10,20,30}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == 11) here;
            (z[1] == 22) here;
            (z[2] == 33) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_sub_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {10,20,30};
            int y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == 9) here;
            (z[1] == 18) here;
            (z[2] == 27) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_mul_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {2,3,4};
            int y[] = {5,6,7}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == 10) here;
            (z[1] == 18) here;
            (z[2] == 28) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_shl_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,3,7};
            int y[] = {1,2,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == 2) here;
            (z[1] == 12) here;
            (z[2] == 56) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_shr_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {-1, 8, 64};
            int y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == (int)(0xffffffffu >> 1)) here;
            (z[1] == 4) here;
            (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_shr_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {-8, 8, 64};
            int y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -4) here;
            (z[1] == 4) here;
            (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_and_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {0xff, 0x0f};
            int y[] = {0x0f, 0xff}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){
                {x,2*4},{y,2*4},{z,2*4},
            })) { continue; }
            (z[0] == 0x0f) here;
            (z[1] == 0x0f) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_or_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {0xf0, 0x0f};
            int y[] = {0x0f, 0xf0}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){
                {x,2*4},{y,2*4},{z,2*4},
            })) { continue; }
            (z[0] == 0xff) here;
            (z[1] == 0xff) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_xor_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {0xff, 0xff};
            int y[] = {0x0f, 0xff}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){
                {x,2*4},{y,2*4},{z,2*4},
            })) { continue; }
            (z[0] == 0xf0) here;
            (z[1] == 0x00) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder),
                   c = umbra_load_i32(builder,
                           (umbra_ptr){0}, ix),
                   t = umbra_load_i32(builder,
                           (umbra_ptr){1}, ix),
                   f = umbra_load_i32(builder,
                           (umbra_ptr){2}, ix),
                   r = umbra_sel_i32(builder, c, t, f);
        umbra_store_i32(builder, (umbra_ptr){3}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int cond[] = {-1, 0, -1};
            int va[] = {10, 20, 30};
            int vb[] = {40, 50, 60};
            int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {cond,3*4},{va,3*4},
                {vb,3*4},{z,3*4},
            })) { continue; }
            (z[0] == 10) here;
            (z[1] == 50) here;
            (z[2] == 30) here;
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
            int x[] = {1,2,3};
            int y[] = {1,9,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] ==  0) here;
            (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ne_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,2};
            int y[] = {1,9}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){
                {x,2*4},{y,2*4},{z,2*4},
            })) { continue; }
            (z[0] ==  0) here;
            (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_lt_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,5,3};
            int y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] ==  0) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_le_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1,5,3};
            int y[] = {2,5,1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] == -1) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_gt_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {3,5,1};
            int y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] ==  0) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ge_s32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {3,5,1};
            int y[] = {2,5,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] == -1) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_lt_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1, -1};
            int y[] = {2, 1}, z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){
                {x,2*4},{y,2*4},{z,2*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_le_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {1, 2, -1};
            int y[] = {2, 2, 1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] == -1) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_gt_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {2, -1, 1};
            int y[] = {1, 1, 2}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] == -1) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B; BINOP_I32(umbra_ge_u32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[] = {2, 2, 1};
            int y[] = {1, 2, -1}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] == -1) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
  }
}

static void test_cmp_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B;
        BINOP_CMP_F32(umbra_eq_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2,3};
            float y[] = {1,9,3}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] ==  0) here;
            (z[2] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_CMP_F32(umbra_ne_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,2};
            float y[] = {1,9}; int z[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){
                {x,2*4},{y,2*4},{z,2*4},
            })) { continue; }
            (z[0] ==  0) here;
            (z[1] == -1) here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_CMP_F32(umbra_lt_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,5,3};
            float y[] = {2,5,1}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] ==  0) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_CMP_F32(umbra_le_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {1,5,3};
            float y[] = {2,5,1}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] == -1) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_CMP_F32(umbra_gt_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {3,5,1};
            float y[] = {2,5,3}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] ==  0) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_CMP_F32(umbra_ge_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {3,5,1};
            float y[] = {2,5,3}; int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            (z[0] == -1) here;
            (z[1] == -1) here;
            (z[2] ==  0) here;
        }
        cleanup(&B);
    }
  }
}

static void test_imm(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder),
                   v = umbra_imm_i32(builder, 42);
        umbra_store_i32(builder, (umbra_ptr){0}, ix, v);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {z,3*4},
            })) { continue; }
            (z[0] == 42) here;
            (z[1] == 42) here;
            (z[2] == 42) here;
        }
        cleanup(&B);
    }
  }
}

static void test_fma_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_builder *builder =
        umbra_builder();
    umbra_val ix = umbra_lane(builder);
    umbra_val x = umbra_load_i32(builder,
                      (umbra_ptr){0}, ix),
              y = umbra_load_i32(builder,
                      (umbra_ptr){1}, ix),
              w = umbra_load_i32(builder,
                      (umbra_ptr){2}, ix),
              m = umbra_mul_f32(builder, x, y),
              r = umbra_add_f32(builder, m, w);
    umbra_store_i32(builder, (umbra_ptr){3}, ix, r);
    backends B = make(builder, opt);
    for (int bi = 0; bi < 4; bi++) {
        float a[] = {2,3}, c[] = {4,5};
        float d[] = {10,20}, z[2] = {0};
        if (!run(&B, bi, 2, (umbra_buf[]){
            {a,2*4},{c,2*4},{d,2*4},{z,2*4},
        })) { continue; }
        equiv(z[0], 18) here;
        equiv(z[1], 35) here;
    }
    cleanup(&B);
  }
}

static void test_min_max_sqrt_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B;
        BINOP_F32(umbra_min_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {5,1,3};
            float y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            equiv(z[0], 2) here;
            equiv(z[1], 1) here;
            equiv(z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_F32(umbra_max_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[] = {5,1,3};
            float y[] = {2,4,3}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},{z,3*4},
            })) { continue; }
            equiv(z[0], 5) here;
            equiv(z[1], 4) here;
            equiv(z[2], 3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val x = umbra_load_i32(builder,
                          (umbra_ptr){0}, ix),
                  r = umbra_sqrt_f32(builder, x);
        umbra_store_i32(builder, (umbra_ptr){1}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            float a[] = {4,9,16}, z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {a,3*4},{z,3*4},
            })) { continue; }
            equiv(z[0], 2) here;
            equiv(z[1], 3) here;
            equiv(z[2], 4) here;
        }
        cleanup(&B);
    }
  }
}

static void test_abs_sign_f32(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val x = umbra_load_i32(builder,
                          (umbra_ptr){0}, ix),
                  r = umbra_abs_f32(builder, x);
        umbra_store_i32(builder,
            (umbra_ptr){1}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            float a[] = {-1.5f, 2.5f, -0.0f};
            float z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {a,3*4},{z,3*4},
            })) { continue; }
            equiv(z[0], 1.5f) here;
            equiv(z[1], 2.5f) here;
            equiv(z[2], 0.0f) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val x = umbra_load_i32(builder,
                          (umbra_ptr){0}, ix),
                  r = umbra_sign_f32(builder, x);
        umbra_store_i32(builder,
            (umbra_ptr){1}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            float a[] = {-3.0f, 7.0f, 0.0f};
            float z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {a,3*4},{z,3*4},
            })) { continue; }
            equiv(z[0], -1.0f) here;
            equiv(z[1],  1.0f) here;
            equiv(z[2],  0.0f) here;
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
        for (int i = 0; i < 100; i++) {
            x[i] = (float)i;
            y[i] = (float)(100-i);
        }
        if (!run(&B, bi, 100, (umbra_buf[]){
            {x,100*4},{y,100*4},{z,100*4},
        })) { continue; }
        for (int i = 0; i < 100; i++) {
            equiv(z[i], 100) here;
        }
    }
    cleanup(&B);
  }
}

static void test_convert(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder),
                   x = umbra_load_i32(builder,
                           (umbra_ptr){0}, ix);
        umbra_val r  = umbra_cvt_f32_i32(builder, x);
        umbra_store_i32(builder, (umbra_ptr){1}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int a[] = {1, 255, -3};
            float z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {a,3*4},{z,3*4},
            })) { continue; }
            equiv(z[0], 1) here;
            equiv(z[1], 255) here;
            equiv(z[2], -3) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val x = umbra_load_i32(builder,
                          (umbra_ptr){0}, ix);
        umbra_val r  = umbra_cvt_i32_f32(builder, x);
        umbra_store_i32(builder, (umbra_ptr){1}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            float a[] = {1.9f, 255.0f, -3.7f};
            int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {a,3*4},{z,3*4},
            })) { continue; }
            (z[0] == 1) here;
            (z[1] == 255) here;
            (z[2] == -3) here;
        }
        cleanup(&B);
    }
  }
}

static void test_dedup(void) {
    struct umbra_builder *builder =
        umbra_builder();
    umbra_val v1 = umbra_imm_i32(builder, 42),
              v2 = umbra_imm_i32(builder, 42);
    (v1.id == v2.id) here;
    umbra_val c = umbra_imm_i32(builder, 99);
    (v1.id != c.id) here;
    umbra_val ix = umbra_lane(builder),
               x = umbra_load_i32(builder,
                       (umbra_ptr){0}, ix),
              s1 = umbra_add_i32(builder, x, v1),
              s2 = umbra_add_i32(builder, x, v1);
    (s1.id == s2.id) here;
    umbra_builder_free(builder);
}

static void test_constprop(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder),
                   x = umbra_imm_i32(builder, 3),
                   y = umbra_imm_i32(builder, 5),
                   s = umbra_add_i32(builder, x, y);
        umbra_store_i32(builder, (umbra_ptr){0}, ix, s);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {z,3*4},
            })) { continue; }
            (z[0] == 8) here;
            (z[1] == 8) here;
            (z[2] == 8) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val a = umbra_imm_f32(builder, 2.0f),
                  y = umbra_imm_f32(builder, 3.0f),
                  s = umbra_mul_f32(builder, a, y);
        umbra_store_i32(builder, (umbra_ptr){0}, ix, s);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            float z[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {z,3*4},
            })) { continue; }
            equiv(z[0], 6) here;
            equiv(z[1], 6) here;
            equiv(z[2], 6) here;
        }
        cleanup(&B);
    }
  }
}

static void test_strength_reduction(void) {
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder),
                   x = umbra_load_i32(builder,
                           (umbra_ptr){0}, ix),
                   z = umbra_imm_i32(builder, 0),
                   s = umbra_add_i32(builder, x, z);
        (s.id == x.id) here;
        umbra_builder_free(builder);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix  = umbra_lane(builder),
                   x  = umbra_load_i32(builder,
                            (umbra_ptr){0}, ix),
                  one = umbra_imm_i32(builder, 1),
                   s  = umbra_mul_i32(builder, x, one);
        (s.id == x.id) here;
        umbra_builder_free(builder);
    }
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix    = umbra_lane(builder),
                  x     = umbra_load_i32(builder,
                              (umbra_ptr){0}, ix),
                  eight = umbra_imm_i32(builder, 8),
                  s     = umbra_mul_i32(builder, x, eight);
        umbra_store_i32(builder, (umbra_ptr){1}, ix, s);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t a[] = {1,2,3,4,5}, c[5] = {0};
            if (!run(&B, bi, 5, (umbra_buf[]){
                {a,5*4},{c,5*4},
            })) { continue; }
            for (int i = 0; i < 5; i++) {
                (c[i] == a[i] * 8) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder),
                   x = umbra_load_i32(builder,
                           (umbra_ptr){0}, ix),
                   s = umbra_sub_i32(builder, x, x);
        umbra_store_i32(builder, (umbra_ptr){1}, ix, s);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t a[] = {1,2,3}, c[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {a,3*4},{c,3*4},
            })) { continue; }
            for (int i = 0; i < 3; i++) {
                (c[i] == 0) here;
            }
        }
        cleanup(&B);
    }
  }
}

static void test_zero_imm(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_builder *builder =
        umbra_builder();
    umbra_val zero = umbra_imm_i32(builder, 0);
    (zero.id == 0) here;
    umbra_val ix = umbra_lane(builder),
               x = umbra_load_i32(builder,
                       (umbra_ptr){0}, ix),
               r = umbra_eq_i32(builder, x, zero);
    umbra_store_i32(builder, (umbra_ptr){1}, ix, r);
    backends B = make(builder, opt);
    for (int bi = 0; bi < 4; bi++) {
        int a[] = {0, 1, 0}, z[3] = {0};
        if (!run(&B, bi, 3, (umbra_buf[]){
            {a,3*4},{z,3*4},
        })) { continue; }
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
    }
    cleanup(&B);
  }
}


static void test_load_8x4(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_builder *builder =
        umbra_builder();
    umbra_val ix = umbra_lane(builder);
    umbra_val px_ = umbra_load_i32(builder,
                        (umbra_ptr){0}, ix),
             m_   = umbra_imm_i32(builder, 0xFF);
    umbra_val r = umbra_and_i32(builder, px_, m_),
              g = umbra_and_i32(builder,
                      umbra_shr_u32(builder, px_,
                          umbra_imm_i32(builder, 8)),
                      m_),
              b = umbra_and_i32(builder,
                      umbra_shr_u32(builder, px_,
                          umbra_imm_i32(builder, 16)),
                      m_),
              a = umbra_shr_u32(builder, px_,
                      umbra_imm_i32(builder, 24));
    umbra_store_i32(builder, (umbra_ptr){1}, ix, r);
    umbra_store_i32(builder, (umbra_ptr){2}, ix, g);
    umbra_store_i32(builder, (umbra_ptr){3}, ix, b);
    umbra_store_i32(builder, (umbra_ptr){4}, ix, a);
    backends B = make(builder, opt);
    for (int bi = 0; bi < 4; bi++) {
        uint32_t src[] = {
            0xAABBCCDD, 0x11223344, 0xFF00FF00,
        };
        int32_t rr[3]={0}, gg[3]={0};
        int32_t b_[3]={0}, aa[3]={0};
        if (!run(&B, bi, 3, (umbra_buf[]){
            {src,3*4},{rr,3*4},{gg,3*4},
            {b_,3*4},{aa,3*4},
        })) { continue; }
        (rr[0] == 0xDD) here;
        (gg[0] == 0xCC) here;
        (b_[0] == 0xBB) here;
        (aa[0] == 0xAA) here;
        (rr[1] == 0x44) here;
        (gg[1] == 0x33) here;
        (b_[1] == 0x22) here;
        (aa[1] == 0x11) here;
        (rr[2] == 0x00) here;
        (gg[2] == 0xFF) here;
        (b_[2] == 0x00) here;
        (aa[2] == 0xFF) here;
    }
    cleanup(&B);
  }
}

static void test_store_8x4(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_builder *builder =
        umbra_builder();
    umbra_val ix = umbra_lane(builder);
    umbra_val r = umbra_load_i32(builder,
                      (umbra_ptr){0}, ix),
              g = umbra_load_i32(builder,
                      (umbra_ptr){1}, ix),
              b = umbra_load_i32(builder,
                      (umbra_ptr){2}, ix),
              a = umbra_load_i32(builder,
                      (umbra_ptr){3}, ix);
    umbra_val m_ = umbra_imm_i32(builder, 0xFF);
    umbra_val px_ = umbra_and_i32(builder, r, m_);
    px_ = umbra_or_i32(builder, px_,
        umbra_shl_i32(builder,
            umbra_and_i32(builder, g, m_),
            umbra_imm_i32(builder, 8)));
    px_ = umbra_or_i32(builder, px_,
        umbra_shl_i32(builder,
            umbra_and_i32(builder, b, m_),
            umbra_imm_i32(builder, 16)));
    px_ = umbra_or_i32(builder, px_,
        umbra_shl_i32(builder, a,
            umbra_imm_i32(builder, 24)));
    umbra_store_i32(builder,
        (umbra_ptr){4}, ix, px_);
    backends B = make(builder, opt);
    for (int bi = 0; bi < 4; bi++) {
        int32_t rr[] = {0xDD, 0x44, 0x00};
        int32_t gg[] = {0xCC, 0x33, 0xFF};
        int32_t b_[]= {0xBB, 0x22, 0x00};
        int32_t aa[] = {0xAA, 0x11, 0xFF};
        uint32_t dst[3] = {0};
        if (!run(&B, bi, 3, (umbra_buf[]){
            {rr,3*4},{gg,3*4},{b_,3*4},
            {aa,3*4},{dst,3*4},
        })) { continue; }
        (dst[0] == 0xAABBCCDDu) here;
        (dst[1] == 0x11223344u) here;
        (dst[2] == 0xFF00FF00u) here;
    }
    cleanup(&B);
  }
}

static void test_srcover(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_builder *builder = build_srcover();
    backends B = make_full(builder, opt);
    float const tol = 0;
    for (int bi = 0; bi < 4; bi++) {
        uint32_t src_px[] = {
            0x80402010u, 0x80402010u, 0x80402010u,
        };
        __fp16 dst_r[] = {0.5, 0.5, 0.5},
               dst_g[] = {0.5, 0.5, 0.5},
               dst_b[] = {0.5, 0.5, 0.5},
               dst_a[] = {0.5, 0.5, 0.5};
        if (!run(&B, bi, 3, (umbra_buf[]){
            {src_px,3*4},
            {dst_r,3*2},{dst_g,3*2},
            {dst_b,3*2},{dst_a,3*2},
        })) { continue; }
        ((float)dst_r[0] > 0.28f - tol
            && (float)dst_r[0] < 0.34f + tol) here;
    }
    cleanup(&B);
  }
}

static void test_hash_quality(void) {
    struct umbra_builder *builder =
        umbra_builder();
    enum { N = 1000 };
    int ids[N];
    for (int i = 0; i < N; i++) {
        ids[i] = umbra_imm_i32(builder, i).id;
    }
    for (int i = 0; i < N; i++) {
        (umbra_imm_i32(builder, i).id == ids[i]) here;
    }
    for (int i = 1; i < N; i++) {
        (ids[i] != ids[i-1]) here;
    }
    umbra_val ix = umbra_lane(builder),
               x = umbra_load_i32(builder,
                       (umbra_ptr){0}, ix);
    for (int i = 0; i < N; i++) {
        umbra_val c    = umbra_imm_i32(builder, i);
        umbra_val sum  = umbra_add_i32(builder, x, c);
        umbra_val sum2 = umbra_add_i32(builder, x, c);
        (sum.id == sum2.id) here;
    }
    umbra_builder_free(builder);
}

static void test_mixed_ptr_sizes(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val a = umbra_load_i32(builder,
                          (umbra_ptr){0}, ix);
        umbra_val r = umbra_add_i32(builder, a,
                          umbra_imm_i32(builder, 1));
        umbra_store_i32(builder, (umbra_ptr){1}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint32_t x[] = {10, 20, 30};
            uint32_t y[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*4},{y,3*4},
            })) { continue; }
            (y[0] == 11) here;
            (y[1] == 21) here;
            (y[2] == 31) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val a = umbra_widen_f16(builder,
                          umbra_load_i16(builder,
                              (umbra_ptr){0}, ix));
        umbra_val r = umbra_add_f32(builder, a,
                          umbra_imm_f32(builder,
                                        1.0f));
        umbra_store_i16(builder, (umbra_ptr){1}, ix,
                        umbra_narrow_f32(builder, r));
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            __fp16 x[] = {1, 2, 3}, y[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {x,3*2},{y,3*2},
            })) { continue; }
            equiv((float)y[0], 2) here;
            equiv((float)y[1], 3) here;
            equiv((float)y[2], 4) here;
        }
        cleanup(&B);
    }
  }
}

static void test_n9(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        backends B;
        BINOP_F32(umbra_add_f32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            float x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) {
                x[i] = (float)(i+1);
                y[i] = (float)(10*(i+1));
            }
            if (!run(&B, bi, 9, (umbra_buf[]){
                {x,9*4},{y,9*4},{z,9*4},
            })) { continue; }
            for (int i = 0; i < 9; i++) {
                equiv(z[i], x[i]+y[i]) here;
            }
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_mul_i32, B, opt);
        for (int bi = 0; bi < 4; bi++) {
            int x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) {
                x[i] = i+1; y[i] = i+2;
            }
            if (!run(&B, bi, 9, (umbra_buf[]){
                {x,9*4},{y,9*4},{z,9*4},
            })) { continue; }
            for (int i = 0; i < 9; i++) {
                (z[i] == x[i]*y[i]) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val px_ = umbra_load_i32(builder,
                            (umbra_ptr){0}, ix),
                 m_   = umbra_imm_i32(builder, 0xFF);
        umbra_val ch[4] = {
            umbra_and_i32(builder, px_, m_),
            umbra_and_i32(builder,
                umbra_shr_u32(builder, px_,
                    umbra_imm_i32(builder, 8)),
                m_),
            umbra_and_i32(builder,
                umbra_shr_u32(builder, px_,
                    umbra_imm_i32(builder, 16)),
                m_),
            umbra_shr_u32(builder, px_,
                umbra_imm_i32(builder, 24)),
        };
        umbra_val spx = umbra_and_i32(builder,
                            ch[0], m_);
        spx = umbra_or_i32(builder, spx,
            umbra_shl_i32(builder,
                umbra_and_i32(builder, ch[1], m_),
                umbra_imm_i32(builder, 8)));
        spx = umbra_or_i32(builder, spx,
            umbra_shl_i32(builder,
                umbra_and_i32(builder, ch[2], m_),
                umbra_imm_i32(builder, 16)));
        spx = umbra_or_i32(builder, spx,
            umbra_shl_i32(builder, ch[3],
                umbra_imm_i32(builder, 24)));
        umbra_store_i32(builder,
            (umbra_ptr){1}, ix, spx);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint32_t src[9], dst[9] = {0};
            for (int i = 0; i < 9; i++) {
                src[i] = 0xAABBCC00u + (uint32_t)i;
            }
            if (!run(&B, bi, 9, (umbra_buf[]){
                {src,9*4},{dst,9*4},
            })) { continue; }
            for (int i = 0; i < 9; i++) {
                (dst[i] == src[i]) here;
            }
        }
        cleanup(&B);
    }
  }
}

static void test_preamble_pair_alias(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_builder *builder =
        umbra_builder();
    umbra_val ix = umbra_lane(builder);

    enum { N_PRE = 24 };
    umbra_val pre[N_PRE];
    for (int i = 0; i < N_PRE; i++) {
        float fv_ = (float)(i + 1);
        pre[i] = umbra_imm_i32(builder,
            ((union { float f; int i; }){.f=fv_}).i);
    }

    umbra_val x = umbra_load_i32(builder,
                      (umbra_ptr){0}, ix);

    umbra_val sum = umbra_mul_f32(builder, x, pre[0]);
    for (int i = 1; i < N_PRE; i++) {
        sum = umbra_add_f32(builder, sum,
                  umbra_mul_f32(builder, x, pre[i]));
    }

    umbra_store_i32(builder, (umbra_ptr){1}, ix, sum);
    backends B = make(builder, opt);

    for (int bi = 0; bi < 4; bi++) {
        float in[16], out[16] = {0};
        for (int i = 0; i < 16; i++) {
            in[i] = (float)(i + 1);
        }
        if (!run(&B, bi, 16, (umbra_buf[]){
            {in, 16*4}, {out, 16*4},
        })) { continue; }
        float ref[16];
        umbra_interpreter_run(B.interp, 16,
            (umbra_buf[]){
                {in, 16*4}, {ref, 16*4},
            });
        for (int i = 0; i < 16; i++) {
            equiv(out[i], ref[i]) here;
        }
    }
    cleanup(&B);
  }
}

static void test_gather_clamp(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix  = umbra_lane(builder),
                  idx = umbra_load_i32(builder,
                            (umbra_ptr){0}, ix),
                  val = umbra_load_i32(builder,
                            (umbra_ptr){1}, idx);
        umbra_store_i32(builder, (umbra_ptr){2}, ix, val);
        backends B = make_full(builder, opt);
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
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix  = umbra_lane(builder),
                  idx = umbra_load_i32(builder,
                            (umbra_ptr){0}, ix);
        umbra_val val = umbra_widen_s16(builder,
                            umbra_load_i16(builder,
                                (umbra_ptr){1}, idx));
        umbra_store_i32(builder, (umbra_ptr){2}, ix, val);
        backends B = make_full(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t indices[4] = {-1, 1, 3, 999};
            int16_t src[3]     = {100, 200, 300};
            int32_t dst[4]     = {0};
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
    struct umbra_builder *builder =
        umbra_builder();
    umbra_val ix  = umbra_lane(builder),
              idx = umbra_load_i32(builder,
                        (umbra_ptr){0}, ix),
              val = umbra_load_i32(builder,
                        (umbra_ptr){1}, ix);
    umbra_store_i32(builder, (umbra_ptr){2}, idx, val);
    backends B = make_full(builder, opt);
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
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix  = umbra_lane(builder);
        umbra_val off = umbra_load_i32(builder,
            (umbra_ptr){1}, umbra_imm_i32(builder, 0));
        umbra_val ixo = umbra_add_i32(builder, ix, off);
        umbra_val val = umbra_widen_s16(builder,
            umbra_load_i16(builder,
                (umbra_ptr){0}, ixo));
        umbra_store_i32(builder, (umbra_ptr){2}, ix, val);
        backends B = make_full(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int16_t src[16] = {
                10,11,12,13,14,15,16,17,
                18,19,20,21,22,23,24,25,
            };
            int32_t uni[1]  = {4};
            int32_t dst[8]  = {0};
            if (!run(&B, bi, 8, (umbra_buf[]){
                {src, (long)sizeof src},
                {uni, -(long)sizeof uni},
                {dst, (long)sizeof dst},
            })) { continue; }
            for (int k = 0; k < 8; k++) {
                (dst[k] == 14 + k) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix  = umbra_lane(builder);
        umbra_val off = umbra_load_i32(builder,
            (umbra_ptr){1}, umbra_imm_i32(builder, 0));
        umbra_val ixo = umbra_add_i32(builder, ix, off);
        umbra_val val = umbra_load_i32(builder,
            (umbra_ptr){0}, ixo);
        umbra_store_i32(builder, (umbra_ptr){2}, ix, val);
        backends B = make_full(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t src[16] = {
                100,101,102,103,
                104,105,106,107,
                108,109,110,111,
                112,113,114,115,
            };
            int32_t uni[1]  = {3};
            int32_t dst[8]  = {0};
            if (!run(&B, bi, 8, (umbra_buf[]){
                {src, (long)sizeof src},
                {uni, -(long)sizeof uni},
                {dst, (long)sizeof dst},
            })) { continue; }
            for (int k = 0; k < 8; k++) {
                (dst[k] == 103 + k) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix  = umbra_lane(builder);
        umbra_val off = umbra_load_i32(builder,
            (umbra_ptr){1}, umbra_imm_i32(builder, 0));
        umbra_val ixo = umbra_add_i32(builder, ix, off);
        umbra_val val = umbra_load_i32(builder,
            (umbra_ptr){0}, ix);
        umbra_store_i32(builder,
            (umbra_ptr){2}, ixo, val);
        backends B = make_full(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            int32_t src[8]  = {
                10,11,12,13,14,15,16,17,
            };
            int32_t uni[1]  = {2};
            int32_t dst[16] = {0};
            if (!run(&B, bi, 8, (umbra_buf[]){
                {src, (long)sizeof src},
                {uni, -(long)sizeof uni},
                {dst, (long)sizeof dst},
            })) { continue; }
            for (int k = 0; k < 8; k++) {
                (dst[k + 2] == 10 + k) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix  = umbra_lane(builder);
        umbra_val off = umbra_load_i32(builder,
            (umbra_ptr){1}, umbra_imm_i32(builder, 0));
        umbra_val ixo = umbra_add_i32(builder, ix, off);
        umbra_val val = umbra_widen_f16(builder,
            umbra_load_i16(builder,
                (umbra_ptr){0}, ixo));
        umbra_store_i16(builder, (umbra_ptr){2}, ix,
            umbra_narrow_f32(builder, val));
        backends B = make_full(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint16_t src[16];
            for (int k = 0; k < 16; k++) {
                __fp16 h = (__fp16)(k + 10);
                __builtin_memcpy(&src[k], &h, 2);
            }
            int32_t  uni[1]  = {5};
            uint16_t dst[8]  = {0};
            if (!run(&B, bi, 8, (umbra_buf[]){
                {src, (long)sizeof src},
                {uni, -(long)sizeof uni},
                {dst, (long)sizeof dst},
            })) { continue; }
            for (int k = 0; k < 8; k++) {
                __fp16 h;
                __builtin_memcpy(&h, &dst[k], 2);
                equiv((float)h,
                      (float)(15 + k)) here;
            }
        }
        cleanup(&B);
    }
  }
}

static void test_shift_imm(void) {
  for (int opt = 0; opt < 2; opt++) {
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val x = umbra_load_i32(builder,
                          (umbra_ptr){0}, ix);
        umbra_val r = umbra_shl_i32(builder, x,
                          umbra_imm_i32(builder, 8));
        umbra_store_i32(builder, (umbra_ptr){1}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint32_t src[] = {1, 2, 3, 0xff};
            uint32_t dst[4] = {0};
            if (!run(&B, bi, 4, (umbra_buf[]){
                {src,4*4},{dst,4*4},
            })) { continue; }
            (dst[0] == 0x100u)  here;
            (dst[1] == 0x200u)  here;
            (dst[2] == 0x300u)  here;
            (dst[3] == 0xff00u) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val x = umbra_load_i32(builder,
                          (umbra_ptr){0}, ix);
        umbra_val r = umbra_shr_u32(builder, x,
                          umbra_imm_i32(builder, 8));
        umbra_store_i32(builder, (umbra_ptr){1}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint32_t src[] = {0x100, 0x200, 0xff00};
            uint32_t dst[3] = {0};
            if (!run(&B, bi, 3, (umbra_buf[]){
                {src,3*4},{dst,3*4},
            })) { continue; }
            (dst[0] == 1)    here;
            (dst[1] == 2)    here;
            (dst[2] == 0xff) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder =
            umbra_builder();
        umbra_val ix = umbra_lane(builder);
        umbra_val x = umbra_load_i32(builder,
                          (umbra_ptr){0}, ix);
        umbra_val r = umbra_shr_s32(builder, x,
                          umbra_imm_i32(builder, 4));
        umbra_store_i32(builder, (umbra_ptr){1}, ix, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 4; bi++) {
            uint32_t src[] = {0x80, 0xfffffff0u};
            uint32_t dst[2] = {0};
            if (!run(&B, bi, 2, (umbra_buf[]){
                {src,2*4},{dst,2*4},
            })) { continue; }
            (dst[0] == 8) here;
            (dst[1] == 0xffffffffu) here;
        }
        cleanup(&B);
    }
  }
}

static void test_pack_channels(void) {
  for (int opt = 0; opt < 2; opt++) {
    struct umbra_builder *builder =
        umbra_builder();
    umbra_val ix = umbra_lane(builder);
    umbra_val r = umbra_load_i32(builder,
                      (umbra_ptr){0}, ix);
    umbra_val g = umbra_load_i32(builder,
                      (umbra_ptr){1}, ix);
    umbra_val b = umbra_load_i32(builder,
                      (umbra_ptr){2}, ix);
    umbra_val a = umbra_load_i32(builder,
                      (umbra_ptr){3}, ix);
    umbra_val mask = umbra_imm_i32(builder, 0xff);
    umbra_val px = umbra_and_i32(builder, r, mask);
    px = umbra_pack(builder, px,
             umbra_and_i32(builder, g, mask), 8);
    px = umbra_pack(builder, px,
             umbra_and_i32(builder, b, mask), 16);
    px = umbra_pack(builder, px, a, 24);
    umbra_store_i32(builder, (umbra_ptr){4}, ix, px);
    backends B = make(builder, opt);
    for (int bi = 0; bi < 4; bi++) {
        uint32_t rr[] = {0xAA, 0x11, 0xFF};
        uint32_t gg[] = {0xBB, 0x22, 0x00};
        uint32_t bb_[] = {0xCC, 0x33, 0xFF};
        uint32_t aa[] = {0xDD, 0x44, 0x00};
        uint32_t dst[3] = {0};
        if (!run(&B, bi, 3, (umbra_buf[]){
            {rr,3*4},{gg,3*4},{bb_,3*4},
            {aa,3*4},{dst,3*4},
        })) { continue; }
        (dst[0] == 0xDDCCBBAAu) here;
        (dst[1] == 0x44332211u) here;
        (dst[2] == 0x00FF00FFu) here;
    }
    cleanup(&B);
  }
}

static _Bool run_m_(backends *B, int b,
                    int n, int m, int loop_off,
                    umbra_buf buf[]) {
    switch (b) {
    case 0:
        umbra_interpreter_run_m(
            B->interp, n, m, loop_off, buf);
        return 1;
    case 1:
        if (B->cg) {
            umbra_codegen_run_m(
                B->cg, n, m, loop_off, buf);
            return 1;
        }
        return 0;
    case 2:
        if (B->jit) {
            umbra_jit_run_m(
                B->jit, n, m, loop_off, buf);
            return 1;
        }
        return 0;
    case 3:
        if (B->mtl) {
            umbra_metal_run_m(
                B->mtl, n, m, loop_off, buf);
            return 1;
        }
        return 0;
    }
    return 0;
}

static void test_run_m(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val ix = umbra_lane(b);
    int ji = umbra_reserve(b, 1);
    umbra_val j = umbra_load_i32(b, (umbra_ptr){1},
                      umbra_imm_i32(b, ji));
    umbra_val jf = umbra_cvt_f32_i32(b, j);
    umbra_val acc = umbra_load_i32(b,
                        (umbra_ptr){0}, ix);
    umbra_val sum = umbra_add_f32(b, acc, jf);
    umbra_store_i32(b, (umbra_ptr){0}, ix, sum);
    int loop_off = ji * 4;

    backends B = make_full(b, 0);

    enum { N = 17, M = 4 };

    for (int bi = 0; bi < 4; bi++) {
        float buf0[N];
        __builtin_memset(buf0, 0, sizeof buf0);
        int32_t uni[4] = {0};
        umbra_buf buf[] = {
            { buf0, (long)sizeof buf0 },
            { uni, -(long)sizeof uni },
        };
        if (!run_m_(&B, bi, N, M, loop_off, buf)) {
            continue;
        }
        for (int i = 0; i < N; i++) {
            (equiv(buf0[i], 6.0f)) here;
        }
    }

    cleanup(&B);
}

static void test_gather_deref_large(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val ix = umbra_lane(b);
    umbra_val idx = umbra_load_i32(b,
                        (umbra_ptr){0}, ix);
    int off = umbra_reserve_ptr(b);
    umbra_ptr src = umbra_deref_ptr(b,
                        (umbra_ptr){1}, off);
    umbra_val val = umbra_widen_s16(b,
                        umbra_load_i16(b, src, idx));
    umbra_store_i32(b, (umbra_ptr){2}, ix, val);
    backends B = make_full(b, 0);

    enum { N = 33000 };
    static int16_t data[N];
    __builtin_memset(data, 0, sizeof data);
    data[0]     = 10;
    data[100]   = 20;
    data[32800] = 30;
    data[N-1]   = 42;

    int32_t indices[4] = {0, 100, 32800, N-1};
    int32_t dst[4]     = {0};

    long long uni_[2] = {0};
    char *uni = (char*)uni_;
    {
        void *p = data;
        long sz = (long)(N * 2);
        __builtin_memcpy(uni + off,     &p,  8);
        __builtin_memcpy(uni + off + 8, &sz, 8);
    }

    for (int bi = 0; bi < 4; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 4, (umbra_buf[]){
            {indices, (long)sizeof indices},
            {uni,     -(long)sizeof uni_},
            {dst,     (long)sizeof dst},
        })) { continue; }
        (dst[0] == 10) here;
        (dst[1] == 20) here;
        (dst[2] == 30) here;
        (dst[3] == 42) here;
    }

    cleanup(&B);
}

int main(void) {
    test_f32_ops();
    test_i32_ops();
    test_cmp_i32();
    test_cmp_f32();
    test_imm();
    test_fma_f32();
    test_min_max_sqrt_f32();
    test_abs_sign_f32();
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
    test_shift_imm();
    test_pack_channels();
    test_run_m();
    test_gather_deref_large();
    return 0;
}
