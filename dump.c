#include "srcover.h"
#include "umbra_draw.h"
#include <stdio.h>

#if defined(__aarch64__)
  #define JIT_EXT "arm64"
#elif defined(__AVX2__)
  #define JIT_EXT "avx2"
#endif

static void dump(char const *name,
                 umbra_shader_fn   shader,
                 umbra_coverage_fn coverage,
                 umbra_blend_fn    blend,
                 umbra_load_fn     load,
                 umbra_store_fn    store) {
    struct umbra_builder *builder =
        umbra_draw_build(shader, coverage,
                         blend, load, store, NULL);
    { char p[64];
      snprintf(p, sizeof p, "dumps/%s.bb", name);
      FILE *f = fopen(p, "w");
      umbra_builder_dump(builder, f);
      fclose(f); }

    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);
    { char p[64];
      snprintf(p, sizeof p, "dumps/%s.opt", name);
      FILE *f = fopen(p, "w");
      umbra_basic_block_dump(bb, f);
      fclose(f); }

    struct umbra_codegen *cg  = umbra_codegen(bb);
    struct umbra_jit     *jit = umbra_jit(bb);
    struct umbra_metal   *mtl = umbra_metal(bb);
    umbra_basic_block_free(bb);

    if (cg) {
        char p[64];
        snprintf(p, sizeof p, "dumps/%s.c", name);
        FILE *f = fopen(p, "w");
        umbra_codegen_dump(cg, f);
        fclose(f);
    }
#ifdef JIT_EXT
    if (jit) {
        char p[64];
        snprintf(p, sizeof p,
                 "dumps/%s." JIT_EXT, name);
        FILE *f = fopen(p, "w");
        umbra_jit_dump(jit, f);
        fclose(f);
    }
    if (jit) {
        char p[64];
        snprintf(p, sizeof p,
                 "dumps/%s." JIT_EXT ".mca", name);
        FILE *f = fopen(p, "w");
        umbra_jit_mca(jit, f);
        fclose(f);
    }
#endif
    if (mtl) {
        char p[64];
        snprintf(p, sizeof p, "dumps/%s.metal", name);
        FILE *f = fopen(p, "w");
        umbra_metal_dump(mtl, f);
        fclose(f);
    }

    if (cg)  { umbra_codegen_free(cg); }
    if (jit) { umbra_jit_free(jit); }
    if (mtl) { umbra_metal_free(mtl); }
}

int main(void) {
    struct umbra_builder *builder = build_srcover();
    { FILE *f = fopen("dumps/srcover.bb", "w");
      umbra_builder_dump(builder, f);
      fclose(f); }

    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);
    { FILE *f = fopen("dumps/srcover.opt", "w");
      umbra_basic_block_dump(bb, f);
      fclose(f); }

    struct umbra_codegen *cg  = umbra_codegen(bb);
    struct umbra_jit     *jit = umbra_jit(bb);
    struct umbra_metal   *mtl = umbra_metal(bb);
    umbra_basic_block_free(bb);

    if (cg) {
        FILE *f = fopen("dumps/srcover.c", "w");
        umbra_codegen_dump(cg, f);
        fclose(f);
    }
#ifdef JIT_EXT
    if (jit) {
        FILE *f = fopen("dumps/srcover." JIT_EXT, "w");
        umbra_jit_dump(jit, f);
        fclose(f);
    }
    if (jit) {
        FILE *f =
            fopen("dumps/srcover." JIT_EXT ".mca", "w");
        umbra_jit_mca(jit, f);
        fclose(f);
    }
#endif
    if (mtl) {
        FILE *f = fopen("dumps/srcover.metal", "w");
        umbra_metal_dump(mtl, f);
        fclose(f);
    }

    if (cg)  { umbra_codegen_free(cg); }
    if (jit) { umbra_jit_free(jit); }
    if (mtl) { umbra_metal_free(mtl); }

    dump("solid_src_8888",
         umbra_shader_solid, NULL,
         umbra_blend_src,
         umbra_load_8888, umbra_store_8888);
    dump("solid_srcover_8888",
         umbra_shader_solid, NULL,
         umbra_blend_srcover,
         umbra_load_8888, umbra_store_8888);

    return 0;
}
