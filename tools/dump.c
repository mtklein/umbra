#include "../include/umbra_draw.h"
#include "../slides/slide.h"
#include "../slides/slug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_image_write.h"
#pragma clang diagnostic pop

// Write to path~ then rename for atomicity — avoids half-written dumps
// visible in git diff when building in the same worktree.
static FILE* atomic_open(char const *path) {
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%s~", path);
    return fopen(tmp, "w");
}
static void atomic_close(FILE *f, char const *path) {
    fclose(f);
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%s~", path);
    rename(tmp, path);
}

static struct umbra_builder *build_srcover(void) {
    struct umbra_builder *b = umbra_builder();

    umbra_val32 px = umbra_load_32(b, (umbra_ptr32){0}), mask = umbra_imm_i32(b, 0xFF);
    umbra_val32 rgba[4] = {
        umbra_and_32(b, px, mask),
        umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 8)), mask),
        umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 16)), mask),
        umbra_shr_u32(b, px, umbra_imm_i32(b, 24)),
    };

    umbra_val32 inv255 = umbra_imm_f32(b, 1.0f / 255.0f);
    umbra_val32 sr = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[0]), inv255),
              sg = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[1]), inv255),
              sb = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[2]), inv255),
              sa = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[3]), inv255),
              dr = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr16){.ix=1})),
              dg = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr16){.ix=2})),
              db = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr16){.ix=3})),
              da = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr16){.ix=4})),
              one = umbra_imm_f32(b, 1.0f), inv_a = umbra_sub_f32(b, one, sa),
              rout = umbra_add_f32(b, sr, umbra_mul_f32(b, dr, inv_a)),
              gout = umbra_add_f32(b, sg, umbra_mul_f32(b, dg, inv_a)),
              bout = umbra_add_f32(b, sb, umbra_mul_f32(b, db, inv_a)),
              aout = umbra_add_f32(b, sa, umbra_mul_f32(b, da, inv_a));
    umbra_store_16(b, (umbra_ptr16){.ix=1}, umbra_f16_from_f32(b, rout));
    umbra_store_16(b, (umbra_ptr16){.ix=2}, umbra_f16_from_f32(b, gout));
    umbra_store_16(b, (umbra_ptr16){.ix=3}, umbra_f16_from_f32(b, bout));
    umbra_store_16(b, (umbra_ptr16){.ix=4}, umbra_f16_from_f32(b, aout));
    return b;
}

#if defined(__aarch64__)
#define JIT_EXT "arm64"
#elif defined(__AVX2__)
#define JIT_EXT "avx2"
#endif

static void dump_bb(char const *dir, char const *name, struct umbra_flat_ir *bb) {
    char p[128];
    {
        snprintf(p, sizeof p, "%s/%s.ir", dir, name);
        FILE *f = atomic_open(p);
        umbra_flat_ir_dump(bb, f);
        atomic_close(f, p);
    }

    struct umbra_backend *bes[] = {
#ifdef JIT_EXT
        umbra_backend_jit(),
#endif
        umbra_backend_metal(),
        umbra_backend_vulkan(),
        umbra_backend_wgpu(),
    };
    int nb = (int)(sizeof bes / sizeof bes[0]);
    struct umbra_program *progs[sizeof bes / sizeof bes[0]];
    for (int i = 0; i < nb; i++) {
        progs[i] = bes[i] ? bes[i]->compile(bes[i], bb) : NULL;
    }
    char const *exts[] = {
#ifdef JIT_EXT
        JIT_EXT,
#endif
        "metal",
        "vulkan",
        "wgpu",
    };
    for (int i = 0; i < nb; i++) {
        if (!progs[i]) { continue; }
        snprintf(p, sizeof p, "%s/%s.%s", dir, name, exts[i]);
        FILE *f = atomic_open(p);
        if (progs[i]->dump) { progs[i]->dump(progs[i], f); }
        atomic_close(f, p);
        progs[i]->free(progs[i]);
    }
    for (int i = 0; i < (int)(sizeof bes / sizeof bes[0]); i++) {
        if (bes[i]) { bes[i]->free(bes[i]); }
    }
}

static void dump_builder(char const *dir, char const *name, struct umbra_builder *b) {
    char p[128];
    snprintf(p, sizeof p, "%s/%s.builder", dir, name);
    FILE *f = atomic_open(p);
    umbra_builder_dump(b, f);
    atomic_close(f, p);

    struct umbra_flat_ir *bb = umbra_flat_ir(b);
    umbra_builder_free(b);
    dump_bb(dir, name, bb);
    umbra_flat_ir_free(bb);
}

static void slugify(char const *title, char *out, size_t sz) {
    int n = snprintf(out, sz, "dumps/");
    for (int i = 0; title[i] && n < (int)sz - 1; i++) {
        char c = title[i];
        if (c >= 'A' && c <= 'Z') {
            out[n++] = (char)(c + 32);
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out[n++] = c;
        } else if (n > 0 && out[n - 1] != '_') {
            out[n++] = '_';
        }
    }
    while (n > 0 && out[n - 1] == '_') { n--; }
    out[n] = '\0';
}

enum { RW = 128, RH = 96 };

static void fp16p_to_float(float *out, void const *pixbuf) {
    __fp16 const *src = pixbuf;
    int const ps = RW * RH;
    for (int i = 0; i < ps; i++) {
        out[4 * i + 0] = (float)src[i];
        out[4 * i + 1] = (float)src[i + ps];
        out[4 * i + 2] = (float)src[i + 2 * ps];
        out[4 * i + 3] = (float)src[i + 3 * ps];
    }
}

static void render_hdr(char const *dir, int slide_idx, struct umbra_backend *be) {
    struct slide *s = slide_get(slide_idx);
    size_t const pixbuf_sz = (size_t)RW * RH * umbra_fmt_fp16_planar.bpp * 4;
    void *pixbuf = calloc(1, pixbuf_sz);

    s->prepare(s, be, umbra_fmt_fp16_planar);
    s->draw(s, 0, 0, 0, RW, RH, pixbuf);
    be->flush(be);

    float *fdata = malloc((size_t)(RW * RH) * 4 * sizeof(float));
    fp16p_to_float(fdata, pixbuf);
    free(pixbuf);

    char p[256];
    snprintf(p, sizeof p, "%s/render.hdr", dir);
    stbi_write_hdr(p, RW, RH, 4, fdata);
    free(fdata);
}

int main(void) {
    dump_builder("dumps", "srcover", build_srcover());

    slides_init(RW, RH);

    struct umbra_backend *be = umbra_backend_jit();
    if (!be) { be = umbra_backend_interp(); }

    for (int i = 0; i < slide_count(); i++) {
        struct slide *s = slide_get(i);
        if (!s->get_builder) { continue; }
        char dir[128];
        slugify(s->title, dir, sizeof dir);
        mkdir(dir, 0755);
        struct umbra_builder *b = s->get_builder(s, umbra_fmt_fp16);
        if (b) { dump_builder(dir, "draw", b); }
        struct umbra_builder *bp = s->get_builder(s, umbra_fmt_fp16_planar);
        if (bp) { dump_builder(dir, "draw_fp16p", bp); }

        render_hdr(dir, i, be);
    }

    dump_builder("dumps", "slug_acc", slug_build_acc(NULL));

    slides_cleanup();
    be->free(be);
    return 0;
}
