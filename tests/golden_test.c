#include "../slides/slides.h"
#include "test.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct umbra_builder builder;

enum { W = 128, H = 96, LUT_N = 64 };

static slug_curves gt_slug;

typedef void (*run_fn)(void*, int, umbra_buf[]);

static void run_interp(void *ctx, int n,
                       umbra_buf buf[]) {
    umbra_interpreter_run(ctx, n, buf);
}
static void run_jit(void *ctx, int n,
                    umbra_buf buf[]) {
    umbra_jit_run(ctx, n, buf);
}
static void run_codegen(void *ctx, int n,
                        umbra_buf buf[]) {
    umbra_codegen_run(ctx, n, buf);
}
static void run_metal(void *ctx, int n,
                      umbra_buf buf[]) {
    umbra_metal_run(ctx, n, buf);
}

enum { NUM_BACKENDS = 4 };
static char const *backend_name[NUM_BACKENDS] = {
    "interp", "jit", "codegen", "metal",
};
static run_fn const run_fns[NUM_BACKENDS] = {
    run_interp, run_jit, run_codegen, run_metal,
};

enum {
    FMT_8888, FMT_565, FMT_FP16,
    FMT_FP16P, FMT_1010102, NUM_FMTS,
};
static char const *fmt_name[] = {
    "8888", "565", "fp16", "fp16p", "1010102",
};
static umbra_load_fn fmt_load[] = {
    umbra_load_8888,  umbra_load_565,
    umbra_load_fp16,  umbra_load_fp16_planar,
    umbra_load_1010102,
};
static umbra_store_fn fmt_store[] = {
    umbra_store_8888, umbra_store_565,
    umbra_store_fp16, umbra_store_fp16_planar,
    umbra_store_1010102,
};
static int fmt_bpp[] = {4, 2, 8, 2, 4};
static int fmt_tol[] = {0, 0, 0, 0, 0};

typedef struct {
    struct umbra_interpreter *interp;
    int                       uni_len;
    int                       pad_;
} pipe;

static pipe fill_pipes[NUM_FMTS];
static pipe readback_pipes[NUM_FMTS];

static void build_fill(int fmt) {
    builder *builder = umbra_builder();
    umbra_val ix = umbra_lane(builder);
    int fi = umbra_reserve(builder, 4);
    umbra_color c = {
        umbra_load_i32(builder, (umbra_ptr){1},
                     umbra_imm_i32(builder, fi)),
        umbra_load_i32(builder, (umbra_ptr){1},
                     umbra_imm_i32(builder, fi+1)),
        umbra_load_i32(builder, (umbra_ptr){1},
                     umbra_imm_i32(builder, fi+2)),
        umbra_load_i32(builder, (umbra_ptr){1},
                     umbra_imm_i32(builder, fi+3)),
    };
    fmt_store[fmt](builder, (umbra_ptr){0}, ix, c);
    fill_pipes[fmt].uni_len = umbra_uni_len(builder);
    struct umbra_basic_block *opt = umbra_basic_block(builder);
    umbra_builder_free(builder);
    fill_pipes[fmt].interp = umbra_interpreter(opt);
    umbra_basic_block_free(opt);
}

static void build_readback(int fmt) {
    builder *builder = umbra_builder();
    umbra_val ix = umbra_lane(builder);
    umbra_color c =
        fmt_load[fmt](builder, (umbra_ptr){0}, ix);
    umbra_store_8888(builder, (umbra_ptr){2}, ix, c);
    readback_pipes[fmt].uni_len = umbra_uni_len(builder);
    struct umbra_basic_block *opt = umbra_basic_block(builder);
    umbra_builder_free(builder);
    readback_pipes[fmt].interp =
        umbra_interpreter(opt);
    umbra_basic_block_free(opt);
}

static void build_pipes(void) {
    for (int f = 0; f < NUM_FMTS; f++) {
        build_fill(f);
        build_readback(f);
    }
}

static void free_pipes(void) {
    for (int f = 0; f < NUM_FMTS; f++) {
        if (fill_pipes[f].interp) {
            umbra_interpreter_free(
                fill_pipes[f].interp);
        }
        if (readback_pipes[f].interp) {
            umbra_interpreter_free(
                readback_pipes[f].interp);
        }
    }
}

static void uni_f32(char *u, int off,
                    float const *v, int n) {
    __builtin_memcpy(u+off, v, (unsigned long)n*4);
}
static void uni_i32(char *u, int off, int32_t v) {
    __builtin_memcpy(u+off, &v, 4);
}
static void uni_ptr(char *u, int off,
                    void *p, long sz) {
    __builtin_memcpy(u+off,   &p,  8);
    __builtin_memcpy(u+off+8, &sz, 8);
}

static void fill_bg_row(int fmt, void *dst,
                        int n, uint32_t bg,
                        long row_sz,
                        int32_t stride) {
    float hc[4] = {
        (float)( bg        & 0xffu) / 255.0f,
        (float)((bg >>  8) & 0xffu) / 255.0f,
        (float)((bg >> 16) & 0xffu) / 255.0f,
        (float)((bg >> 24) & 0xffu) / 255.0f,
    };
    long long uni_[4] = {0};
    char *uni = (char*)uni_;
    uni_f32(uni, 0, hc, 4);
    if (fill_pipes[fmt].uni_len > 16) {
        uni_i32(uni, 16, stride);
    }
    umbra_buf buf[] = {
        { dst,  row_sz },
        { uni, -(long)fill_pipes[fmt].uni_len },
    };
    umbra_interpreter_run(
        fill_pipes[fmt].interp, n, buf);
}

static void readback_row(int fmt, uint32_t *dst,
                         void *src, int n,
                         long src_sz,
                         int32_t stride) {
    long long uni_[2] = {0};
    char *uni = (char*)uni_;
    if (readback_pipes[fmt].uni_len > 0) {
        __builtin_memcpy(uni, &stride, 4);
    }
    umbra_buf buf[] = {
        { src,  -src_sz },
        { uni,  -(long)readback_pipes[fmt].uni_len },
        { dst,  (long)(n * 4) },
    };
    umbra_interpreter_run(
        readback_pipes[fmt].interp, n, buf);
}

static float linear_lut[LUT_N * 4];
static float radial_lut[LUT_N * 4];

static void build_luts(void) {
    float const linear_stops[][4] = {
        {1.2f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.8f, 0.0f, 1.0f},
        {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f},
        {0.0f, 0.0f, 1.2f, 1.0f},
        {0.8f, 0.0f, 1.0f, 1.0f},
    };
    umbra_gradient_lut_even(linear_lut, LUT_N,
                            6, linear_stops);

    float const radial_stops[][4] = {
        {1.5f, 1.5f, 1.2f, 1.0f},
        {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f},
        {0.05f, 0.0f, 0.15f, 1.0f},
    };
    umbra_gradient_lut_even(radial_lut, LUT_N,
                            4, radial_stops);
}

static void build_perspective_matrix(
        float out[11], float t,
        int sw, int sh, int bw, int bh) {
    float cx = (float)sw * 0.5f;
    float cy = (float)sh * 0.5f;
    float bx = (float)bw * 0.5f;
    float by = (float)bh * 0.5f;
    float angle = t * 0.3f;
    float tilt  = sinf(t * 0.7f) * 0.0008f;
    float sc    = 1.0f + 0.2f * sinf(t * 0.5f);
    float ca = cosf(angle), sa = sinf(angle);
    float w0 = 1.0f - tilt * cx;
    out[0] = ca * sc;     out[1] = sa * sc;
    out[2] = -cx*ca*sc - cy*sa*sc + bx*w0;
    out[3] = -sa * sc;    out[4] = ca * sc;
    out[5] = cx*sa*sc - cy*ca*sc + by*w0;
    out[6] = tilt;  out[7] = 0.0f;  out[8] = w0;
    out[9] = (float)bw; out[10] = (float)bh;
}

static void render_slide(
        int slide_idx, int fmt,
        void *ctx, run_fn run,
        void *pixbuf,
        text_cov *bitmap_cov, text_cov *sdf_cov,
        umbra_draw_layout const *lay) {
    slide const *s = &slides[slide_idx];
    float hc[8];
    for (int i = 0; i < 8; i++) {
        hc[i] = s->color[i];
    }

    int bpp = fmt_bpp[fmt];
    _Bool planar = (fmt == FMT_FP16P);
    int32_t planar_stride = W * H;
    long row_sz = planar
        ? (long)(W * H * 4) * 2
        : (long)(W * bpp);

    #define ROW(y) (planar \
        ? (void*)((__fp16*)pixbuf + (y) * W) \
        : (void*)((uint8_t*)pixbuf             \
                  + (y) * W * bpp))

    for (int y = 0; y < H; y++) {
        fill_bg_row(fmt, ROW(y), W,
                    s->bg, row_sz,
                    planar_stride);
    }

    int uni_len = lay->uni_len;
    int ps = planar ? (s->load ? 2 : 1) : 0;

    if (s->coverage ==
            umbra_coverage_bitmap_matrix) {
        float mat[11];
        build_perspective_matrix(mat, 1.0f,
            W, H, bitmap_cov->w, bitmap_cov->h);
        for (int y = 0; y < H; y++) {
            long long uni_[12] = {0};
            char *uni = (char*)uni_;
            uni_i32(uni, lay->x0, 0);
            uni_i32(uni, lay->y,  y);
            uni_f32(uni, lay->shader, hc, 4);
            uni_f32(uni, lay->coverage, mat, 11);
            uni_ptr(uni,
                (lay->coverage + 11*4 + 7) & ~7,
                bitmap_cov->data,
                (long)(W * H * 2));
            for (int i = 0; i < ps; i++) {
                uni_i32(uni,
                    uni_len - (ps - i) * 4,
                    planar_stride);
            }
            umbra_buf buf[] = {
                { ROW(y), row_sz },
                { uni, -(long)uni_len },
            };
            run(ctx, W, buf);
        }
    } else if (s->coverage ==
                   umbra_coverage_slug) {
        float mat[11];
        build_perspective_matrix(mat, 1.0f,
            W, H, (int)gt_slug.w, (int)gt_slug.h);
        mat[9]  = gt_slug.w;
        mat[10] = gt_slug.h;
        for (int y = 0; y < H; y++) {
            long long uni_[12] = {0};
            char *uni = (char*)uni_;
            uni_i32(uni, lay->x0, 0);
            uni_i32(uni, lay->y,  y);
            uni_f32(uni, lay->shader, hc, 4);
            uni_f32(uni, lay->coverage, mat, 11);
            uni_ptr(uni,
                (lay->coverage + 11*4 + 7) & ~7,
                gt_slug.data,
                (long)(gt_slug.count * 6 * 4));
            for (int i = 0; i < ps; i++) {
                uni_i32(uni,
                    uni_len - (ps - i) * 4,
                    planar_stride);
            }
            umbra_buf buf[] = {
                { ROW(y), row_sz },
                { uni, -(long)uni_len },
            };
            run(ctx, W, buf);
        }
    } else if (s->coverage ==
                   umbra_coverage_bitmap ||
               s->coverage ==
                   umbra_coverage_sdf) {
        text_cov *tc =
            (s->coverage == umbra_coverage_bitmap)
                ? bitmap_cov : sdf_cov;
        for (int y = 0; y < H; y++) {
            long long uni_[6] = {0};
            char *uni = (char*)uni_;
            uni_i32(uni, lay->x0, 0);
            uni_i32(uni, lay->y,  y);
            uni_f32(uni, lay->shader, hc, 4);
            uni_ptr(uni, lay->coverage,
                    tc->data + y * W,
                    (long)(W * 2));
            for (int i = 0; i < ps; i++) {
                uni_i32(uni,
                    uni_len - (ps - i) * 4,
                    planar_stride);
            }
            umbra_buf buf[] = {
                { ROW(y), row_sz },
                { uni, -(long)uni_len },
            };
            run(ctx, W, buf);
        }
    } else if (s->shader != umbra_shader_solid) {
        _Bool is_lut =
            (s->shader == umbra_shader_linear_grad
          || s->shader == umbra_shader_radial_grad);
        float gp[4] = {
            s->grad[0], s->grad[1],
            s->grad[2], s->grad[3],
        };

        for (int y = 0; y < H; y++) {
            long long uni_[8] = {0};
            char *uni = (char*)uni_;
            uni_i32(uni, lay->x0, 0);
            uni_i32(uni, lay->y,  y);
            if (is_lut) {
                float *lut =
                    (s->shader ==
                     umbra_shader_linear_grad)
                        ? linear_lut : radial_lut;
                uni_f32(uni, lay->shader, gp, 4);
                uni_ptr(uni,
                    (lay->shader + 16 + 7) & ~7,
                    lut, (long)(LUT_N * 4 * 4));
            } else {
                uni_f32(uni, lay->shader, gp, 3);
                uni_f32(uni,
                    lay->shader + 12, hc, 8);
            }
            for (int i = 0; i < ps; i++) {
                uni_i32(uni,
                    uni_len - (ps - i) * 4,
                    planar_stride);
            }
            umbra_buf buf[] = {
                { ROW(y), row_sz },
                { uni, -(long)uni_len },
            };
            run(ctx, W, buf);
        }
    } else {
        float rect[4] = {
            30.0f, 20.0f, 90.0f, 60.0f,
        };
        for (int y = 0; y < H; y++) {
            long long uni_[6] = {0};
            char *uni = (char*)uni_;
            uni_i32(uni, lay->x0, 0);
            uni_i32(uni, lay->y,  y);
            uni_f32(uni, lay->shader, hc, 4);
            if (s->coverage) {
                uni_f32(uni, lay->coverage,
                        rect, 4);
            }
            for (int i = 0; i < ps; i++) {
                uni_i32(uni,
                    uni_len - (ps - i) * 4,
                    planar_stride);
            }
            umbra_buf buf[] = {
                { ROW(y), row_sz },
                { uni, -(long)uni_len },
            };
            run(ctx, W, buf);
        }
    }
    #undef ROW
}

static void readback_to_8888(int fmt,
                             void *pixbuf,
                             uint32_t *out) {
    _Bool planar = (fmt == FMT_FP16P);
    int bpp = fmt_bpp[fmt];
    int32_t planar_stride = W * H;
    long row_sz = planar
        ? (long)(W * H * 4) * 2
        : (long)(W * bpp);

    for (int y = 0; y < H; y++) {
        void *src = planar
            ? (void*)((__fp16*)pixbuf + y * W)
            : (void*)((uint8_t*)pixbuf
                      + y * W * bpp);
        readback_row(fmt, out + y * W, src,
                     W, row_sz, planar_stride);
    }
}

static void test_slide_golden(
        int slide_idx, int fmt,
        text_cov *bitmap_cov,
        text_cov *sdf_cov) {
    slide const *s = &slides[slide_idx];

    umbra_load_fn  load =
        s->load ? fmt_load[fmt] : NULL;
    umbra_store_fn store = fmt_store[fmt];

    umbra_draw_layout lay;
    struct umbra_builder *bld =
        umbra_draw_build(s->shader, s->coverage,
                         s->blend, load, store,
                         &lay);
    struct umbra_basic_block *bb = umbra_basic_block(bld);
    umbra_builder_free(bld);

    struct umbra_interpreter *interp =
        umbra_interpreter(bb);
    struct umbra_jit     *jit = umbra_jit(bb);
    struct umbra_codegen *cg  = umbra_codegen(bb);
    struct umbra_metal   *mtl = umbra_metal(bb);
    umbra_basic_block_free(bb);

    void *backs[NUM_BACKENDS] = {
        interp, jit, cg, mtl,
    };

    size_t pixbuf_sz =
        (fmt == FMT_FP16P)
            ? (size_t)(W * H * 4) * 2
            : (size_t)(W * H)
              * (size_t)fmt_bpp[fmt];
    void *pbuf_ref = malloc(pixbuf_sz);
    void *pbuf_tst = malloc(pixbuf_sz);
    uint32_t *ref = malloc((size_t)(W * H) * 4);
    uint32_t *tst = malloc((size_t)(W * H) * 4);

    render_slide(slide_idx, fmt,
                 interp, run_interp,
                 pbuf_ref,
                 bitmap_cov, sdf_cov, &lay);
    readback_to_8888(fmt, pbuf_ref, ref);

    for (int bi = 1; bi < NUM_BACKENDS; bi++) {
        if (!backs[bi]) { continue; }
        if (bi == 1 && fmt == FMT_1010102) {
            continue;
        }
        if (bi == 3) { continue; }
        render_slide(slide_idx, fmt,
                     backs[bi], run_fns[bi],
                     pbuf_tst,
                     bitmap_cov, sdf_cov, &lay);
        readback_to_8888(fmt, pbuf_tst, tst);

        int mismatches = 0;
        int worst = 0;
        for (int i = 0; i < W * H; i++) {
            if (ref[i] == tst[i]) { continue; }
            for (int ch = 0; ch < 4; ch++) {
                int a = (int)(
                    (ref[i] >> (ch*8)) & 0xff);
                int b = (int)(
                    (tst[i] >> (ch*8)) & 0xff);
                int d = a - b;
                if (d < 0) { d = -d; }
                if (d > worst) { worst = d; }
            }
            mismatches++;
        }
        int tol = fmt_tol[fmt];
        if (worst > tol) {
            dprintf(2,
                "slide %d \"%s\" %s/%s: "
                "%d/%d pixels differ, "
                "worst channel delta = %d "
                "(tol %d)\n",
                slide_idx + 1, s->title,
                backend_name[bi], fmt_name[fmt],
                mismatches, W * H, worst, tol);
        }
        (worst <= tol) here;
    }

    free(ref);
    free(tst);
    free(pbuf_ref);
    free(pbuf_tst);
    umbra_interpreter_free(interp);
    if (jit) { umbra_jit_free(jit); }
    if (cg)  { umbra_codegen_free(cg); }
    if (mtl) { umbra_metal_free(mtl); }
}

int main(void) {
    text_cov bitmap_cov =
        text_rasterize(W, H, 24.0f, 0);
    text_cov sdf_cov =
        text_rasterize(W, H, 24.0f, 1);
    gt_slug = slug_extract("Hi", 24.0f);
    slug_n_curves = gt_slug.count;
    build_luts();
    build_pipes();

    for (int fi = 0; fi < NUM_FMTS; fi++) {
        for (int si = 0; si < SLIDE_COUNT; si++) {
            test_slide_golden(si, fi,
                &bitmap_cov, &sdf_cov);
        }
    }

    text_cov_free(&bitmap_cov);
    text_cov_free(&sdf_cov);
    slug_free(&gt_slug);
    free_pipes();
    return 0;
}
