#include "srcover.h"
#include "umbra_draw.h"
#include "slides/slide.h"
#include "slides/slug.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(__aarch64__)
  #define JIT_EXT "arm64"
#elif defined(__AVX2__)
  #define JIT_EXT "avx2"
#endif

static void dump_builder(char const *dir,
                         char const *name,
                         struct umbra_builder *b) {
    char p[128];
    { snprintf(p, sizeof p,
               "%s/%s.builder", dir, name);
      FILE *f = fopen(p, "w");
      umbra_dump_builder(b, f);
      fclose(f); }

    struct umbra_basic_block *bb =
        umbra_basic_block(b);
    umbra_builder_free(b);
    { snprintf(p, sizeof p, "%s/%s.bb", dir, name);
      FILE *f = fopen(p, "w");
      umbra_dump_basic_block(bb, f);
      fclose(f); }

    struct umbra_codegen *cg  = umbra_codegen(bb);
    struct umbra_jit     *jit = umbra_jit(bb);
    struct umbra_metal   *mtl = umbra_metal(bb);
    umbra_basic_block_free(bb);

    if (cg) {
        snprintf(p, sizeof p,
                 "%s/%s.c", dir, name);
        FILE *f = fopen(p, "w");
        umbra_dump_codegen(cg, f);
        fclose(f);
    }
#ifdef JIT_EXT
    if (jit) {
        snprintf(p, sizeof p,
                 "%s/%s." JIT_EXT, dir, name);
        FILE *f = fopen(p, "w");
        umbra_dump_jit(jit, f);
        fclose(f);
    }
    if (jit) {
        snprintf(p, sizeof p,
                 "%s/%s." JIT_EXT ".mca", dir, name);
        FILE *f = fopen(p, "w");
        umbra_dump_jit_mca(jit, f);
        fclose(f);
    }
#endif
    if (mtl) {
        snprintf(p, sizeof p,
                 "%s/%s.metal", dir, name);
        FILE *f = fopen(p, "w");
        umbra_dump_metal(mtl, f);
        fclose(f);
    }

    if (cg)  { umbra_codegen_free(cg); }
    if (jit) { umbra_jit_free(jit); }
    if (mtl) { umbra_metal_free(mtl); }
}

static void slugify(char const *title,
                    char *out, size_t sz) {
    int n = snprintf(out, sz, "dumps/");
    for (int i = 0; title[i] && n < (int)sz - 1; i++) {
        char c = title[i];
        if (c >= 'A' && c <= 'Z') {
            out[n++] = (char)(c + 32);
        } else if ((c >= 'a' && c <= 'z')
                || (c >= '0' && c <= '9')) {
            out[n++] = c;
        } else if (n > 0 && out[n-1] != '_') {
            out[n++] = '_';
        }
    }
    while (n > 0 && out[n-1] == '_') { n--; }
    out[n] = '\0';
}

int main(void) {
    dump_builder("dumps", "srcover",
        build_srcover());

    slides_init(640, 480);

    for (int i = 0; i < slide_count() - 1; i++) {
        slide *s = slide_get(i);
        char dir[128];
        slugify(s->title, dir, sizeof dir);
        mkdir(dir, 0755);

        dump_builder(dir, "draw",
            umbra_draw_build(
                s->shader, s->coverage,
                s->blend, s->load, s->store,
                NULL));
    }

    dump_builder("dumps", "slug_acc",
        slug_build_acc(NULL));

    slides_cleanup();
    return 0;
}
