#include "../include/umbra.h"
#include "../slides/slide.h"
#include "../third_party/stb/stb_image_write.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { W = 640, H = 480 };

static void render_slide(char const *label, struct umbra_backend *be, struct slide *s) {
    s->fmt = umbra_fmt_8888;
    int bpp = 4;
    size_t row_sz = (size_t)(W * bpp);

    struct umbra_builder *fb = umbra_builder();
    struct umbra_uniforms *fill_uni = calloc(1, sizeof(struct umbra_uniforms));
    size_t fi = umbra_uniforms_reserve_f32(fill_uni, 4);
    umbra_color fc = {
        umbra_uniform_32(fb, (umbra_ptr){0}, fi),
        umbra_uniform_32(fb, (umbra_ptr){0}, fi + 4),
        umbra_uniform_32(fb, (umbra_ptr){0}, fi + 8),
        umbra_uniform_32(fb, (umbra_ptr){0}, fi + 12),
    };
    umbra_store_color(fb, (umbra_ptr){1}, fc, umbra_fmt_8888);
    struct umbra_basic_block *fbb = umbra_basic_block(fb);
    umbra_builder_free(fb);
    struct umbra_program *fill_prog = be->compile(be, fbb);
    umbra_basic_block_free(fbb);

    struct umbra_builder *rb = umbra_builder();
    umbra_color rc = umbra_load_color(rb, (umbra_ptr){1}, umbra_fmt_8888);
    umbra_store_color(rb, (umbra_ptr){2}, rc, umbra_fmt_8888);
    struct umbra_basic_block *rbb = umbra_basic_block(rb);
    umbra_builder_free(rb);
    struct umbra_program *rb_prog = be->compile(be, rbb);
    umbra_basic_block_free(rbb);

    void *pixbuf = calloc(1, (size_t)(W * H * bpp));

    float hc[4] = {
        (float)(s->bg & 0xFFu) / 255.0f,
        (float)((s->bg >> 8) & 0xFFu) / 255.0f,
        (float)((s->bg >> 16) & 0xFFu) / 255.0f,
        (float)((s->bg >> 24) & 0xFFu) / 255.0f,
    };
    umbra_uniforms_fill_f32(fill_uni, 0, hc, 4);
    for (int y = 0; y < H; y++) {
        void *row = (char*)pixbuf + y * W * bpp;
        struct umbra_buf buf[] = {
            (struct umbra_buf){.ptr=fill_uni->data, .sz=fill_uni->size, .read_only=1},
            {.ptr=row, .sz=row_sz},
        };
        fill_prog->queue(fill_prog, 0, 0, W, 1, buf);
    }
    be->flush(be);

    s->init(s, W, H);
    if (s->animate) s->animate(s, 0.016f);
    s->prepare(s, W, H, be);
    s->draw(s, W, H, 0, H, pixbuf);
    be->flush(be);

    uint32_t *rgba = calloc((size_t)(W * H), 4);
    for (int y = 0; y < H; y++) {
        void *src = (char*)pixbuf + y * W * bpp;
        struct umbra_buf buf[] = {
            {0},
            {.ptr=src, .sz=row_sz, .read_only=1},
            {.ptr=rgba + y * W, .sz=(size_t)(W*4)},
        };
        rb_prog->queue(rb_prog, 0, 0, W, 1, buf);
    }
    be->flush(be);

    char path[256];
    snprintf(path, sizeof path, "/tmp/srgb_%s.png", label);
    stbi_write_png(path, W, H, 4, rgba, W * 4);
    printf("Wrote %s — row 0 x=0: R=%d  x=32: R=%d  x=64: R=%d\n",
           path, rgba[0]&0xFF, rgba[32]&0xFF, rgba[64]&0xFF);

    free(pixbuf); free(rgba);
    fill_prog->free(fill_prog);
    rb_prog->free(rb_prog);
    if (fill_uni) { free(fill_uni->data); free(fill_uni); }
}

int main(void) {
    slides_init(W, H);
    struct slide *s = slide_get(0);

    struct umbra_backend *interp = umbra_backend_interp();
    struct umbra_backend *jit    = umbra_backend_jit();
    struct umbra_backend *metal  = umbra_backend_metal();

    render_slide("interp", interp, s);
    if (jit)   render_slide("jit", jit, s);
    if (metal) render_slide("metal", metal, s);

    interp->free(interp);
    if (jit)   jit->free(jit);
    if (metal) metal->free(metal);
    slides_cleanup();

    open("/tmp/srgb_interp.png", 0); // just to trigger open
    return 0;
}
