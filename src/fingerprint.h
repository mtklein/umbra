#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct { uint64_t lo, hi; } fingerprint;

// TODO: is there a way to reduce this API as used into a single compute-and-compare?
fingerprint fingerprint_hash(void const *buf, size_t bytes);
_Bool fingerprint_eq(fingerprint, fingerprint);
