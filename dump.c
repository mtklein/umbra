#include "srcover.h"
#include <stdio.h>

int main(void) {
    struct umbra_basic_block *bb = build_srcover();
    { FILE *f = fopen("dumps/srcover.bb", "w"); umbra_basic_block_dump(bb, f); fclose(f); }

    umbra_basic_block_optimize(bb);
    { FILE *f = fopen("dumps/srcover.opt", "w"); umbra_basic_block_dump(bb, f); fclose(f); }

    struct umbra_codegen *cg  = umbra_codegen(bb);
    struct umbra_jit     *jit = umbra_jit(bb);
    umbra_basic_block_free(bb);

    if (cg)  { FILE *f = fopen("dumps/srcover.c",     "w"); umbra_codegen_dump(cg,  f); fclose(f); }
    if (jit) { FILE *f = fopen("dumps/srcover.arm64", "w"); umbra_jit_dump    (jit, f); fclose(f); }

    if (jit) { FILE *f = fopen("dumps/srcover.mca",   "w"); umbra_jit_mca     (jit, f); fclose(f); }

    if (cg)  { umbra_codegen_free(cg); }
    if (jit) { umbra_jit_free(jit); }
    return 0;
}
