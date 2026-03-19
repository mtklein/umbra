#include "../slides/slide.h"
#include "../slides/text.h"
#include "../slides/slug.h"
#include "test.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct umbra_builder builder;

enum { W = 128, H = 96 };

enum { N_BACKS = 3 };
static char const *backend_name[N_BACKS] = {
    "interp", "jit", "metal",
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
    struct umbra_program *prog;
    int                   uni_len;
    int                   pad_;
} pipe;

static pipe fill_pipes[NUM_FMTS];
static pipe readback_pipes[NUM_FMTS];
static struct umbra_backend *interp_be;

static void build_fill(int fmt) {
    builder *builder = umbra_builder();
    umbra_val ix = umbra_iota(builder);
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
    fill_pipes[fmt].uni_len =
        umbra_uni_len(builder);
    struct umbra_basic_block *opt =
        umbra_basic_block(builder);
    umbra_builder_free(builder);
    fill_pipes[fmt].prog =
        umbra_backend_compile(interp_be, opt);
    umbra_basic_block_free(opt);
}

static void build_readback(int fmt) {
    builder *builder = umbra_builder();
    umbra_val ix = umbra_iota(builder);
    umbra_color c =
        fmt_load[fmt](builder, (umbra_ptr){0}, ix);
    umbra_store_8888(builder,
        (umbra_ptr){2}, ix, c);
    readback_pipes[fmt].uni_len =
        umbra_uni_len(builder);
    struct umbra_basic_block *opt =
        umbra_basic_block(builder);
    umbra_builder_free(builder);
    readback_pipes[fmt].prog =
        umbra_backend_compile(interp_be, opt);
    umbra_basic_block_free(opt);
}

static void build_pipes(void) {
    interp_be = umbra_backend_interp();
    for (int f = 0; f < NUM_FMTS; f++) {
        build_fill(f);
        build_readback(f);
    }
}

static void free_pipes(void) {
    for (int f = 0; f < NUM_FMTS; f++) {
        umbra_program_free(fill_pipes[f].prog);
        umbra_program_free(readback_pipes[f].prog);
    }
    umbra_backend_free(interp_be);
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
    slide_uni_f32(uni, 0, hc, 4);
    if (fill_pipes[fmt].uni_len > 16) {
        slide_uni_i32(uni, 16, stride);
    }
    umbra_buf buf[] = {
        { dst,  row_sz },
        { uni, -(long)fill_pipes[fmt].uni_len },
    };
    umbra_program_queue(
        fill_pipes[fmt].prog, n, buf);
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
    umbra_program_queue(
        readback_pipes[fmt].prog, n, buf);
}

static void render_slide(
        int slide_idx, int fmt,
        struct umbra_program *program,
        void *pixbuf,
        umbra_draw_layout const *lay) {
    slide *s = slide_get(slide_idx);

    int bpp = fmt_bpp[fmt];
    _Bool planar = (fmt == FMT_FP16P);
    int32_t planar_stride = W * H;
    long row_sz = planar
        ? (long)(W * H * 4) * 2
        : (long)(W * bpp);
    int ps = planar ? (s->load ? 2 : 1) : 0;

    #define ROW(y) (planar \
        ? (void*)((__fp16*)pixbuf + (y) * W) \
        : (void*)((uint8_t*)pixbuf             \
                  + (y) * W * bpp))

    for (int y = 0; y < H; y++) {
        fill_bg_row(fmt, ROW(y), W,
                    s->bg, row_sz,
                    planar_stride);
    }
    for (int y = 0; y < H; y++) {
        s->render_row(s, y, W,
            ROW(y), row_sz,
            lay, ps, planar_stride,
            program);
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
        int slide_idx, int fmt) {
    slide *s = slide_get(slide_idx);

    umbra_load_fn  load =
        s->load ? fmt_load[fmt] : NULL;
    umbra_store_fn store = fmt_store[fmt];

    umbra_draw_layout lay;
    struct umbra_builder *bld =
        umbra_draw_build(s->shader, s->coverage,
                         s->blend, load, store,
                         &lay);
    struct umbra_basic_block *bb =
        umbra_basic_block(bld);
    umbra_builder_free(bld);

    struct umbra_backend *bes[N_BACKS] = {
        umbra_backend_interp(),
        umbra_backend_jit(),
        umbra_backend_metal(),
    };
    struct umbra_program *progs[N_BACKS];
    for (int bi = 0; bi < N_BACKS; bi++) {
        progs[bi] = bes[bi]
            ? umbra_backend_compile(bes[bi], bb)
            : NULL;
    }
    umbra_basic_block_free(bb);

    _Bool planar = (fmt == FMT_FP16P);
    size_t pixbuf_sz = planar
        ? (size_t)(W * H * 4) * 2
        : (size_t)(W * H) * (size_t)fmt_bpp[fmt];
    size_t pad = planar
        ? (size_t)((H - 1) * W) * 2 : 0;
    void *pbuf_ref = calloc(1, pixbuf_sz + pad);
    void *pbuf_tst = calloc(1, pixbuf_sz + pad);
    uint32_t *ref = malloc((size_t)(W * H) * 4);
    uint32_t *tst = malloc((size_t)(W * H) * 4);

    render_slide(slide_idx, fmt,
                 progs[0], pbuf_ref, &lay);
    readback_to_8888(fmt, pbuf_ref, ref);

    for (int bi = 1; bi < N_BACKS; bi++) {
        if (!progs[bi]) { continue; }
        render_slide(slide_idx, fmt,
                     progs[bi], pbuf_tst, &lay);
        umbra_program_flush(progs[bi]);
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
    for (int bi = 0; bi < N_BACKS; bi++) {
        umbra_program_free(progs[bi]);
        umbra_backend_free(bes[bi]);
    }
}

static void test_slug_rect(void) {
    static float rect[] = {
         5, 5,  5,20,  5,35,
         5,35, 30,35, 55,35,
        55,35, 55,20, 55, 5,
        55, 5, 30, 5,  5, 5,
    };

    slug_acc_layout alay;
    builder *ab = slug_build_acc(&alay);
    struct umbra_basic_block *abb =
        umbra_basic_block(ab);
    umbra_builder_free(ab);
    struct umbra_backend *be =
        umbra_backend_interp();
    struct umbra_program *acc =
        umbra_backend_compile(be, abb);
    umbra_basic_block_free(abb);

    umbra_draw_layout lay;
    builder *bld = umbra_draw_build(
        umbra_shader_solid, umbra_coverage_wind,
        umbra_blend_srcover, umbra_load_8888,
        umbra_store_8888, &lay);
    struct umbra_basic_block *bb =
        umbra_basic_block(bld);
    umbra_builder_free(bld);
    struct umbra_program *interp =
        umbra_backend_compile(be, bb);
    umbra_basic_block_free(bb);

    uint32_t pixels[W * H];
    for (int i = 0; i < W * H; i++) {
        pixels[i] = 0xff000000;
    }

    float mat[11] = {
        1,0,0, 0,1,0, 0,0,1, 60,40,
    };
    float color[4] = {1,1,1,1};

    float wind_buf[W];
    for (int y = 0; y < H; y++) {
        __builtin_memset(wind_buf, 0,
            sizeof wind_buf);
        long long au_[12] = {0};
        char *au = (char*)au_;
        slide_uni_i32(au, alay.x0, 0);
        slide_uni_i32(au, alay.y, y);
        slide_uni_f32(au, alay.mat, mat, 11);
        slide_uni_ptr(au, alay.curves_off,
            rect, (long)sizeof rect);
        umbra_buf abuf[] = {
            { wind_buf,
              (long)sizeof wind_buf },
            { au, -(long)alay.uni_len },
        };
        for (int j = 0; j < 4; j++) {
            int32_t j32 = j;
            __builtin_memcpy(
                au + alay.loop_off,
                &j32, 4);
            umbra_program_queue(acc, W, abuf);
        }
        umbra_program_flush(acc);

        long long uni_[12] = {0};
        char *uni = (char*)uni_;
        slide_uni_i32(uni, lay.x0, 0);
        slide_uni_i32(uni, lay.y, y);
        slide_uni_f32(uni, lay.shader, color, 4);
        slide_uni_ptr(uni, lay.coverage,
            wind_buf, -(long)sizeof wind_buf);
        umbra_buf buf[] = {
            { pixels + y * W, (long)(W * 4) },
            { uni, -(long)lay.uni_len },
        };
        umbra_program_queue(interp, W, buf);
        umbra_program_flush(interp);
    }

    uint32_t bg = 0xff000000;
    uint32_t fg = 0xffffffff;
    (pixels[20*W + 30] == fg) here;
    (pixels[20*W +  2] == bg) here;
    (pixels[20*W + 58] == bg) here;
    (pixels[ 2*W + 30] == bg) here;
    (pixels[38*W + 30] == bg) here;
    (pixels[20*W + 70] == bg) here;

    umbra_program_free(acc);
    umbra_program_free(interp);
    umbra_backend_free(be);
}

static void test_perspective_text(void) {
    enum { BW = 16, BH = 8 };
    uint16_t bmp[BW * BH];
    __builtin_memset(bmp, 0, sizeof bmp);
    bmp[4 * BW + 8] = 255;

    struct umbra_backend *be =
        umbra_backend_interp();

    umbra_draw_layout lay;
    builder *bld = umbra_draw_build(
        umbra_shader_solid,
        umbra_coverage_bitmap_matrix,
        umbra_blend_srcover, umbra_load_8888,
        umbra_store_8888, &lay);
    struct umbra_basic_block *bb =
        umbra_basic_block(bld);
    umbra_builder_free(bld);
    struct umbra_program *interp =
        umbra_backend_compile(be, bb);
    umbra_basic_block_free(bb);

    uint32_t pixels[BW];
    for (int i = 0; i < BW; i++) {
        pixels[i] = 0xff000000;
    }

    float mat[11] = {
        1,0,0, 0,1,0, 0,0,1,
        (float)BW, (float)BH,
    };
    float color[4] = {1,1,1,1};

    long long uni_[12] = {0};
    char *uni = (char*)uni_;
    slide_uni_i32(uni, lay.x0, 0);
    slide_uni_i32(uni, lay.y, 4);
    slide_uni_f32(uni, lay.shader, color, 4);
    slide_uni_f32(uni, lay.coverage, mat, 11);
    slide_uni_ptr(uni,
        (lay.coverage + 11*4 + 7) & ~7,
        bmp, (long)sizeof bmp);
    umbra_buf buf[] = {
        { pixels, (long)sizeof pixels },
        { uni, -(long)lay.uni_len },
    };
    umbra_program_queue(interp, BW, buf);
    umbra_program_flush(interp);

    (pixels[8] == 0xffffffff) here;
    (pixels[0] == 0xff000000) here;

    umbra_program_free(interp);

    text_cov tc = text_rasterize(W, H, 24.0f, 0);

    umbra_draw_layout lay2;
    bld = umbra_draw_build(
        umbra_shader_solid,
        umbra_coverage_bitmap_matrix,
        umbra_blend_srcover, umbra_load_8888,
        umbra_store_8888, &lay2);
    bb = umbra_basic_block(bld);
    umbra_builder_free(bld);
    interp = umbra_backend_compile(be, bb);
    umbra_basic_block_free(bb);

    uint32_t px2[W * H];
    for (int i = 0; i < W * H; i++) {
        px2[i] = 0xff0a0a1e;
    }
    float mat2[11];
    slide_perspective_matrix(mat2, 1.0f,
        W, H, tc.w, tc.h);
    float hc2[4] = {1,0.8f,0.2f,1};
    for (int y = 0; y < H; y++) {
        long long u2_[12] = {0};
        char *u2 = (char*)u2_;
        slide_uni_i32(u2, lay2.x0, 0);
        slide_uni_i32(u2, lay2.y, y);
        slide_uni_f32(u2, lay2.shader, hc2, 4);
        slide_uni_f32(u2, lay2.coverage, mat2, 11);
        slide_uni_ptr(u2,
            (lay2.coverage + 11*4 + 7) & ~7,
            tc.data,
            (long)(W * H * 2));
        umbra_buf b2[] = {
            { px2 + y * W, (long)(W * 4) },
            { u2, -(long)lay2.uni_len },
        };
        umbra_program_queue(interp, W, b2);
        umbra_program_flush(interp);
    }
    int changed = 0;
    for (int i = 0; i < W * H; i++) {
        if (px2[i] != 0xff0a0a1e) { changed++; }
    }
    (changed > 0) here;

    umbra_program_free(interp);
    umbra_backend_free(be);
    text_cov_free(&tc);
}

int main(void) {
    slides_init(W, H);
    build_pipes();

    test_perspective_text();
    test_slug_rect();

    for (int fi = 0; fi < NUM_FMTS; fi++) {
        for (int si = 0; si < slide_count() - 1; si++) {
            test_slide_golden(si, fi);
        }
    }

    free_pipes();
    slides_cleanup();
    return 0;
}
