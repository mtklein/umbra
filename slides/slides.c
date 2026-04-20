#include "slide.h"
#include "../src/assume.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

void slide_perspective_matrix(struct umbra_matrix *out, float t,
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
    *out = (struct umbra_matrix){
        .sx = ca * sc,  .kx = sa * sc,
        .tx = -cx*ca*sc - cy*sa*sc + bx*w0,
        .ky = -sa * sc, .sy = ca * sc,
        .ty = cx*sa*sc - cy*ca*sc + by*w0,
        .p0 = tilt, .p1 = 0.0f, .p2 = w0,
    };
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
    umbra_ptr   const dst = umbra_bind_buf(b, &bg->dst_buf);
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
    bg->prog->queue(bg->prog, l, t, r, b);
}

void slide_bg_free(struct slide_bg *bg) {
    if (bg) {
        umbra_program_free(bg->prog);
        free(bg);
    }
}

struct umbra_builder* slide_draw_builder(struct slide *s,
                                         struct umbra_buf *dst,
                                         struct umbra_fmt fmt,
                                         union transform const *pre) {
    if (s->build_draw || s->build_sdf_draw) {
        struct umbra_builder *b = umbra_builder();
        umbra_ptr const dst_ptr = umbra_bind_buf(b, dst);
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

struct slide_runtime* slide_runtime(struct slide *s,
                                    int w, int h,
                                    struct umbra_backend *be, struct umbra_fmt fmt,
                                    union transform const *pre) {
    struct slide_runtime *rt = calloc(1, sizeof *rt);
    rt->fmt = fmt;
    rt->w   = w;
    rt->h   = h;

    struct umbra_builder *b = slide_draw_builder(s, &rt->dst_buf, fmt, pre);
    if (b) {
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        rt->draw = be->compile(be, ir);
        umbra_flat_ir_free(ir);
    }

    if (s->build_sdf_draw) {
        struct umbra_builder *bb = umbra_builder();
        rt->bounds = umbra_sdf_bounds_program(bb, pre ? &pre->affine : NULL,
                                              s->sdf_fn, s->sdf_ctx);
        umbra_builder_free(bb);
    }
    return rt;
}

void slide_runtime_draw(struct slide_runtime *rt, struct slide *s,
                        double secs, int l, int t, int r, int b) {
    if (s->animate) { s->animate(s, secs); }

    if (rt->bounds) {
        umbra_sdf_dispatch(rt->bounds, rt->draw, l, t, r, b);
    } else {
        rt->draw->queue(rt->draw, l, t, r, b);
    }
}

void slide_runtime_free(struct slide_runtime *rt) {
    if (rt) {
        umbra_program_free(rt->draw);
        umbra_sdf_bounds_program_free(rt->bounds);
        free(rt);
    }
}
