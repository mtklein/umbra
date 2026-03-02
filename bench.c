#define _POSIX_C_SOURCE 199309L
#include "len.h"
#include "umbra.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char *argv[]) {
    int n = 4096;
    if (argc > 1) {
        n = atoi(argv[1]);
    }

    struct umbra_inst const inst[] = {
        [0] = {.op=umbra_lane},
        [1] = {umbra_load_32,  .ptr=0, .x=0},
        [2] = {umbra_load_32,  .ptr=1, .x=0},
        [3] = {umbra_mul_f32,  .x=1, .y=2},
        [4] = {umbra_add_f32,  .x=1, .y=3},
        [5] = {umbra_store_32, .ptr=2, .x=0, .y=4},
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

    umbra_program_free(p);
    free(x);
    free(y);
    free(z);
    return 0;
}
