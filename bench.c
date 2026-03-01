#include "len.h"
#include "umbra.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef __wasi__
#include <time.h>
static double now(void) {
    clock_t t = clock();
    return (double)t / (double)CLOCKS_PER_SEC;
}
#endif

int main(int argc, char *argv[]) {
    int n = 4096;
    if (argc > 1) {
        n = atoi(argv[1]);
    }

    struct umbra_inst const inst[] = {
        [0] = {umbra_load_32,  .ptr=0},
        [1] = {umbra_load_32,  .ptr=1},
        [2] = {umbra_mul_f32,  .x=0, .y=1},
        [3] = {umbra_add_f32,  .x=0, .y=2},
        [4] = {umbra_store_32, .ptr=2, .x=3},
    };
    struct umbra_program *p = umbra_program(inst, len(inst));

    float *x = malloc((size_t)n * sizeof *x),
          *y = malloc((size_t)n * sizeof *y),
          *z = malloc((size_t)n * sizeof *z);
    for (int i = 0; i < n; i++) {
        x[i] = (float)i;
        y[i] = (float)(n - i);
    }

    umbra_program_run(p, n, (void*[]){x, y, z});

#ifndef __wasi__
    int iters = 1;
    for (;;) {
        double start = now();
        for (int i = 0; i < iters; i++) {
            umbra_program_run(p, n, (void*[]){x, y, z});
        }
        double elapsed = now() - start;
        if (elapsed >= 1.0) {
            double ns_per_elem = elapsed / ((double)iters * (double)n) * 1e9;
            printf("%d elements, %d iters, %.3fs, %.2f ns/elem\n",
                   n, iters, elapsed, ns_per_elem);
            break;
        }
        iters *= 2;
    }
#else
    printf("ran %d elements\n", n);
#endif

    umbra_program_free(p);
    free(x);
    free(y);
    free(z);
    return 0;
}
