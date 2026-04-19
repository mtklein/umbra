#include "fingerprint.h"
#include <string.h>

_Bool fingerprint_eq(fingerprint a, fingerprint b) {
    return a.lo == b.lo && a.hi == b.hi;
}

// 128-bit fingerprint for establishing buffer identity with very high
// probability (~2^-128 collision rate).  Speed is critical: we're replacing
// memcmp + memcpy of a full shadow buffer, so we must be faster than reading
// the data twice.
//
// On aarch64 we use the AES crypto extensions (vaeseq_u8 / vaesmcq_u8) as a
// mixing function.  4 parallel pipelines saturate memory bandwidth on Apple
// Silicon (~100+ GB/s on M4 Max).  Each round of AES is a bijective,
// non-linear mix — one round per 16-byte block is plenty for fingerprinting
// (not cryptography).
//
// On x86 and wasm we use a portable multiply-xor-shift hash inspired by
// wyhash.  64-bit multiplies are fast everywhere and 128-bit output gives
// the same collision resistance.

#if defined(__aarch64__) && defined(__ARM_FEATURE_AES)

#include <arm_neon.h>

fingerprint fingerprint_hash(void const *buf, size_t bytes) {
    uint8x16_t h0 = vdupq_n_u8(0),
               h1 = vdupq_n_u8(0),
               h2 = vdupq_n_u8(0),
               h3 = vdupq_n_u8(0);

    // Seed each pipeline with different constants so identical blocks in
    // different positions don't cancel.
    uint8x16_t const k0 = vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(0x243f6a8885a308d3),
                                                              vcreate_u64(0x13198a2e03707344))),
                     k1 = vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(0xa4093822299f31d0),
                                                              vcreate_u64(0x082efa98ec4e6c89))),
                     k2 = vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(0x452821e638d01377),
                                                              vcreate_u64(0xbe5466cf34e90c6c))),
                     k3 = vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(0xc0ac29b7c97c50dd),
                                                              vcreate_u64(0x3f84d5b5b5470917)));
    h0 = veorq_u8(h0, k0);
    h1 = veorq_u8(h1, k1);
    h2 = veorq_u8(h2, k2);
    h3 = veorq_u8(h3, k3);

    char const *p = buf;

    // Main loop: 4 × 16 = 64 bytes per iteration.
    size_t n64 = bytes / 64;
    for (size_t i = 0; i < n64; i++) {
        uint8x16_t d0 = vld1q_u8((uint8_t const *)(p +  0));
        uint8x16_t d1 = vld1q_u8((uint8_t const *)(p + 16));
        uint8x16_t d2 = vld1q_u8((uint8_t const *)(p + 32));
        uint8x16_t d3 = vld1q_u8((uint8_t const *)(p + 48));
        h0 = vaesmcq_u8(vaeseq_u8(h0, d0));
        h1 = vaesmcq_u8(vaeseq_u8(h1, d1));
        h2 = vaesmcq_u8(vaeseq_u8(h2, d2));
        h3 = vaesmcq_u8(vaeseq_u8(h3, d3));
        p += 64;
    }

    // Tail: up to 63 remaining bytes, one 16-byte block at a time.
    size_t tail = bytes - n64 * 64;
    while (tail >= 16) {
        uint8x16_t d = vld1q_u8((uint8_t const *)p);
        h0 = vaesmcq_u8(vaeseq_u8(h0, d));
        p += 16;
        tail -= 16;
    }
    if (tail) {
        uint8_t tmp[16] = {0};
        __builtin_memcpy(tmp, p, tail);
        h0 = vaesmcq_u8(vaeseq_u8(h0, vld1q_u8(tmp)));
    }

    // Mix the byte count in to distinguish buffers that differ only in
    // trailing zeros.
    {
        uint8_t size_buf[16] = {0};
        __builtin_memcpy(size_buf, &bytes, sizeof bytes);
        h0 = vaesmcq_u8(vaeseq_u8(h0, vld1q_u8(size_buf)));
    }

    // Fold the 4 pipelines together with a few extra AES rounds.
    h0 = veorq_u8(h0, h1);
    h2 = veorq_u8(h2, h3);
    h0 = veorq_u8(h0, h2);
    h0 = vaesmcq_u8(vaeseq_u8(h0, k0));
    h0 = vaesmcq_u8(vaeseq_u8(h0, k1));

    fingerprint out;
    vst1q_u8((uint8_t *)&out, h0);
    return out;
}

#else

// Portable path: multiply-xor-shift, inspired by wyhash.
// Uses 64×64→128 multiply as the mixing primitive.

static void mum(uint64_t *a, uint64_t *b) {
#if defined(__SIZEOF_INT128__)
    __uint128_t r = (__uint128_t)*a * *b;
    *a = (uint64_t)r;
    *b = (uint64_t)(r >> 64);
#else
    uint64_t ha = *a >> 32, la = (uint32_t)*a,
             hb = *b >> 32, lb = (uint32_t)*b;
    uint64_t rh = ha * hb,
             rm0 = ha * lb,
             rm1 = hb * la,
             rl = la * lb;
    uint64_t t = rl + (rm0 << 32);
    uint64_t c = t < rl;
    t += (rm1 << 32);
    c += t < (rm1 << 32);
    *a = t;
    *b = rh + (rm0 >> 32) + (rm1 >> 32) + c;
#endif
}

static uint64_t mix(uint64_t a, uint64_t b) { mum(&a, &b); return a ^ b; }

static uint64_t r8(void const *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static uint64_t r4(void const *p) { uint32_t v; memcpy(&v, p, 4); return v; }

fingerprint fingerprint_hash(void const *buf, size_t bytes) {
    uint64_t const s0 = 0xa0761d6478bd642f,
                   s1 = 0xe7037ed1a0b428db,
                   s2 = 0x8ebc6af09c88c6e3,
                   s3 = 0x589965cc75374cc3;

    char const *p = buf;
    uint64_t a = (uint64_t)bytes ^ s0,
             b = s1;

    if (bytes >= 32) {
        uint64_t h0 = a, h1 = b, h2 = s2, h3 = s3;
        size_t n32 = bytes / 32;
        for (size_t i = 0; i < n32; i++) {
            h0 = mix(r8(p +  0) ^ s0, r8(p +  8) ^ h0);
            h1 = mix(r8(p + 16) ^ s1, r8(p + 24) ^ h1);
            p += 32;
        }
        a = h0 ^ h2;
        b = h1 ^ h3;
    }

    size_t tail = bytes & 31;
    while (tail >= 8) {
        a = mix(r8(p) ^ s2, a);
        p += 8;
        tail -= 8;
    }
    if (tail >= 4) {
        b ^= r4(p) | ((uint64_t)r4(p + tail - 4) << 32);
        tail = 0;
    } else if (tail) {
        unsigned char const *u = (unsigned char const *)p;
        uint64_t v = (uint64_t)u[0]
                  | ((uint64_t)u[tail >> 1] << 8)
                  | ((uint64_t)u[tail -  1] << 16);
        b ^= v;
        tail = 0;
    }
    (void)tail;

    a = mix(a, b ^ s2);
    b = mix(a, b ^ s3);
    return (fingerprint){a, b};
}

#endif
