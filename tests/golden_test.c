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
static umbra_fmt const fmt_enums[] = {
    umbra_fmt_8888, umbra_fmt_565, umbra_fmt_fp16,
    umbra_fmt_fp16_planar, umbra_fmt_1010102,
};

typedef struct {
    struct umbra_program  *prog;
    struct umbra_uniforms *uni;
    int                    out_ptr, pad_;
} pipe;

static pipe fill_pipes[NUM_FMTS];
static struct umbra_backend *interp_be;

static void build_fill(int fmt) {
    builder               *builder = umbra_builder();
    struct umbra_uniforms *u       = calloc(1, sizeof(struct umbra_uniforms));
    size_t fi = umbra_reserve_f32(u, 4);
    umbra_color c = {
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi),
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4),
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 8),
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 12),
    };
    umbra_store_color(builder, (umbra_ptr){1, 0}, c, fmt_enums[fmt]);
    fill_pipes[fmt].uni = u;
    struct umbra_basic_block *opt =
        umbra_basic_block(builder);
    umbra_builder_free(builder);
    fill_pipes[fmt].prog =
        interp_be->compile(interp_be, opt);
    umbra_basic_block_free(opt);
}


static void build_pipes(void) {
    interp_be = umbra_backend_interp();
    for (int f = 0; f < NUM_FMTS; f++) {
        build_fill(f);
    }
}

static void free_pipes(void) {
    for (int f = 0; f < NUM_FMTS; f++) {
        fill_pipes[f].prog->free(fill_pipes[f].prog);
        if (fill_pipes[f].uni) { free(fill_pipes[f].uni->data); free(fill_pipes[f].uni); }
    }
    interp_be->free(interp_be);
}

static size_t pixbuf_size(int fmt) {
    size_t bpp = umbra_fmt_size(fmt_enums[fmt]);
    size_t planes = (fmt_enums[fmt] == umbra_fmt_fp16_planar) ? 4 : 1;
    return (size_t)W * (size_t)H * bpp * planes;
}

static void fill_bg(int fmt, void *dst, uint32_t bg) {
    float hc[4] = {
        (float)( bg        & 0xffu) / 255.0f,
        (float)((bg >>  8) & 0xffu) / 255.0f,
        (float)((bg >> 16) & 0xffu) / 255.0f,
        (float)((bg >> 24) & 0xffu) / 255.0f,
    };
    umbra_set_f32(fill_pipes[fmt].uni, 0, hc, 4);
    size_t bpp = umbra_fmt_size(fmt_enums[fmt]);
    struct umbra_buf buf[2] = {
        (struct umbra_buf){.ptr=fill_pipes[fmt].uni->data, .sz=fill_pipes[fmt].uni->size, .read_only=1},
        {.ptr=dst, .sz=pixbuf_size(fmt), .row_bytes=(size_t)W * bpp},
    };
    fill_pipes[fmt].prog->queue(fill_pipes[fmt].prog, 0, 0, W, H, buf);
}


static void render_slide(
        int slide_idx, int fmt,
        struct umbra_backend *be,
        struct umbra_program *program,
        void *pixbuf,
        umbra_draw_layout const *lay) {
    slide *s = slide_get(slide_idx);

    fill_bg(fmt, pixbuf, s->bg);
    if (s->prepare) { s->prepare(s, W, H, be); }
    s->draw(s, W, H, 0, H, pixbuf,
            lay, program);
}

static void test_slide_golden(
        int slide_idx, int fmt) {
    slide *s = slide_get(slide_idx);


    umbra_fmt saved_fmt = s->fmt;
    s->fmt = fmt_enums[fmt];

    umbra_draw_layout lay;
    struct umbra_builder *bld =
        umbra_draw_build(s->shader, s->coverage,
                         s->blend, fmt_enums[fmt],
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
            ? bes[bi]->compile(bes[bi], bb)
            : NULL;
    }
    umbra_basic_block_free(bb);

    size_t pixbuf_sz = pixbuf_size(fmt);
    void *pbuf_ref = calloc(1, pixbuf_sz);
    void *pbuf_tst = calloc(1, pixbuf_sz);

    render_slide(slide_idx, fmt,
                 bes[0], progs[0], pbuf_ref, &lay);

    for (int bi = 1; bi < N_BACKS; bi++) {
        if (!progs[bi]) { continue; }
        render_slide(slide_idx, fmt,
                     bes[bi], progs[bi], pbuf_tst, &lay);
        bes[bi]->flush(bes[bi]);

        int mismatches = 0;
        int worst = 0;
        uint8_t const *r = pbuf_ref, *t = pbuf_tst;
        int npx = W * H;
        if (fmt_enums[fmt] == umbra_fmt_fp16_planar) npx *= 4;
        for (int i = 0; i < npx; i++) {
            _Bool differ = 0;
            switch (fmt_enums[fmt]) {
            case umbra_fmt_8888: {
                uint32_t rp, tp;
                __builtin_memcpy(&rp, r+i*4, 4);
                __builtin_memcpy(&tp, t+i*4, 4);
                for (int ch = 0; ch < 4; ch++) {
                    int d = (int)((rp>>(ch*8))&0xFF) - (int)((tp>>(ch*8))&0xFF);
                    if (d<0) d=-d;
                    if (d>worst) worst=d;
                    if (d) differ=1;
                }
            } break;
            case umbra_fmt_565: {
                uint16_t rp, tp;
                __builtin_memcpy(&rp, r+i*2, 2);
                __builtin_memcpy(&tp, t+i*2, 2);
                int dr = (int)(rp>>11)    - (int)(tp>>11);
                int dg = (int)((rp>>5)&63)- (int)((tp>>5)&63);
                int db = (int)(rp&31)     - (int)(tp&31);
                if (dr<0) dr=-dr; if (dg<0) dg=-dg; if (db<0) db=-db;
                if (dr>worst) worst=dr;
                if (dg>worst) worst=dg;
                if (db>worst) worst=db;
                if (rp!=tp) differ=1;
            } break;
            case umbra_fmt_1010102: {
                uint32_t rp, tp;
                __builtin_memcpy(&rp, r+i*4, 4);
                __builtin_memcpy(&tp, t+i*4, 4);
                int d0=(int)(rp&0x3FF)     -(int)(tp&0x3FF);
                int d1=(int)((rp>>10)&0x3FF)-(int)((tp>>10)&0x3FF);
                int d2=(int)((rp>>20)&0x3FF)-(int)((tp>>20)&0x3FF);
                int d3=(int)(rp>>30)       -(int)(tp>>30);
                if(d0<0)d0=-d0;if(d1<0)d1=-d1;if(d2<0)d2=-d2;if(d3<0)d3=-d3;
                if(d0>worst)worst=d0;if(d1>worst)worst=d1;
                if(d2>worst)worst=d2;if(d3>worst)worst=d3;
                if(rp!=tp) differ=1;
            } break;
            case umbra_fmt_fp16:
            case umbra_fmt_fp16_planar: {
                uint16_t rp, tp;
                int nb = (fmt_enums[fmt]==umbra_fmt_fp16) ? 4 : 1;
                for (int ch=0; ch<nb; ch++) {
                    __builtin_memcpy(&rp, r+i*nb*2+ch*2, 2);
                    __builtin_memcpy(&tp, t+i*nb*2+ch*2, 2);
                    int d = (int)rp - (int)tp;
                    if (d<0) d=-d;
                    if (d>worst) worst=d;
                    if (rp!=tp) differ=1;
                }
            } break;
            }
            if (differ) mismatches++;
        }
        int tol = 0;
        if (worst > tol) {
            dprintf(2,
                "slide %d \"%s\" %s/%s: "
                "%d/%d pixels differ, "
                "worst channel delta = %d\n",
                slide_idx + 1, s->title,
                backend_name[bi], fmt_name[fmt],
                mismatches, W * H, worst);
        }
        (worst <= tol) here;
    }

    free(pbuf_ref);
    free(pbuf_tst);
    for (int bi = 0; bi < N_BACKS; bi++) {
        if (progs[bi]) { progs[bi]->free(progs[bi]); }
        if (bes[bi]) { bes[bi]->free(bes[bi]); }
    }
    if (lay.uni) { free(lay.uni->data); free(lay.uni); }
    s->fmt = saved_fmt;
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
        be->compile(be, abb);
    umbra_basic_block_free(abb);

    umbra_draw_layout lay;
    builder *bld = umbra_draw_build(
        umbra_shader_solid, umbra_coverage_wind,
        umbra_blend_srcover, umbra_fmt_8888,
        &lay);
    struct umbra_basic_block *bb =
        umbra_basic_block(bld);
    umbra_builder_free(bld);
    struct umbra_program *interp =
        be->compile(be, bb);
    umbra_basic_block_free(bb);

    uint32_t pixels[W * H];
    for (int i = 0; i < W * H; i++) {
        pixels[i] = 0xff000000;
    }

    float mat[11] = {
        1,0,0, 0,1,0, 0,0,1, 60,40,
    };
    float color[4] = {1,1,1,1};

    float wind_buf[W * H];
    __builtin_memset(wind_buf, 0, sizeof wind_buf);
    umbra_set_f32(alay.uni, alay.mat, mat, 11);
    umbra_set_ptr(alay.uni, alay.curves_off,
        rect, sizeof rect, 0, 0);
    struct umbra_buf abuf[] = {
        (struct umbra_buf){.ptr=alay.uni->data, .sz=alay.uni->size, .read_only=1},
        {.ptr=wind_buf, .sz=sizeof wind_buf, .row_bytes=W * sizeof(float)},
    };
    for (int j = 0; j < 4; j++) {
        float jf;
        int32_t j32 = j;
        __builtin_memcpy(&jf, &j32, 4);
        umbra_set_f32(alay.uni, alay.loop_off, &jf, 1);
        abuf[0] = (struct umbra_buf){.ptr=alay.uni->data, .sz=alay.uni->size, .read_only=1};
        acc->queue(acc, 0, 0, W, H, abuf);
    }
    be->flush(be);

    umbra_set_f32(lay.uni, lay.shader, color, 4);
    umbra_set_ptr(lay.uni, lay.coverage,
        wind_buf, sizeof wind_buf, 1, (size_t)W * sizeof(float));
    struct umbra_buf buf[] = {
        (struct umbra_buf){.ptr=lay.uni->data, .sz=lay.uni->size, .read_only=1},
        {.ptr=pixels, .sz=sizeof pixels, .row_bytes=W * 4},
    };
    interp->queue(interp, 0, 0, W, H, buf);
    be->flush(be);

    uint32_t bg = 0xff000000;
    uint32_t fg = 0xffffffff;
    pixels[20*W + 30] == fg here;
    pixels[20*W +  2] == bg here;
    pixels[20*W + 58] == bg here;
    pixels[ 2*W + 30] == bg here;
    pixels[38*W + 30] == bg here;
    pixels[20*W + 70] == bg here;

    if (lay.uni) { free(lay.uni->data); free(lay.uni); }
    if (alay.uni) { free(alay.uni->data); free(alay.uni); }
    acc->free(acc);
    interp->free(interp);
    be->free(be);
}

static void test_perspective_text(void) {
    enum { BW = 16, BH = 8 };
    uint16_t bmp[BW * BH];
    __builtin_memset(bmp, 0, sizeof bmp);
    bmp[0 * BW + 8] = 255;

    struct umbra_backend *be =
        umbra_backend_interp();

    umbra_draw_layout lay;
    builder *bld = umbra_draw_build(
        umbra_shader_solid,
        umbra_coverage_bitmap_matrix,
        umbra_blend_srcover, umbra_fmt_8888,
        &lay);
    struct umbra_basic_block *bb =
        umbra_basic_block(bld);
    umbra_builder_free(bld);
    struct umbra_program *interp =
        be->compile(be, bb);
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

    umbra_set_f32(lay.uni, lay.shader, color, 4);
    umbra_set_f32(lay.uni, lay.coverage, mat, 11);
    umbra_set_ptr(lay.uni, (lay.coverage + 44 + 7) & ~(size_t)7,
        bmp, sizeof bmp, 0, 0);
    struct umbra_buf buf[] = {
        (struct umbra_buf){.ptr=lay.uni->data, .sz=lay.uni->size, .read_only=1},
        {.ptr=pixels, .sz=sizeof pixels},
    };
    interp->queue(interp, 0, 0, BW, 1, buf);
    be->flush(be);

    pixels[8] == 0xffffffff here;
    pixels[0] == 0xff000000 here;

    interp->free(interp);

    text_cov tc = text_rasterize(W, H, 24.0f, 0);

    umbra_draw_layout lay2;
    bld = umbra_draw_build(
        umbra_shader_solid,
        umbra_coverage_bitmap_matrix,
        umbra_blend_srcover, umbra_fmt_8888,
        &lay2);
    bb = umbra_basic_block(bld);
    umbra_builder_free(bld);
    interp = be->compile(be, bb);
    umbra_basic_block_free(bb);

    uint32_t px2[W * H];
    for (int i = 0; i < W * H; i++) {
        px2[i] = 0xff0a0a1e;
    }
    float mat2[11];
    slide_perspective_matrix(mat2, 1.0f,
        W, H, tc.w, tc.h);
    float hc2[4] = {1,0.8f,0.2f,1};
    {
        umbra_set_f32(lay2.uni, lay2.shader, hc2, 4);
        umbra_set_f32(lay2.uni, lay2.coverage, mat2, 11);
        umbra_set_ptr(lay2.uni, (lay2.coverage + 44 + 7) & ~(size_t)7,
            tc.data,
            (size_t)(W * H * 2), 0, 0);
        struct umbra_buf b2[] = {
            (struct umbra_buf){.ptr=lay2.uni->data, .sz=lay2.uni->size, .read_only=1},
            {.ptr=px2, .sz=(size_t)(W * H * 4), .row_bytes=W * 4},
        };
        interp->queue(interp, 0, 0, W, H, b2);
        be->flush(be);
    }
    int changed = 0;
    for (int i = 0; i < W * H; i++) {
        if (px2[i] != 0xff0a0a1e) { changed++; }
    }
    changed > 0 here;

    if (lay.uni) { free(lay.uni->data); free(lay.uni); }
    if (lay2.uni) { free(lay2.uni->data); free(lay2.uni); }
    interp->free(interp);
    be->free(be);
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
