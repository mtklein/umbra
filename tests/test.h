#pragma once
#include "../include/umbra.h"

int dprintf(int, char const[], ...);
#define here || (dprintf(1, "%s:%d failed\n",__FILE__,__LINE__),__builtin_debugtrap(),0)

static inline _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

enum { NUM_BACKENDS = 3 };

typedef struct {
    struct umbra_backend *be[NUM_BACKENDS];
    struct umbra_program *b[NUM_BACKENDS];
} test_backends;

static inline test_backends test_backends_make(
        struct umbra_basic_block const *bb) {
    test_backends B;
    B.be[0] = umbra_backend_interp();
    B.be[1] = umbra_backend_jit();
    B.be[2] = umbra_backend_metal();
    for (int i = 0; i < NUM_BACKENDS; i++) {
        B.b[i] = B.be[i]
            ? umbra_backend_compile(B.be[i], bb)
            : NULL;
    }
    (B.b[0] != 0) here;
#if defined(__aarch64__) || defined(__AVX2__)
    (B.b[1] != 0) here;
#endif
#if defined(__APPLE__) && !defined(__wasm__)
    (B.b[2] != 0) here;
#endif
    return B;
}

static inline _Bool test_backends_run(
        test_backends *B, int bi,
        int n, umbra_buf buf[]) {
    if (!B->b[bi]) { return 0; }
    umbra_program_queue(B->b[bi], n, buf);
    umbra_backend_flush(B->be[bi]);
    return 1;
}

static inline void test_backends_free(
        test_backends *B) {
    for (int i = 0; i < NUM_BACKENDS; i++) {
        umbra_program_free(B->b[i]);
        umbra_backend_free(B->be[i]);
    }
}
