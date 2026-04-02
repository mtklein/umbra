#pragma once
#include "../include/umbra.h"
#include <stdlib.h>

int dprintf(int, char const[], ...);
#define here ? (void)0 : (dprintf(1, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap())

static inline _Bool equiv(float x, float y) {
    return (x <= y && y <= x) || (x != x && y != y);
}

enum { NUM_BACKENDS = 4 };

typedef struct {
    struct umbra_backend *be[NUM_BACKENDS];
    struct umbra_program *p[NUM_BACKENDS];
} test_backends;

static inline test_backends test_backends_make(struct umbra_basic_block const *bb) {
    test_backends B;
    B.be[0] = umbra_backend_interp();
    B.be[1] = umbra_backend_jit();
    B.be[2] = umbra_backend_metal();
    B.be[3] = umbra_backend_vulkan();
    for (int i = 0; i < NUM_BACKENDS; i++) {
        B.p[i] = B.be[i] ? B.be[i]->compile(B.be[i], bb) : NULL;
    }
    B.p[0] != 0 here;
#if defined(__aarch64__) || defined(__AVX2__)
    B.p[1] != 0 here;
#endif
#if (defined(__APPLE__) && defined(__clang__)) && !defined(__wasm__)
    B.p[2] != 0 here;
#endif
    return B;
}

static inline _Bool test_backends_run(test_backends *B, int bi, int r, int b,
                                      struct umbra_buf buf[]) {
    if (!B->p[bi]) { return 0; }
    B->p[bi]->queue(B->p[bi], 0, 0, r, b, buf);
    B->be[bi]->flush(B->be[bi]);
    return 1;
}

static inline void test_backends_free(test_backends *B) {
    for (int i = 0; i < NUM_BACKENDS; i++) {
        if (B->p[i]) { B->p[i]->free(B->p[i]); }
        if (B->be[i]) { B->be[i]->free(B->be[i]); }
    }
}
