#include "program.h"
#include "bb.h"

void *umbra_metal_backend_create(void) { return 0; }
void  umbra_metal_backend_free(void *ctx) { (void)ctx; }

struct umbra_metal {
    int dummy;
};
struct umbra_metal *umbra_metal(void *ctx, struct umbra_basic_block const *bb) {
    (void)ctx;
    (void)bb;
    return 0;
}
void umbra_metal_run(struct umbra_metal *m, int n, umbra_buf buf[]) {
    (void)m;
    (void)n;
    (void)buf;
}
void umbra_metal_begin_batch(void *ctx) { (void)ctx; }
void umbra_metal_flush(void *ctx) { (void)ctx; }
void umbra_metal_free(struct umbra_metal *m) { (void)m; }
void umbra_dump_metal(struct umbra_metal const *m, FILE *f) {
    (void)m;
    (void)f;
}
