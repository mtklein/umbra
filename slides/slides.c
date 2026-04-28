#include "slide.h"
#include "../src/assume.h"
#include "../src/count.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

void slide_perspective_matrix(struct umbra_matrix *out, float t,
                              int sw, int sh, int bw, int bh) {
    float const cx    = (float)sw * 0.5f,
                cy    = (float)sh * 0.5f,
                bx    = (float)bw * 0.5f,
                by    = (float)bh * 0.5f,
                angle = t * 0.3f,
                tilt  = sinf(t * 0.7f) * 0.0008f,
                sc    = 1.0f + 0.2f * sinf(t * 0.5f),
                ca    = cosf(angle),
                sa    = sinf(angle),
                w0    = 1.0f - tilt * cx;
    *out = (struct umbra_matrix){
        .sx = ca * sc,  .kx = sa * sc,
        .tx = -cx*ca*sc - cy*sa*sc + bx*w0,
        .ky = -sa * sc, .sy = ca * sc,
        .ty = cx*sa*sc - cy*ca*sc + by*w0,
        .p0 = tilt, .p1 = 0.0f, .p2 = w0,
    };
}

void slide_affine_matrix(struct umbra_affine *out, float t,
                         int sw, int sh, int bw, int bh) {
    float const cx    = (float)sw * 0.5f,
                cy    = (float)sh * 0.5f,
                bx    = (float)bw * 0.5f,
                by    = (float)bh * 0.5f,
                angle = t * 0.3f,
                sc    = 1.0f + 0.2f * sinf(t * 0.5f),
                ca    = cosf(angle),
                sa    = sinf(angle);
    *out = (struct umbra_affine){
        .sx = ca * sc,  .kx = sa * sc,
        .tx = -cx*ca*sc - cy*sa*sc + bx,
        .ky = -sa * sc, .sy = ca * sc,
        .ty = cx*sa*sc - cy*ca*sc + by,
    };
}

_Bool slide_sdf_stroke_enabled = 0;
float slide_sdf_stroke_width   = 3.0f;

void slide_effective_sdf(struct slide *s, umbra_sdf **out_fn, void **out_ctx) {
    if (slide_sdf_stroke_enabled) {
        s->sdf_stroke = (struct umbra_sdf_stroke){
            .inner_fn  = s->sdf_fn,
            .inner_ctx = s->sdf_ctx,
            .width     = slide_sdf_stroke_width,
        };
        *out_fn  = umbra_sdf_stroke;
        *out_ctx = &s->sdf_stroke;
    } else {
        *out_fn  = s->sdf_fn;
        *out_ctx = s->sdf_ctx;
    }
}

enum { MAX_SLIDES = 32 };

struct slide* make_overview(void);

static slide_factory_fn registry[MAX_SLIDES];
static int              registry_count;

static struct slide *all[MAX_SLIDES];
static int           count;

void slide_register(slide_factory_fn factory) {
    assume(registry_count < MAX_SLIDES);
    registry[registry_count++] = factory;
}

int           slide_count(void) { return count; }
struct slide* slide_get(int i)  { return all[i]; }

static void add_slide(struct slide *s, int w, int h) {
    all[count] = s;
    s->w = w;
    s->h = h;
    if (s->init) { s->init(s); }
    count++;
}

void slides_init(int w, int h) {
    count = 0;
    for (int i = 0; i < registry_count; i++) {
        add_slide(registry[i](), w, h);
    }
    add_slide(make_overview(), w, h);
}

void slides_cleanup(void) {
    for (int i = 0; i < count; i++) {
        if (all[i]->free) { all[i]->free(all[i]); }
    }
    count = 0;
}

struct slide_bg {
    struct umbra_program *prog;
    umbra_color           color;
    struct umbra_buf      dst_buf;
};

struct slide_bg* slide_bg(struct umbra_backend *be, struct umbra_fmt fmt) {
    struct slide_bg *bg = calloc(1, sizeof *bg);

    struct umbra_builder *b = umbra_builder();
    umbra_ptr   const dst = umbra_bind_sealed(b, &bg->dst_buf);
    umbra_val32 const x   = umbra_f32_from_i32(b, umbra_x(b)),
                      y   = umbra_f32_from_i32(b, umbra_y(b));
    umbra_build_draw(b, dst, fmt, x, y,
                     NULL,               NULL,
                     umbra_shader_color, &bg->color,
                     NULL,               NULL);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    bg->prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);
    return bg;
}

void slide_bg_draw(struct slide_bg *bg, umbra_color color,
                   int l, int t, int r, int b, struct umbra_buf dst) {
    bg->color   = color;
    bg->dst_buf = dst;
    bg->prog->queue(bg->prog, l, t, r, b, NULL, 0);
}

void slide_bg_free(struct slide_bg *bg) {
    if (bg) {
        umbra_program_free(bg->prog);
        free(bg);
    }
}

struct umbra_builder* slide_draw_builder(struct slide *s,
                                         umbra_ptr *out_dst_ptr,
                                         struct umbra_fmt fmt,
                                         union transform const *pre) {
    if (s->build_draw || s->build_sdf_draw) {
        struct umbra_builder *b = umbra_builder();
        umbra_ptr const dst_ptr = umbra_bind_sealed(b, NULL);
        if (out_dst_ptr) { *out_dst_ptr = dst_ptr; }
        umbra_val32 x = umbra_f32_from_i32(b, umbra_x(b)),
                    y = umbra_f32_from_i32(b, umbra_y(b));
        if (pre) {
            umbra_point_val32 const p = umbra_transform_perspective(&pre->persp, b, x, y);
            x = p.x;
            y = p.y;
        }
        if (s->build_sdf_draw) { s->build_sdf_draw(s, b, dst_ptr, fmt, x, y); }
        else                   { s->build_draw    (s, b, dst_ptr, fmt, x, y); }
        return b;
    }
    return NULL;
}

static struct umbra_builder* slide_draw_full_builder(struct slide *s,
                                                     struct umbra_fmt fmt,
                                                     union transform const *pre) {
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_sealed(b, NULL);
    umbra_val32 x = umbra_f32_from_i32(b, umbra_x(b)),
                y = umbra_f32_from_i32(b, umbra_y(b));
    if (pre) {
        umbra_point_val32 const p = umbra_transform_perspective(&pre->persp, b, x, y);
        x = p.x;
        y = p.y;
    }
    s->build_draw_full(s, b, dst_ptr, fmt, x, y);
    return b;
}

struct slide_runtime* slide_runtime(struct slide *s,
                                    int w, int h,
                                    struct umbra_backend *be, struct umbra_fmt fmt,
                                    union transform const *pre) {
    struct slide_runtime *rt = calloc(1, sizeof *rt);
    rt->fmt = fmt;
    rt->w   = w;
    rt->h   = h;

    struct umbra_builder *b = slide_draw_builder(s, &rt->dst_ptr, fmt, pre);
    if (b) {
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        rt->draw = be->compile(be, ir);
        umbra_flat_ir_free(ir);
    }

    if (s->build_sdf_draw) {
        struct umbra_builder *bf = slide_draw_full_builder(s, fmt, pre);
        struct umbra_flat_ir *irf = umbra_flat_ir(bf);
        umbra_builder_free(bf);
        rt->draw_full = be->compile(be, irf);
        umbra_flat_ir_free(irf);

        umbra_sdf *bfn;
        void      *bctx;
        slide_effective_sdf(s, &bfn, &bctx);
        struct umbra_builder *bb = umbra_builder();
        rt->bounds = umbra_sdf_bounds_program(bb, pre ? &pre->affine : NULL,
                                              bfn, bctx);
        umbra_builder_free(bb);
    }
    return rt;
}

void slide_runtime_animate(struct slide *s, double secs) {
    if (s->animate) { s->animate(s, secs); }
}

void slide_runtime_draw(struct slide_runtime *rt, struct umbra_buf dst,
                        int l, int t, int r, int b) {
    struct umbra_late_binding const lates[] = {
        {.ptr = rt->dst_ptr, .buf = dst},
    };
    if (rt->bounds) {
        umbra_sdf_dispatch(rt->bounds, rt->draw, rt->draw_full, l, t, r, b,
                           lates, count(lates));
    } else {
        rt->draw->queue(rt->draw, l, t, r, b, lates, count(lates));
    }
}

void slide_runtime_free(struct slide_runtime *rt) {
    if (rt) {
        umbra_program_free(rt->draw);
        umbra_program_free(rt->draw_full);
        umbra_sdf_bounds_program_free(rt->bounds);
        free(rt);
    }
}
