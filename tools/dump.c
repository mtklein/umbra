#include "../include/umbra_draw.h"
#include "../slides/slide.h"
#include "../slides/slug.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static struct umbra_builder *build_srcover(void) {
    struct umbra_builder *b = umbra_builder();

    umbra_val px = umbra_load_32(b, (umbra_ptr){0, 0}), mask = umbra_imm_i32(b, 0xFF);
    umbra_val rgba[4] = {
        umbra_and_i32(b, px, mask),
        umbra_and_i32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 8)), mask),
        umbra_and_i32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 16)), mask),
        umbra_shr_u32(b, px, umbra_imm_i32(b, 24)),
    };

    umbra_val inv255 = umbra_imm_f32(b, 1.0f / 255.0f);
    umbra_val sr = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[0]), inv255),
              sg = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[1]), inv255),
              sb = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[2]), inv255),
              sa = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[3]), inv255),
              dr = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr){1, 0})),
              dg = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr){2, 0})),
              db = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr){3, 0})),
              da = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr){4, 0})),
              one = umbra_imm_f32(b, 1.0f), inv_a = umbra_sub_f32(b, one, sa),
              rout = umbra_add_f32(b, sr, umbra_mul_f32(b, dr, inv_a)),
              gout = umbra_add_f32(b, sg, umbra_mul_f32(b, dg, inv_a)),
              bout = umbra_add_f32(b, sb, umbra_mul_f32(b, db, inv_a)),
              aout = umbra_add_f32(b, sa, umbra_mul_f32(b, da, inv_a));
    umbra_store_16(b, (umbra_ptr){1, 0}, umbra_f16_from_f32(b, rout));
    umbra_store_16(b, (umbra_ptr){2, 0}, umbra_f16_from_f32(b, gout));
    umbra_store_16(b, (umbra_ptr){3, 0}, umbra_f16_from_f32(b, bout));
    umbra_store_16(b, (umbra_ptr){4, 0}, umbra_f16_from_f32(b, aout));
    return b;
}

#if defined(__aarch64__)
#define JIT_EXT "arm64"
#elif defined(__AVX2__)
#define JIT_EXT "avx2"
#endif

static void dump_bb(char const *dir, char const *name, struct umbra_builder *b) {
    char p[128];
    {
        snprintf(p, sizeof p, "%s/%s.builder", dir, name);
        FILE *f = fopen(p, "w");
        umbra_dump_builder(b, f);
        fclose(f);
    }

    struct umbra_basic_block *bb = umbra_basic_block(b);
    umbra_builder_free(b);
    {
        snprintf(p, sizeof p, "%s/%s.bb", dir, name);
        FILE *f = fopen(p, "w");
        umbra_dump_basic_block(bb, f);
        fclose(f);
    }

    struct umbra_backend *bes[] = {
#ifdef JIT_EXT
        umbra_backend_jit(),
#endif
        umbra_backend_metal(),
    };
    struct umbra_program *progs[] = {
#ifdef JIT_EXT
        bes[0]->compile(bes[0], bb),
#endif
        bes[sizeof bes / sizeof bes[0] - 1]->compile(bes[sizeof bes / sizeof bes[0] - 1], bb),
    };
    char const *exts[] = {
#ifdef JIT_EXT
        JIT_EXT,
#endif
        "metal",
    };
    umbra_basic_block_free(bb);

    int nb = (int)(sizeof progs / sizeof progs[0]);
    for (int i = 0; i < nb; i++) {
        if (!progs[i]) { continue; }
        snprintf(p, sizeof p, "%s/%s.%s", dir, name, exts[i]);
        FILE *f = fopen(p, "w");
        if (progs[i]->dump) { progs[i]->dump(progs[i], f); }
        fclose(f);
        progs[i]->free(progs[i]);
    }
    for (int i = 0; i < (int)(sizeof bes / sizeof bes[0]); i++) {
        if (bes[i]) { bes[i]->free(bes[i]); }
    }
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

int main(void) {
    dump_bb("dumps", "srcover", build_srcover());

    slides_init_for_dump();

    for (int i = 0; i < slide_count(); i++) {
        slide *s = slide_get(i);
        char   dir[128];
        slugify(s->title, dir, sizeof dir);
        mkdir(dir, 0755);

        dump_bb(dir, "draw",
                umbra_draw_build(s->shader, s->coverage, s->blend, s->fmt, NULL));
    }

    dump_bb("dumps", "slug_acc", slug_build_acc(NULL));

    slides_cleanup();
    return 0;
}
