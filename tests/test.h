#pragma once
#include "../include/umbra.h"
#include <stdlib.h>

int dprintf(int, char const[], ...);

typedef void (*test_fn)(void);
void test_register(test_fn fn, char const *name);
void test_run     (char const *match, int shards, int shard);

#define TEST(NAME)                                                                \
    static void NAME(void);                                                       \
    _Pragma("clang diagnostic push")                                              \
    _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"")                 \
    __attribute__((constructor)) static void test_ctor_##NAME(void) {             \
        test_register(NAME, #NAME);                                               \
    }                                                                             \
    _Pragma("clang diagnostic pop")                                               \
    static void NAME(void)

#define here ? (void)0 : (dprintf(2, "%s:%d failed\n", __FILE__, __LINE__), __builtin_trap())

_Bool equiv (float x, float y);
_Bool val_eq(umbra_val32 a, umbra_val32 b);
_Bool val_lt(umbra_val32 a, umbra_val32 b);

enum { NUM_BACKENDS = 5 };

struct test_backends {
    struct umbra_backend *be[NUM_BACKENDS];
    struct umbra_program *p[NUM_BACKENDS];
};

struct test_backends test_backends_make(struct umbra_flat_ir const *ir);
_Bool                test_backends_run (struct test_backends *B, int bi, int r, int b,
                                        struct umbra_buf buf[]);
void                 test_backends_free(struct test_backends *B);
