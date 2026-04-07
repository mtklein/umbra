#include "../src/hash.h"
#include "test.h"
#include <stdint.h>
#include <stdlib.h>

static _Bool match_pointee(int val, void *ctx) {
    return val == *(int const *)ctx;
}

static _Bool match_int(int val, void *ctx) {
    return val == (int)(intptr_t)ctx;
}

#if defined(__clang__)
    __attribute__((no_sanitize("unsigned-integer-overflow", "unsigned-shift-base")))
#endif
static unsigned murmur3_scramble(int x) {
    unsigned bits = (unsigned)x * 0xcc9e2d51u;
    bits = (bits << 15) | (bits >> 17);
    return bits * 0x1b873593u;
}
static unsigned     zero(int x) { (void)x; return 0; }
static unsigned identity(int x) { return (unsigned)x; }

static void run_one(int const n, unsigned (*hash_fn)(int)) {
    struct hash h = {0};
    for (int i = 0; i < n; i += 2) {
        !hash_lookup(h, hash_fn(i), match_pointee, &i) here;
        hash_insert(&h, hash_fn(i), i);
        hash_lookup(h, hash_fn(i), match_pointee, &i)  here;
    }
    for (int i = 0; i < n; i += 1) {
        hash_lookup(h, hash_fn(i), match_pointee, &i) here;
        i++;
        !hash_lookup(h, hash_fn(i), match_pointee, &i) here;
    }
    for (int i = 0; i < n; i += 2) {
        hash_lookup(h, hash_fn(i), match_int, (void *)(intptr_t)i) here;
    }
    free(h.data);
}

TEST(hash_basics) {
    struct hash h = {0};

    !hash_lookup(h, 5, match_pointee, &(int){5}) here;
    !hash_lookup(h, 7, match_pointee, &(int){7}) here;

    hash_insert(&h, 5, 5);
    hash_lookup(h, 5, match_pointee, &(int){5}) here;
    !hash_lookup(h, 5, match_pointee, &(int){7}) here;
    !hash_lookup(h, 7, match_pointee, &(int){7}) here;

    hash_insert(&h, 7, 7);
    hash_lookup(h, 5, match_pointee, &(int){5}) here;
    hash_lookup(h, 7, match_pointee, &(int){7}) here;
    !hash_lookup(h, 5, match_pointee, &(int){7}) here;
    !hash_lookup(h, 7, match_pointee, &(int){5}) here;

    free(h.data);
}

TEST(hash_zero_user_hash) {
    struct hash h = {0};
    hash_insert(&h, 0, 42);
    hash_lookup(h, 0, match_int, (void *)(intptr_t)42) here;
    free(h.data);
}

TEST(hash_collisions_small) { run_one(  100, zero);     }
TEST(hash_identity)         { run_one(10000, identity); }
TEST(hash_murmur)           { run_one(10000, murmur3_scramble); }
