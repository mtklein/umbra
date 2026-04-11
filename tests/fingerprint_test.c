#include "../src/fingerprint.h"
#include "test.h"
#include <string.h>

TEST(test_fingerprint_empty) {
    fingerprint a = fingerprint_hash(NULL, 0);
    fingerprint b = fingerprint_hash(NULL, 0);
    fingerprint_eq(a, b) here;
}

TEST(test_fingerprint_deterministic) {
    char buf[256];
    memset(buf, 0x42, sizeof buf);
    fingerprint a = fingerprint_hash(buf, sizeof buf);
    fingerprint b = fingerprint_hash(buf, sizeof buf);
    fingerprint_eq(a, b) here;
}

TEST(test_fingerprint_change_detected) {
    char buf[256];
    memset(buf, 0, sizeof buf);
    fingerprint a = fingerprint_hash(buf, sizeof buf);

    buf[0] = 1;
    fingerprint b = fingerprint_hash(buf, sizeof buf);
    !fingerprint_eq(a, b) here;

    buf[0] = 0;
    buf[255] = 1;
    fingerprint c = fingerprint_hash(buf, sizeof buf);
    !fingerprint_eq(a, c) here;
    !fingerprint_eq(b, c) here;
}

TEST(test_fingerprint_length_matters) {
    // Two all-zero buffers of different lengths must hash differently.
    char buf[64] = {0};
    fingerprint a = fingerprint_hash(buf, 32);
    fingerprint b = fingerprint_hash(buf, 33);
    !fingerprint_eq(a, b) here;
}

TEST(test_fingerprint_all_sizes) {
    // Sweep sizes 0..130 to exercise tail handling at every alignment.
    char buf[130];
    for (int i = 0; i < (int)sizeof buf; i++) { buf[i] = (char)(i * 7 + 13); }
    fingerprint prev = fingerprint_hash(buf, 0);
    for (int sz = 1; sz <= (int)sizeof buf; sz++) {
        fingerprint cur = fingerprint_hash(buf, (size_t)sz);
        !fingerprint_eq(cur, prev) here;
        prev = cur;
    }
}

TEST(test_fingerprint_large) {
    // 1 MB buffer.
    enum { SZ = 1 << 20 };
    char *buf = malloc(SZ);
    for (int i = 0; i < SZ; i++) { buf[i] = (char)(i ^ (i >> 8)); }
    fingerprint a = fingerprint_hash(buf, SZ);
    fingerprint b = fingerprint_hash(buf, SZ);
    fingerprint_eq(a, b) here;

    // Flip one bit in the middle.
    buf[SZ / 2] ^= 1;
    fingerprint c = fingerprint_hash(buf, SZ);
    !fingerprint_eq(a, c) here;

    free(buf);
}
