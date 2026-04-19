#include "slide.h"
#include "text.h"
#include <math.h>
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
    if (registry_count < MAX_SLIDES) {
        registry[registry_count++] = factory;
    }
}

int           slide_count(void) { return count; }
struct slide* slide_get(int i)  { return all[i]; }

static void add_slide(struct slide *s, int w, int h) {
    all[count] = s;
    if (s->init) { s->init(s, w, h); }
    count++;
}

void slides_init(int w, int h) {
    text_shared_init(w, h, (float)h * 0.15f);

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
    text_shared_cleanup();
    slide_bg_cleanup();
    count = 0;
}

static struct umbra_flat_ir    *bg_ir;
static struct umbra_program    *bg_prog;
static umbra_color              bg_color;
static struct umbra_fmt         bg_fmt;
static int                      bg_w, bg_h;
static struct umbra_buf         bg_dst_buf;

void slide_bg_prepare(struct umbra_backend *be, struct umbra_fmt fmt, int w, int h) {
    if (bg_fmt.name != fmt.name || !bg_ir || bg_w != w || bg_h != h) {
        umbra_flat_ir_free(bg_ir);
        bg_fmt = fmt;
        bg_w   = w;
        bg_h   = h;
        struct umbra_builder *b = umbra_draw_builder(
        NULL, NULL,            NULL,               NULL,
            umbra_shader_color, &bg_color,
            NULL,               NULL,
            &bg_dst_buf,        fmt);
        bg_ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    umbra_program_free(bg_prog);
    bg_prog = be->compile(be, bg_ir);
}

void slide_bg_draw(float const bg[4], int l, int t, int r, int b, void *buf) {
    bg_color = (umbra_color){bg[0], bg[1], bg[2], bg[3]};
    bg_dst_buf = (struct umbra_buf){
        .ptr=buf, .count=bg_w * bg_h * bg_fmt.planes, .stride=bg_w,
    };
    bg_prog->queue(bg_prog, l, t, r, b);
}

void slide_bg_cleanup(void) {
    if (bg_prog) { umbra_program_free(bg_prog); bg_prog = NULL; }
    umbra_flat_ir_free(bg_ir); bg_ir = NULL;
    bg_fmt = (struct umbra_fmt){0};
    bg_w = bg_h = 0;
}
