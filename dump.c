#include "srcover.h"
#include "umbra_draw.h"
#include "slides/slide.h"
#include "slides/slug.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(__aarch64__)
  #define JIT_EXT "arm64"
#elif defined(__AVX2__)
  #define JIT_EXT "avx2"
#endif

static void dump_bb(char const *dir,
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

    struct umbra_backend *backs[] = {
#ifdef JIT_EXT
        umbra_backend_jit(bb),
#endif
        umbra_backend_metal(bb),
    };
    char const *exts[] = {
#ifdef JIT_EXT
        JIT_EXT ".mca",
#endif
        "metal",
    };
    umbra_basic_block_free(bb);

    int nb = (int)(sizeof backs / sizeof backs[0]);
    for (int i = 0; i < nb; i++) {
        if (!backs[i]) { continue; }
        snprintf(p, sizeof p,
                 "%s/%s.%s", dir, name, exts[i]);
        FILE *f = fopen(p, "w");
        umbra_backend_dump(backs[i], f);
        fclose(f);
        umbra_backend_free(backs[i]);
    }
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
    dump_bb("dumps", "srcover",
        build_srcover());

    slides_init_for_dump();

    for (int i = 0; i < slide_count(); i++) {
        slide *s = slide_get(i);
        char dir[128];
        slugify(s->title, dir, sizeof dir);
        mkdir(dir, 0755);

        dump_bb(dir, "draw",
            umbra_draw_build(
                s->shader, s->coverage,
                s->blend, s->load, s->store,
                NULL));
    }

    dump_bb("dumps", "slug_acc",
        slug_build_acc(NULL));

    slides_cleanup();
    return 0;
}
