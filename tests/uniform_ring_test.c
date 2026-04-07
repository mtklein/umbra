#include "../src/uniform_ring.h"
#include "test.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct fake_be {
    size_t default_cap;
    int    n_create, n_destroy;
};

static struct uniform_ring_chunk fake_new(size_t min_bytes, void *ctx) {
    struct fake_be *be = ctx;
    be->n_create++;
    size_t cap = min_bytes > be->default_cap ? min_bytes : be->default_cap;
    void *mapped = calloc(1, cap);
    return (struct uniform_ring_chunk){.handle=mapped, .mapped=mapped, .cap=cap, .used=0};
}

static void fake_free(void *handle, void *ctx) {
    struct fake_be *be = ctx;
    be->n_destroy++;
    free(handle);
}

static struct uniform_ring make_ring(struct fake_be *be) {
    return (struct uniform_ring){
        .align     =16,
        .ctx       =be,
        .new_chunk =fake_new,
        .free_chunk=fake_free,
    };
}

TEST(uniform_ring_basic) {
    struct fake_be be = {.default_cap=1024};
    struct uniform_ring r = make_ring(&be);

    int p1 = 0x11223344;
    struct uniform_ring_loc l1 = uniform_ring_alloc(&r, &p1, sizeof p1);
    l1.handle != 0 here;
    l1.offset == 0 here;
    be.n_create == 1 here;
    __builtin_memcmp((char*)l1.handle + l1.offset, &p1, sizeof p1) == 0 here;

    int p2 = 0x55667788;
    struct uniform_ring_loc l2 = uniform_ring_alloc(&r, &p2, sizeof p2);
    l2.handle == l1.handle here;
    l2.offset == 16 here;
    be.n_create == 1 here;
    __builtin_memcmp((char*)l2.handle + l2.offset, &p2, sizeof p2) == 0 here;

    uniform_ring_free(&r);
    be.n_destroy == 1 here;
}

TEST(uniform_ring_reset_reuses_chunks) {
    struct fake_be be = {.default_cap=1024};
    struct uniform_ring r = make_ring(&be);

    unsigned p = 0xAABBCCDDu;
    struct uniform_ring_loc l1 = uniform_ring_alloc(&r, &p, sizeof p);
    void *first_handle = l1.handle;
    be.n_create == 1 here;

    uniform_ring_reset(&r);
    struct uniform_ring_loc l2 = uniform_ring_alloc(&r, &p, sizeof p);
    l2.handle == first_handle here;
    l2.offset == 0 here;
    be.n_create == 1 here;

    uniform_ring_free(&r);
    be.n_destroy == 1 here;
}

TEST(uniform_ring_slug_like) {
    struct fake_be be = {.default_cap=64*1024};
    struct uniform_ring r = make_ring(&be);

    char payload[72];
    for (int batch = 0; batch < 5; batch++) {
        uniform_ring_reset(&r);
        for (int j = 0; j < 14; j++) {
            __builtin_memset(payload, j, sizeof payload);
            struct uniform_ring_loc l = uniform_ring_alloc(&r, payload, sizeof payload);
            l.handle != 0 here;
        }
    }
    be.n_create == 1 here;

    uniform_ring_free(&r);
}

TEST(uniform_ring_animation) {
    struct fake_be be = {.default_cap=256};
    struct uniform_ring r = make_ring(&be);

    int payload = 0;
    for (int i = 0; i < 100; i++) {
        payload = i;
        struct uniform_ring_loc l = uniform_ring_alloc(&r, &payload, sizeof payload);
        l.handle != 0 here;
    }
    be.n_create == 7 here;

    uniform_ring_reset(&r);
    for (int i = 0; i < 100; i++) {
        payload = i + 1000;
        uniform_ring_alloc(&r, &payload, sizeof payload);
    }
    be.n_create == 7 here;

    uniform_ring_free(&r);
    be.n_destroy == 7 here;
}

TEST(uniform_ring_oversized) {
    struct fake_be be = {.default_cap=256};
    struct uniform_ring r = make_ring(&be);

    char big[2048];
    __builtin_memset(big, 0xAB, sizeof big);
    struct uniform_ring_loc l = uniform_ring_alloc(&r, big, sizeof big);
    l.handle != 0 here;
    l.offset == 0 here;
    be.n_create == 1 here;
    __builtin_memcmp((char*)l.handle + l.offset, big, sizeof big) == 0 here;

    uniform_ring_free(&r);
}

TEST(uniform_ring_small_then_big) {
    struct fake_be be = {.default_cap=256};
    struct uniform_ring r = make_ring(&be);

    int small = 0x12345678;
    struct uniform_ring_loc l1 = uniform_ring_alloc(&r, &small, sizeof small);
    l1.offset == 0 here;
    be.n_create == 1 here;

    char big[2048];
    __builtin_memset(big, 0xCD, sizeof big);
    struct uniform_ring_loc l2 = uniform_ring_alloc(&r, big, sizeof big);
    l2.handle != l1.handle here;
    l2.offset == 0 here;
    be.n_create == 2 here;
    __builtin_memcmp((char*)l2.handle, big, sizeof big) == 0 here;

    uniform_ring_free(&r);
}

TEST(uniform_ring_null_bytes_reserves_only) {
    struct fake_be be = {.default_cap=256};
    struct uniform_ring r = make_ring(&be);

    struct uniform_ring_loc l = uniform_ring_alloc(&r, 0, 64);
    l.offset == 0 here;
    be.n_create == 1 here;

    char zeros[64] = {0};
    __builtin_memcmp((char*)l.handle, zeros, 64) == 0 here;

    uniform_ring_free(&r);
}
