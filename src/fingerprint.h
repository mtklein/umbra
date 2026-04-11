#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct { uint64_t lo, hi; } fingerprint;

fingerprint fingerprint_hash(void const *buf, size_t bytes);

static inline _Bool fingerprint_eq(fingerprint a, fingerprint b) {
    return a.lo == b.lo && a.hi == b.hi;
}
