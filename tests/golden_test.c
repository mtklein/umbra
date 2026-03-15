#include "../slides/slides.h"
#include "test.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

enum { W = 128, H = 96, LUT_N = 64 };

typedef void (*run_fn)(void*, int, umbra_buf[]);

static void run_interp (void *ctx, int n, umbra_buf buf[]) { umbra_interpreter_run(ctx, n, buf); }
static void run_jit    (void *ctx, int n, umbra_buf buf[]) { umbra_jit_run(ctx, n, buf); }
static void run_codegen(void *ctx, int n, umbra_buf buf[]) { umbra_codegen_run(ctx, n, buf); }
static void run_metal  (void *ctx, int n, umbra_buf buf[]) { umbra_metal_run(ctx, n, buf); }

enum { NUM_BACKENDS = 4 };
static char const *backend_name[NUM_BACKENDS] = {"interp", "jit", "codegen", "metal"};
static run_fn const run_fns[NUM_BACKENDS] = {run_interp, run_jit, run_codegen, run_metal};

static __fp16 linear_lut[LUT_N * 4];
static __fp16 radial_lut[LUT_N * 4];

static void build_luts(void) {
    __fp16 const linear_stops[][4] = {
        {(__fp16)1.2f, (__fp16)0.0f, (__fp16)0.0f, (__fp16)1.0f},
        {(__fp16)1.0f, (__fp16)0.8f, (__fp16)0.0f, (__fp16)1.0f},
        {(__fp16)0.0f, (__fp16)1.2f, (__fp16)0.0f, (__fp16)1.0f},
        {(__fp16)0.0f, (__fp16)0.8f, (__fp16)1.2f, (__fp16)1.0f},
        {(__fp16)0.0f, (__fp16)0.0f, (__fp16)1.2f, (__fp16)1.0f},
        {(__fp16)0.8f, (__fp16)0.0f, (__fp16)1.0f, (__fp16)1.0f},
    };
    umbra_gradient_lut_even(linear_lut, LUT_N, 6, linear_stops);

    __fp16 const radial_stops[][4] = {
        {(__fp16)1.5f, (__fp16)1.5f, (__fp16)1.2f, (__fp16)1.0f},
        {(__fp16)1.2f, (__fp16)0.8f, (__fp16)0.0f, (__fp16)1.0f},
        {(__fp16)0.8f, (__fp16)0.0f, (__fp16)0.2f, (__fp16)1.0f},
        {(__fp16)0.05f, (__fp16)0.0f, (__fp16)0.15f, (__fp16)1.0f},
    };
    umbra_gradient_lut_even(radial_lut, LUT_N, 4, radial_stops);
}

static void build_perspective_matrix(float out[11], float t, int sw, int sh, int bw, int bh) {
    float cx = (float)sw * 0.5f, cy = (float)sh * 0.5f;
    float bx = (float)bw * 0.5f, by = (float)bh * 0.5f;
    float angle = t * 0.3f;
    float tilt  = sinf(t * 0.7f) * 0.0008f;
    float sc    = 1.0f + 0.2f * sinf(t * 0.5f);
    float ca = cosf(angle), sa = sinf(angle);
    float w0 = 1.0f - tilt * cx;
    out[0] = ca * sc;     out[1] = sa * sc;
    out[2] = -cx*ca*sc - cy*sa*sc + bx*w0;
    out[3] = -sa * sc;    out[4] = ca * sc;
    out[5] = cx*sa*sc - cy*ca*sc + by*w0;
    out[6] = tilt;        out[7] = 0.0f;       out[8] = w0;
    out[9] = (float)bw;   out[10] = (float)bh;
}

static void uni_i32(char *u, int off, int32_t v) { __builtin_memcpy(u+off, &v, 4); }
static void uni_h4 (char *u, int off, __fp16 const c[4]) { __builtin_memcpy(u+off, c, 8); }
static void uni_h8 (char *u, int off, __fp16 const c[8]) { __builtin_memcpy(u+off, c, 16); }
static void uni_f32(char *u, int off, float const *v, int n) { __builtin_memcpy(u+off, v, (unsigned long)n*4); }
static void uni_ptr(char *u, int off, void *p, long sz) {
    __builtin_memcpy(u+off,   &p,  8);
    __builtin_memcpy(u+off+8, &sz, 8);
}

static void render_slide(int slide_idx, void *ctx, run_fn run,
                         uint32_t *px, text_cov *bitmap_cov, text_cov *sdf_cov,
                         umbra_draw_layout const *lay) {
    slide const *s = &slides[slide_idx];
    __fp16 hc[8];
    for (int i = 0; i < 8; i++) { hc[i] = (__fp16)s->color[i]; }

    for (int i = 0; i < W * H; i++) { px[i] = s->bg; }

    if (s->coverage == umbra_coverage_bitmap_matrix) {
        float mat[11];
        build_perspective_matrix(mat, 1.0f, W, H, bitmap_cov->w, bitmap_cov->h);
        for (int y = 0; y < H; y++) {
            long long uni_[10] = {0}; char *uni = (char*)uni_;
            uni_i32(uni, lay->x0, 0);
            uni_i32(uni, lay->y,  y);
            uni_h4 (uni, 8, hc);
            uni_f32(uni, 16, mat, 11);
            uni_ptr(uni, 64, bitmap_cov->data, (long)(W * H * 2));
            umbra_buf buf[] = {
                { px + y * W, W * 4 },
                { uni, -(long)lay->uni_len },
            };
            run(ctx, W, buf);
        }
    } else if (s->coverage == umbra_coverage_bitmap || s->coverage == umbra_coverage_sdf) {
        text_cov *tc = (s->coverage == umbra_coverage_bitmap) ? bitmap_cov : sdf_cov;
        for (int y = 0; y < H; y++) {
            long long uni_[4] = {0}; char *uni = (char*)uni_;
            uni_i32(uni, lay->x0, 0);
            uni_i32(uni, lay->y,  y);
            uni_h4 (uni, 8, hc);
            uni_ptr(uni, 16, tc->data + y * W, (long)(W * 2));
            umbra_buf buf[] = {
                { px + y * W, W * 4 },
                { uni, -(long)lay->uni_len },
            };
            run(ctx, W, buf);
        }
    } else if (s->shader != umbra_shader_solid) {
        _Bool is_lut = (s->shader == umbra_shader_linear_grad ||
                        s->shader == umbra_shader_radial_grad);
        float gp[4] = {s->grad[0], s->grad[1], s->grad[2], s->grad[3]};

        for (int y = 0; y < H; y++) {
            long long uni_[5] = {0}; char *uni = (char*)uni_;
            uni_i32(uni, lay->x0, 0);
            uni_i32(uni, lay->y,  y);
            if (is_lut) {
                __fp16 *lut = (s->shader == umbra_shader_linear_grad) ? linear_lut
                                                                      : radial_lut;
                uni_f32(uni, 8, gp, 4);
                uni_ptr(uni, 24, lut, (long)(LUT_N * 4 * 2));
            } else {
                uni_f32(uni, 8, gp, 3);
                uni_h8 (uni, 20, hc);
            }
            umbra_buf buf[] = {
                { px + y * W, W * 4 },
                { uni, -(long)lay->uni_len },
            };
            run(ctx, W, buf);
        }
    } else {
        float rect[4] = { 30.0f, 20.0f, 90.0f, 60.0f };
        for (int y = 0; y < H; y++) {
            long long uni_[4] = {0}; char *uni = (char*)uni_;
            uni_i32(uni, lay->x0, 0);
            uni_i32(uni, lay->y,  y);
            uni_h4 (uni, 8, hc);
            uni_f32(uni, 16, rect, 4);
            umbra_buf buf[] = {
                { px + y * W, W * 4 },
                { uni, -(long)lay->uni_len },
            };
            run(ctx, W, buf);
        }
    }
}

static void test_slide_golden(int slide_idx, text_cov *bitmap_cov, text_cov *sdf_cov) {
    slide const *s = &slides[slide_idx];

    umbra_draw_layout lay;
    struct umbra_basic_block *bb = umbra_draw_build(
        s->shader, s->coverage, s->blend, s->load, s->store, &lay);
    umbra_basic_block_optimize(bb);

    struct umbra_interpreter *interp = umbra_interpreter(bb);
    struct umbra_jit         *jit    = umbra_jit(bb);
    struct umbra_codegen     *cg     = umbra_codegen(bb);
    struct umbra_metal       *mtl    = umbra_metal(bb);
    umbra_basic_block_free(bb);

    void *backends[NUM_BACKENDS] = { interp, jit, cg, mtl };

    uint32_t *ref = malloc((size_t)(W * H) * 4);
    uint32_t *tst = malloc((size_t)(W * H) * 4);

    render_slide(slide_idx, interp, run_interp, ref, bitmap_cov, sdf_cov, &lay);

    for (int bi = 1; bi < NUM_BACKENDS; bi++) {
        if (!backends[bi]) { continue; }
        render_slide(slide_idx, backends[bi], run_fns[bi], tst, bitmap_cov, sdf_cov, &lay);

        int mismatches = 0;
        int worst = 0;
        for (int i = 0; i < W * H; i++) {
            if (ref[i] == tst[i]) { continue; }
            for (int ch = 0; ch < 4; ch++) {
                int a = (int)((ref[i] >> (ch * 8)) & 0xFF);
                int b = (int)((tst[i] >> (ch * 8)) & 0xFF);
                int d = a - b;
                if (d < 0) { d = -d; }
                if (d > worst) { worst = d; }
            }
            mismatches++;
        }
        if (worst > 1) {
            dprintf(2, "slide %d \"%s\" %s: %d/%d pixels differ, "
                    "worst channel delta = %d\n",
                    slide_idx + 1, s->title, backend_name[bi],
                    mismatches, W * H, worst);
        }
        (worst <= 1) here;
    }

    free(ref);
    free(tst);
    umbra_interpreter_free(interp);
    if (jit) { umbra_jit_free(jit); }
    if (cg)  { umbra_codegen_free(cg); }
    if (mtl) { umbra_metal_free(mtl); }
}

int main(void) {
    text_cov bitmap_cov = text_rasterize(W, H, 24.0f, 0);
    text_cov sdf_cov    = text_rasterize(W, H, 24.0f, 1);
    build_luts();

    for (int i = 0; i < SLIDE_COUNT; i++) {
        test_slide_golden(i, &bitmap_cov, &sdf_cov);
    }

    text_cov_free(&bitmap_cov);
    text_cov_free(&sdf_cov);
    return 0;
}
