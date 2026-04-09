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

TEST(uniform_ring_large_payloads) {
    struct fake_be be = {.default_cap=64*1024};
    struct uniform_ring r = make_ring(&be);

    char block[4096];
    for (int i = 0; i < 20; i++) {
        __builtin_memset(block, i + 1, sizeof block);
        struct uniform_ring_loc l = uniform_ring_alloc(&r, block, sizeof block);
        l.handle != 0 here;
        __builtin_memcmp((char*)l.handle + l.offset, block, sizeof block) == 0 here;
    }
    // 16 4-KiB blocks fit per 64-KiB chunk, so 20 blocks need 2 chunks.
    be.n_create == 2 here;
    uniform_ring_used(&r) == 20 * 4096 here;

    uniform_ring_free(&r);
    be.n_destroy == 2 here;
}

TEST(uniform_ring_huge_single_payload) {
    struct fake_be be = {.default_cap=64*1024};
    struct uniform_ring r = make_ring(&be);

    // A single uniform larger than default_cap forces new_chunk(min_bytes=reserved).
    size_t big_bytes = 256 * 1024;
    char  *big       = calloc(1, big_bytes);
    for (size_t i = 0; i < big_bytes; i++) { big[i] = (char)(i & 0xff); }

    struct uniform_ring_loc l = uniform_ring_alloc(&r, big, big_bytes);
    l.handle != 0 here;
    l.offset == 0 here;
    be.n_create == 1 here;
    __builtin_memcmp((char*)l.handle + l.offset, big, big_bytes) == 0 here;

    free(big);
    uniform_ring_free(&r);
    be.n_destroy == 1 here;
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

static struct uniform_ring_pool make_pool(struct fake_be *be, int n, size_t high_water) {
    struct uniform_ring_pool p = {
        .n         =n,
        .high_water=high_water,
        .ctx       =be,
        .wait_frame=0,  // set by callers that need it
    };
    for (int i = 0; i < n; i++) {
        p.rings[i] = (struct uniform_ring){
            .align     =16,
            .ctx       =be,
            .new_chunk =fake_new,
            .free_chunk=fake_free,
        };
    }
    return p;
}

struct wait_log {
    int frames[16];
    int n;
};

static void wait_log_record(int frame, void *ctx) {
    struct wait_log *w = ctx;
    if (w->n < 16) { w->frames[w->n++] = frame; }
}

TEST(uniform_ring_pool_basic_alloc) {
    struct fake_be be = {.default_cap=256};
    struct uniform_ring_pool p = make_pool(&be, 2, 64);

    int x = 0x1ABCDEF0;
    struct uniform_ring_loc l = uniform_ring_pool_alloc(&p, &x, sizeof x);
    l.handle != 0 here;
    p.cur == 0 here;
    p.rotations == 0 here;
    __builtin_memcmp((char*)l.handle + l.offset, &x, sizeof x) == 0 here;

    uniform_ring_pool_free(&p);
}

TEST(uniform_ring_pool_should_rotate_threshold) {
    struct fake_be be = {.default_cap=1024};
    struct uniform_ring_pool p = make_pool(&be, 2, 64);

    int payload = 0;
    !uniform_ring_pool_should_rotate(&p) here;
    for (int i = 0; i < 4; i++) { uniform_ring_pool_alloc(&p, &payload, sizeof payload); }
    !uniform_ring_pool_should_rotate(&p) here;  // 64 used == high_water, not >
    uniform_ring_pool_alloc(&p, &payload, sizeof payload);
    uniform_ring_pool_should_rotate(&p) here;

    uniform_ring_pool_free(&p);
}

TEST(uniform_ring_pool_rotate_increments_and_waits) {
    struct fake_be be = {.default_cap=1024};
    struct uniform_ring_pool p = make_pool(&be, 2, 64);
    struct wait_log w = {0};
    p.ctx        = &w;  // wait_frame ctx is the pool ctx
    p.wait_frame = wait_log_record;
    // Each ring still uses its own ctx (the fake_be) for chunk alloc.
    for (int i = 0; i < p.n; i++) { p.rings[i].ctx = &be; }

    int payload = 0;
    uniform_ring_pool_alloc(&p, &payload, sizeof payload);
    uniform_ring_pool_rotate(&p);
    p.cur == 1 here;
    p.rotations == 1 here;
    w.n == 1 here;
    w.frames[0] == 1 here;

    uniform_ring_pool_alloc(&p, &payload, sizeof payload);
    uniform_ring_pool_rotate(&p);
    p.cur == 0 here;
    p.rotations == 2 here;
    w.n == 2 here;
    w.frames[1] == 0 here;

    uniform_ring_pool_free(&p);
}

TEST(uniform_ring_pool_rotate_resets_new_cur_ring) {
    struct fake_be be = {.default_cap=1024};
    struct uniform_ring_pool p = make_pool(&be, 2, 64);
    struct wait_log w = {0};
    p.ctx        = &w;
    p.wait_frame = wait_log_record;
    for (int i = 0; i < p.n; i++) { p.rings[i].ctx = &be; }

    // Pre-fill ring 1 with some bytes from a "previous cycle".
    p.cur = 1;
    int payload = 0xDEAD;
    uniform_ring_pool_alloc(&p, &payload, sizeof payload);
    uniform_ring_used(&p.rings[1]) > 0 here;
    p.cur = 0;

    // Allocate on ring 0, then rotate. The pool should land on ring 1 with
    // a freshly-reset ring (used == 0) after wait_frame.
    uniform_ring_pool_alloc(&p, &payload, sizeof payload);
    uniform_ring_pool_rotate(&p);
    p.cur == 1 here;
    uniform_ring_used(&p.rings[1]) == 0 here;

    uniform_ring_pool_free(&p);
}

TEST(uniform_ring_pool_drain_all_visits_every_frame) {
    struct fake_be be = {.default_cap=1024};
    struct uniform_ring_pool p = make_pool(&be, 3, 64);
    struct wait_log w = {0};
    p.ctx        = &w;
    p.wait_frame = wait_log_record;
    for (int i = 0; i < p.n; i++) { p.rings[i].ctx = &be; }

    int payload = 0;
    for (int f = 0; f < 3; f++) {
        p.cur = f;
        uniform_ring_pool_alloc(&p, &payload, sizeof payload);
    }
    p.cur = 0;

    uniform_ring_pool_drain_all(&p);
    w.n == 3 here;
    w.frames[0] == 0 here;
    w.frames[1] == 1 here;
    w.frames[2] == 2 here;
    for (int i = 0; i < 3; i++) { uniform_ring_used(&p.rings[i]) == 0 here; }

    uniform_ring_pool_free(&p);
}

TEST(uniform_ring_used_tracks_high_water) {
    struct fake_be be = {.default_cap=256};
    struct uniform_ring r = make_ring(&be);

    uniform_ring_used(&r) == 0 here;

    int payload = 0;
    uniform_ring_alloc(&r, &payload, sizeof payload);
    uniform_ring_used(&r) == 16 here;

    uniform_ring_alloc(&r, &payload, sizeof payload);
    uniform_ring_used(&r) == 32 here;

    for (int i = 0; i < 30; i++) {
        uniform_ring_alloc(&r, &payload, sizeof payload);
    }
    be.n_create == 2 here;
    uniform_ring_used(&r) == 32 * 16 here;

    uniform_ring_reset(&r);
    uniform_ring_used(&r) == 0 here;

    uniform_ring_free(&r);
}
