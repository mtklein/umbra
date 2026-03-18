#pragma once
#include "../umbra.h"

int dprintf(int, char const[], ...);
#define here || (dprintf(1, "%s:%d failed\n",__FILE__,__LINE__),__builtin_debugtrap(),0)

static inline _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

typedef struct {
    struct umbra_interpreter *interp;
    struct umbra_jit         *jit;
    struct umbra_metal       *mtl;
} test_backends;

static inline test_backends test_backends_make(
        struct umbra_basic_block const *bb) {
    test_backends B = {
        umbra_interpreter(bb),
        umbra_jit(bb),
        umbra_metal(bb),
    };
    (B.interp != 0) here;
#if defined(__aarch64__) || defined(__AVX2__)
    (B.jit != 0) here;
#endif
#if defined(__APPLE__) && !defined(__wasm__)
    (B.mtl != 0) here;
#endif
    return B;
}

static inline _Bool test_backends_run(
        test_backends *B, int b,
        int n, umbra_buf buf[]) {
    switch (b) {
    case 0:
        umbra_interpreter_run(B->interp, n, buf);
        return 1;
    case 1:
        if (B->jit) {
            umbra_jit_run(B->jit, n, buf);
            return 1;
        }
        return 0;
    case 2:
        if (B->mtl) {
            umbra_metal_run(B->mtl, n, buf);
            return 1;
        }
        return 0;
    }
    return 0;
}

static inline void test_backends_free(
        test_backends *B) {
    umbra_interpreter_free(B->interp);
    if (B->jit) { umbra_jit_free(B->jit); }
    if (B->mtl) { umbra_metal_free(B->mtl); }
}
