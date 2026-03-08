#include "srcover.h"
#include <stdio.h>

#if defined(__aarch64__)
  #define JIT_EXT "arm64"
#elif defined(__AVX2__)
  #define JIT_EXT "avx2"
#endif

int main(void) {
    struct umbra_basic_block *bb = build_srcover();
    { FILE *f = fopen("dumps/srcover.bb", "w"); umbra_basic_block_dump(bb, f); fclose(f); }

    umbra_basic_block_optimize(bb);
    { FILE *f = fopen("dumps/srcover.opt", "w"); umbra_basic_block_dump(bb, f); fclose(f); }

    struct umbra_codegen *cg  = umbra_codegen(bb);
    struct umbra_jit     *jit = umbra_jit(bb);
    struct umbra_metal   *mtl = umbra_metal(bb);
    umbra_basic_block_free(bb);

    if (cg)  { FILE *f = fopen("dumps/srcover.c",     "w"); umbra_codegen_dump(cg,  f); fclose(f); }
#ifdef JIT_EXT
    if (jit) { FILE *f = fopen("dumps/srcover." JIT_EXT,       "w"); umbra_jit_dump(jit, f); fclose(f); }
    if (jit) { FILE *f = fopen("dumps/srcover." JIT_EXT ".mca","w"); umbra_jit_mca (jit, f); fclose(f); }
#endif
    if (mtl) { FILE *f = fopen("dumps/srcover.metal", "w"); umbra_metal_dump  (mtl, f); fclose(f); }

    if (cg)  { umbra_codegen_free(cg); }
    if (jit) { umbra_jit_free(jit); }
    if (mtl) { umbra_metal_free(mtl); }
    return 0;
}
