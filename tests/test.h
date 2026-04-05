#pragma once
#include "../include/umbra.h"
#include <stdlib.h>
#ifndef __wasm__
    #include <unistd.h>
#endif

int dprintf(int, char const[], ...);

static inline void *test_aligned_alloc(size_t sz) {
#ifdef __wasm__
    return calloc(1, sz);
#else
    void *p = 0;
    posix_memalign(&p, (size_t)sysconf(_SC_PAGESIZE), sz);
    if (p) { __builtin_memset(p, 0, sz); }
    return p;
#endif
}
static inline void *test_misaligned_alloc(size_t sz) {
    // 16-byte aligned but not page-aligned: enough for SIMD, defeats Metal nocopy.
#ifdef __wasm__
    return calloc(1, sz);
#else
    void *p = 0;
    posix_memalign(&p, 16, sz);
    if (p) { __builtin_memset(p, 0, sz); }
    return p;
#endif
}
static inline void test_misaligned_free(void *p) {
    free(p);
}
#define here ? (void)0 : (dprintf(2, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap())

static inline _Bool equiv(float x, float y) {
    return (x <= y && y <= x) || (x != x && y != y);
}
static inline _Bool val_eq(umbra_val32 a, umbra_val32 b) { return a.id == b.id && a.chan == b.chan; }
static inline _Bool val_lt(umbra_val32 a, umbra_val32 b) {
    return a.id < b.id || (a.id == b.id && a.chan < b.chan);
}

enum { NUM_BACKENDS = 4 };

struct test_backends {
    struct umbra_backend *be[NUM_BACKENDS];
    struct umbra_program *p[NUM_BACKENDS];
};

static inline struct test_backends test_backends_make(struct umbra_basic_block const *bb) {
    struct test_backends B;
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

static inline _Bool test_backends_run(struct test_backends *B, int bi, int r, int b,
                                      struct umbra_buf buf[]) {
    if (!B->p[bi]) { return 0; }
    B->p[bi]->queue(B->p[bi], 0, 0, r, b, buf);
    B->be[bi]->flush(B->be[bi]);
    return 1;
}

static inline void test_backends_free(struct test_backends *B) {
    for (int i = 0; i < NUM_BACKENDS; i++) {
        if (B->p[i]) { B->p[i]->free(B->p[i]); }
        if (B->be[i]) { B->be[i]->free(B->be[i]); }
    }
}
