#define _POSIX_C_SOURCE 199309L

#include "vm.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wdeclaration-after-statement" // C11, not C89
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"    // WASI stderr macro

// ============================================================
// Minimal test harness
// ============================================================

static int g_failed = 0;

static void check_f32(const char *name, float got, float want, float tol) {
    float diff = got - want;
    if (diff < 0.0f) { diff = -diff; }
    if (diff > tol) {
        fprintf(stderr, "FAIL %s: got %.6f want %.6f (diff %.6f > tol %.6f)\n",
                name, (double)got, (double)want, (double)diff, (double)tol);
        g_failed = 1;
    }
}

static void check_i32(const char *name, int32_t got, int32_t want) {
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
        g_failed = 1;
    }
}

// ============================================================
// Test: f32 arithmetic (uniform)
// ============================================================

static void test_f32_uniform(void) {
    vm_Program *p = vm_program();

    vm_V32 a = vm_immf(p, 3.0f);
    vm_V32 b = vm_immf(p, 2.0f);

    vm_V32 add  = vm_addf(p, a, b);
    vm_V32 sub  = vm_subf(p, a, b);
    vm_V32 mul  = vm_mulf(p, a, b);
    vm_V32 div  = vm_divf(p, a, b);
    vm_V32 fma  = vm_fmaf(p, a, b, vm_immf(p, 1.0f)); // 3*2+1 = 7
    vm_V32 neg  = vm_negf(p, a);
    vm_V32 abs_ = vm_absf(p, vm_negf(p, b));
    vm_V32 mn   = vm_minf(p, a, b);
    vm_V32 mx   = vm_maxf(p, a, b);
    vm_V32 sq   = vm_sqrtf(p, b);
    vm_V32 fl   = vm_floorf(p, vm_immf(p, 2.7f));
    vm_V32 cl   = vm_ceilf(p, vm_immf(p, 2.1f));

    // Store all results into a float array
    vm_Ptr dst = vm_ptr(p);
    vm_varying_store32(p, dst, 0,  4, add);
    vm_varying_store32(p, dst, 4,  4, sub);
    vm_varying_store32(p, dst, 8,  4, mul);
    vm_varying_store32(p, dst, 12, 4, div);
    vm_varying_store32(p, dst, 16, 4, fma);
    vm_varying_store32(p, dst, 20, 4, neg);
    vm_varying_store32(p, dst, 24, 4, abs_);
    vm_varying_store32(p, dst, 28, 4, mn);
    vm_varying_store32(p, dst, 32, 4, mx);
    vm_varying_store32(p, dst, 36, 4, sq);
    vm_varying_store32(p, dst, 40, 4, fl);
    vm_varying_store32(p, dst, 44, 4, cl);

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    float out[12];
    vm_Binding b0 = { out, dst.id, 0 };
    vm_run(c, &b0, 1, 1);
    vm_compiled_free(c);

    check_f32("addf",  out[0],  5.0f, 1e-6f);
    check_f32("subf",  out[1],  1.0f, 1e-6f);
    check_f32("mulf",  out[2],  6.0f, 1e-6f);
    check_f32("divf",  out[3],  1.5f, 1e-6f);
    check_f32("fmaf",  out[4],  7.0f, 1e-6f);
    check_f32("negf",  out[5], -3.0f, 1e-6f);
    check_f32("absf",  out[6],  2.0f, 1e-6f);
    check_f32("minf",  out[7],  2.0f, 1e-6f);
    check_f32("maxf",  out[8],  3.0f, 1e-6f);
    check_f32("sqrtf", out[9],  sqrtf(2.0f), 1e-6f);
    check_f32("floorf",out[10], 2.0f, 1e-6f);
    check_f32("ceilf", out[11], 3.0f, 1e-6f);
}

// ============================================================
// Test: i32 arithmetic (uniform)
// ============================================================

static void test_i32_uniform(void) {
    vm_Program *p = vm_program();

    vm_V32 a  = vm_immi(p,  7);
    vm_V32 b  = vm_immi(p,  3);
    vm_V32 bn = vm_immi(p, -3);
    vm_V32 sh = vm_immi(p,  2);

    vm_V32 add  = vm_addi(p, a, b);
    vm_V32 sub  = vm_subi(p, a, b);
    vm_V32 mul  = vm_muli(p, a, b);
    vm_V32 neg  = vm_negi(p, a);
    vm_V32 abs_ = vm_absi(p, bn);
    vm_V32 mn   = vm_mini(p, a, b);
    vm_V32 mx   = vm_maxi(p, a, b);
    vm_V32 and_ = vm_andi(p, a, b);     // 7&3 = 3
    vm_V32 or_  = vm_ori (p, a, b);     // 7|3 = 7
    vm_V32 xor_ = vm_xori(p, a, b);     // 7^3 = 4
    vm_V32 shl  = vm_shli(p, b, sh);    // 3<<2 = 12
    vm_V32 shr  = vm_shri(p, a, sh);    // 7>>2 = 1
    vm_V32 sar  = vm_sari(p, bn, sh);   // -3>>2 = -1

    vm_Ptr dst = vm_ptr(p);
    vm_varying_store32(p, dst, 0,  4, add);
    vm_varying_store32(p, dst, 4,  4, sub);
    vm_varying_store32(p, dst, 8,  4, mul);
    vm_varying_store32(p, dst, 12, 4, neg);
    vm_varying_store32(p, dst, 16, 4, abs_);
    vm_varying_store32(p, dst, 20, 4, mn);
    vm_varying_store32(p, dst, 24, 4, mx);
    vm_varying_store32(p, dst, 28, 4, and_);
    vm_varying_store32(p, dst, 32, 4, or_);
    vm_varying_store32(p, dst, 36, 4, xor_);
    vm_varying_store32(p, dst, 40, 4, shl);
    vm_varying_store32(p, dst, 44, 4, shr);
    vm_varying_store32(p, dst, 48, 4, sar);

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    int32_t out[13];
    vm_Binding b0 = { out, dst.id, 0 };
    vm_run(c, &b0, 1, 1);
    vm_compiled_free(c);

    check_i32("addi",  out[0],  10);
    check_i32("subi",  out[1],   4);
    check_i32("muli",  out[2],  21);
    check_i32("negi",  out[3],  -7);
    check_i32("absi",  out[4],   3);
    check_i32("mini",  out[5],   3);
    check_i32("maxi",  out[6],   7);
    check_i32("andi",  out[7],   3);
    check_i32("ori",   out[8],   7);
    check_i32("xori",  out[9],   4);
    check_i32("shli",  out[10], 12);
    check_i32("shri",  out[11],  1);
    check_i32("sari",  out[12], -1);
}

// ============================================================
// Test: f16 arithmetic (uniform)
// ============================================================

static void test_f16_uniform(void) {
    vm_Program *p = vm_program();

    vm_V16 a = vm_immf16(p, 3.0f);
    vm_V16 b = vm_immf16(p, 2.0f);

    vm_V16 add = vm_addf16(p, a, b);
    vm_V16 sub = vm_subf16(p, a, b);
    vm_V16 mul = vm_mulf16(p, a, b);
    vm_V16 div = vm_divf16(p, a, b);
    vm_V16 fma = vm_fmaf16(p, a, b, vm_immf16(p, 1.0f)); // 3*2+1 = 7
    vm_V16 neg = vm_negf16(p, a);
    vm_V16 mn  = vm_minf16(p, a, b);
    vm_V16 mx  = vm_maxf16(p, a, b);
    vm_V16 sq  = vm_sqrtf16(p, b);

    // Convert to f32 for storage (easier to check as float array)
    vm_Ptr dst = vm_ptr(p);
    vm_varying_store32(p, dst, 0,  4, vm_f16_to_f32(p, add));
    vm_varying_store32(p, dst, 4,  4, vm_f16_to_f32(p, sub));
    vm_varying_store32(p, dst, 8,  4, vm_f16_to_f32(p, mul));
    vm_varying_store32(p, dst, 12, 4, vm_f16_to_f32(p, div));
    vm_varying_store32(p, dst, 16, 4, vm_f16_to_f32(p, fma));
    vm_varying_store32(p, dst, 20, 4, vm_f16_to_f32(p, neg));
    vm_varying_store32(p, dst, 24, 4, vm_f16_to_f32(p, mn));
    vm_varying_store32(p, dst, 28, 4, vm_f16_to_f32(p, mx));
    vm_varying_store32(p, dst, 32, 4, vm_f16_to_f32(p, sq));

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    float out[9];
    vm_Binding b0 = { out, dst.id, 0 };
    vm_run(c, &b0, 1, 1);
    vm_compiled_free(c);

    // f16 has ~3 decimal digits of precision; use generous tolerance
    check_f32("addf16",  out[0],  5.0f,      0.01f);
    check_f32("subf16",  out[1],  1.0f,      0.01f);
    check_f32("mulf16",  out[2],  6.0f,      0.01f);
    check_f32("divf16",  out[3],  1.5f,      0.01f);
    check_f32("fmaf16",  out[4],  7.0f,      0.01f);
    check_f32("negf16",  out[5], -3.0f,      0.01f);
    check_f32("minf16",  out[6],  2.0f,      0.01f);
    check_f32("maxf16",  out[7],  3.0f,      0.01f);
    check_f32("sqrtf16", out[8],  sqrtf(2.0f), 0.01f);
}

// ============================================================
// Test: lane_id and varying loads/stores
// ============================================================

#define N_LANES 16

static void test_varying(void) {
    vm_Program *p = vm_program();
    vm_Ptr src = vm_ptr(p);
    vm_Ptr dst = vm_ptr(p);

    // Load lane index as i32, also load from src[], multiply by 2, store
    vm_V32 id   = vm_lane_id(p);
    vm_V32 load = vm_varying_load32(p, src, 0, 4);
    vm_V32 two  = vm_immi(p, 2);
    vm_V32 res  = vm_muli(p, load, two);
    vm_varying_store32(p, dst, 0, 4, id);   // store lane index to dst[0..N-1]
    vm_varying_store32(p, dst, N_LANES*4, 4, res); // store doubled values after

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    int32_t in_buf[N_LANES];
    int32_t out_buf[N_LANES * 2];
    for (int i = 0; i < N_LANES; i++) { in_buf[i] = i * 10; }

    vm_Binding bindings[2] = {
        { in_buf,  src.id, 0 },
        { out_buf, dst.id, 0 },
    };
    vm_run(c, bindings, 2, N_LANES);
    vm_compiled_free(c);

    for (int i = 0; i < N_LANES; i++) {
        check_i32("lane_id",   out_buf[i],           i);
        check_i32("varying*2", out_buf[N_LANES + i], i * 20);
    }
}

// ============================================================
// Test: uniform load (broadcast)
// ============================================================

static void test_uniform_load(void) {
    vm_Program *p = vm_program();
    vm_Ptr src = vm_ptr(p);
    vm_Ptr dst = vm_ptr(p);

    vm_V32 val = vm_uniform_load32(p, src, 0);   // broadcast single value
    vm_V32 id  = vm_lane_id(p);
    vm_V32 res = vm_addi(p, val, id);             // val + lane_id (varying result)
    vm_varying_store32(p, dst, 0, 4, res);

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    int32_t base = 100;
    int32_t out_buf[N_LANES];
    vm_Binding bindings[2] = {
        { &base,   src.id, 0 },
        { out_buf, dst.id, 0 },
    };
    vm_run(c, bindings, 2, N_LANES);
    vm_compiled_free(c);

    for (int i = 0; i < N_LANES; i++) {
        check_i32("uniform_load", out_buf[i], 100 + i);
    }
}

// ============================================================
// Test: gather (per-lane indexed load)
// ============================================================

static void test_gather(void) {
    vm_Program *p = vm_program();
    vm_Ptr tbl = vm_ptr(p);
    vm_Ptr dst = vm_ptr(p);

    // Indices: lane i accesses table[N_LANES-1-i] (reverse order)
    vm_V32 id      = vm_lane_id(p);
    vm_V32 n_minus = vm_immi(p, N_LANES - 1);
    vm_V32 rev     = vm_subi(p, n_minus, id);       // N-1-i
    vm_V32 byte_off= vm_muli(p, rev, vm_immi(p, 4)); // byte offset
    vm_V32 val     = vm_varying_gather32(p, tbl, byte_off);
    vm_varying_store32(p, dst, 0, 4, val);

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    int32_t tbl_buf[N_LANES];
    int32_t out_buf[N_LANES];
    for (int i = 0; i < N_LANES; i++) { tbl_buf[i] = i * 7; }

    vm_Binding bindings[2] = {
        { tbl_buf, tbl.id, 0 },
        { out_buf, dst.id, 0 },
    };
    vm_run(c, bindings, 2, N_LANES);
    vm_compiled_free(c);

    for (int i = 0; i < N_LANES; i++) {
        check_i32("gather", out_buf[i], (N_LANES - 1 - i) * 7);
    }
}

// ============================================================
// Test: comparisons and select
// ============================================================

static void test_cmp_sel(void) {
    vm_Program *p = vm_program();
    vm_Ptr src = vm_ptr(p);
    vm_Ptr dst = vm_ptr(p);

    vm_V32 x    = vm_varying_load32(p, src, 0, 4);
    vm_V32 zero = vm_immf(p, 0.0f);
    vm_V32 one  = vm_immf(p, 1.0f);
    vm_V32 neg1 = vm_immf(p, -1.0f);

    // sign(x): -1, 0, or +1
    vm_V32 pos  = vm_ltf(p, zero, x);          // 0 < x  → mask
    vm_V32 neg  = vm_ltf(p, x, zero);          // x < 0  → mask
    vm_V32 r    = vm_sel32(p, pos, one, vm_sel32(p, neg, neg1, zero));
    vm_varying_store32(p, dst, 0, 4, r);

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    float in_buf[N_LANES];
    float out_buf[N_LANES];
    for (int i = 0; i < N_LANES; i++) {
        in_buf[i] = (float)(i - N_LANES/2);   // -8 .. 7
    }

    vm_Binding bindings[2] = {
        { in_buf,  src.id, 0 },
        { out_buf, dst.id, 0 },
    };
    vm_run(c, bindings, 2, N_LANES);
    vm_compiled_free(c);

    for (int i = 0; i < N_LANES; i++) {
        float x_val = in_buf[i];
        float want  = x_val > 0.0f ? 1.0f : (x_val < 0.0f ? -1.0f : 0.0f);
        check_f32("sign", out_buf[i], want, 1e-6f);
    }
}

// ============================================================
// Test: conversions
// ============================================================

static void test_conversions(void) {
    vm_Program *p = vm_program();
    vm_Ptr dst = vm_ptr(p);

    // f32 → f16 → f32 round-trip
    vm_V32 f32v  = vm_immf(p, 1.5f);
    vm_V16 f16v  = vm_f32_to_f16(p, f32v);
    vm_V32 back  = vm_f16_to_f32(p, f16v);

    // f32 → i32 → f32 round-trip
    vm_V32 fv    = vm_immf(p, -7.9f);
    vm_V32 iv    = vm_f32_to_i32(p, fv);     // truncate → -7
    vm_V32 fback = vm_i32_to_f32(p, iv);

    // i32 → i16 → i32 (sign-extend)
    vm_V32 big   = vm_immi(p, 0x12345);     // 0x12345 → i16 = 0x2345 = 9029
    vm_V16 small = vm_i32_to_i16(p, big);
    vm_V32 ext   = vm_i16_to_i32(p, small);

    vm_varying_store32(p, dst, 0,  4, back);
    vm_varying_store32(p, dst, 4,  4, iv);
    vm_varying_store32(p, dst, 8,  4, fback);
    vm_varying_store32(p, dst, 12, 4, ext);

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    // Use a byte buffer; memcpy to extract typed values without aliasing UB.
    uint8_t raw[16];
    vm_Binding b0 = { raw, dst.id, 0 };
    vm_run(c, &b0, 1, 1);
    vm_compiled_free(c);

    float   f0, f2;
    int32_t i1, i3;
    memcpy(&f0, raw +  0, 4);
    memcpy(&i1, raw +  4, 4);
    memcpy(&f2, raw +  8, 4);
    memcpy(&i3, raw + 12, 4);

    check_f32("f16_roundtrip", f0, 1.5f,  0.001f);
    check_i32("f32_to_i32",    i1, -7);
    check_f32("i32_to_f32",    f2, -7.0f, 1e-6f);
    check_i32("i32_i16_i32",   i3, 0x2345);
}

// ============================================================
// Test: f16 varying load/store + arithmetic
// ============================================================

static void test_f16_varying(void) {
    vm_Program *p = vm_program();
    vm_Ptr src = vm_ptr(p);
    vm_Ptr dst = vm_ptr(p);

    vm_V16 a = vm_varying_load16(p, src, 0, 2);
    vm_V16 b = vm_varying_load16(p, src, N_LANES * 2, 2);
    vm_V16 r = vm_fmaf16(p, a, b, vm_immf16(p, 0.5f));
    vm_varying_store16(p, dst, 0, 2, r);

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    // Encode f16 inputs via the vm itself (lazy: use host _Float16 directly)
    uint16_t in_buf[N_LANES * 2];
    uint16_t out_buf[N_LANES];

    // Build simple test values: a[i] = i+1 as f16, b[i] = 0.5 as f16
    // Use the vm_program helper to get bit patterns
    for (int i = 0; i < N_LANES; i++) {
        // Encode (i+1) as f16 bits using a tiny helper program
        vm_Program *enc = vm_program();
        vm_Ptr enc_dst = vm_ptr(enc);
        vm_V16 v = vm_immf16(enc, (float)(i + 1));
        vm_varying_store16(enc, enc_dst, 0, 2, v);
        vm_Compiled *enc_c = vm_compile(enc, VM_BACKEND_INTERP);
        vm_program_free(enc);
        vm_Binding eb = { &in_buf[i], enc_dst.id, 0 };
        vm_run(enc_c, &eb, 1, 1);
        vm_compiled_free(enc_c);

        vm_Program *enc2 = vm_program();
        vm_Ptr enc_dst2 = vm_ptr(enc2);
        vm_V16 v2 = vm_immf16(enc2, 0.5f);
        vm_varying_store16(enc2, enc_dst2, 0, 2, v2);
        vm_Compiled *enc_c2 = vm_compile(enc2, VM_BACKEND_INTERP);
        vm_program_free(enc2);
        vm_Binding eb2 = { &in_buf[N_LANES + i], enc_dst2.id, 0 };
        vm_run(enc_c2, &eb2, 1, 1);
        vm_compiled_free(enc_c2);
    }

    vm_Binding bindings[2] = {
        { in_buf,  src.id, 0 },
        { out_buf, dst.id, 0 },
    };
    vm_run(c, bindings, 2, N_LANES);
    vm_compiled_free(c);

    // Decode results and check: (i+1)*0.5 + 0.5
    for (int i = 0; i < N_LANES; i++) {
        vm_Program *dec = vm_program();
        vm_Ptr dec_src = vm_ptr(dec);
        vm_Ptr dec_dst = vm_ptr(dec);
        vm_V16 rv = vm_varying_load16(dec, dec_src, 0, 2);
        vm_varying_store32(dec, dec_dst, 0, 4, vm_f16_to_f32(dec, rv));
        vm_Compiled *dec_c = vm_compile(dec, VM_BACKEND_INTERP);
        vm_program_free(dec);
        float decoded;
        vm_Binding db[2] = {
            { &out_buf[i], dec_src.id, 0 },
            { &decoded,    dec_dst.id, 0 },
        };
        vm_run(dec_c, db, 2, 1);
        vm_compiled_free(dec_c);

        float want = (float)(i + 1) * 0.5f + 0.5f;
        check_f32("f16_varying_fma", decoded, want, 0.02f);
    }
}

// ============================================================
// Benchmark: RGBA alpha-blend
//
// dst.rgba = src.rgba * src.a + dst.rgba * (1 - src.a)
// All channels f16.  N pixels in each buffer.
// ============================================================

#define BENCH_N (1 << 20)  // 1M pixels

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

// RGBA pixel layout: r,g,b,a interleaved (8 bytes per pixel as 4×f16)
static void bench_alpha_blend(void) {
    vm_Program *p = vm_program();
    vm_Ptr src_ptr = vm_ptr(p);
    vm_Ptr dst_ptr = vm_ptr(p);

    // Load src RGBA: stride = 8 bytes (4 f16 channels), channels at offsets 0,2,4,6
    vm_V16 sr = vm_varying_load16(p, src_ptr, 0, 8);
    vm_V16 sg = vm_varying_load16(p, src_ptr, 2, 8);
    vm_V16 sb = vm_varying_load16(p, src_ptr, 4, 8);
    vm_V16 sa = vm_varying_load16(p, src_ptr, 6, 8);

    // Load dst RGBA
    vm_V16 dr = vm_varying_load16(p, dst_ptr, 0, 8);
    vm_V16 dg = vm_varying_load16(p, dst_ptr, 2, 8);
    vm_V16 db = vm_varying_load16(p, dst_ptr, 4, 8);

    // one_minus_sa = 1.0 - sa
    vm_V16 one      = vm_immf16(p, 1.0f);
    vm_V16 inv_sa   = vm_subf16(p, one, sa);

    // out = src * sa + dst * inv_sa  (using fma: dst * inv_sa + src * sa)
    vm_V16 out_r = vm_fmaf16(p, dr, inv_sa, vm_mulf16(p, sr, sa));
    vm_V16 out_g = vm_fmaf16(p, dg, inv_sa, vm_mulf16(p, sg, sa));
    vm_V16 out_b = vm_fmaf16(p, db, inv_sa, vm_mulf16(p, sb, sa));

    vm_varying_store16(p, dst_ptr, 0, 8, out_r);
    vm_varying_store16(p, dst_ptr, 2, 8, out_g);
    vm_varying_store16(p, dst_ptr, 4, 8, out_b);
    vm_varying_store16(p, dst_ptr, 6, 8, sa);     // keep src alpha

    vm_Compiled *c = vm_compile(p, VM_BACKEND_INTERP);
    vm_program_free(p);

    uint16_t *src_buf = (uint16_t *)malloc(BENCH_N * 8);
    uint16_t *dst_buf = (uint16_t *)malloc(BENCH_N * 8);

    // Fill with simple test data (doesn't matter for timing)
    for (int i = 0; i < BENCH_N * 4; i++) {
        src_buf[i] = 0x3800; // 0.5 in f16
        dst_buf[i] = 0x3800;
    }

    // Warm up
    vm_Binding bindings[2] = {
        { src_buf, src_ptr.id, 0 },
        { dst_buf, dst_ptr.id, 0 },
    };
    vm_run(c, bindings, 2, BENCH_N);

    // Time several runs
    int n_runs = 4;
    double best = 1e30;
    for (int run = 0; run < n_runs; run++) {
        double t0 = now_s();
        vm_run(c, bindings, 2, BENCH_N);
        double t1 = now_s();
        double elapsed = t1 - t0;
        if (elapsed < best) { best = elapsed; }
    }

    vm_compiled_free(c);
    free(src_buf);
    free(dst_buf);

    double mpix = (double)BENCH_N / (best * 1e6);
    printf("alpha_blend: %.1f Mpix/s  (%.1f ns/pix, interp, %d pixels)\n",
           mpix, best * 1e9 / (double)BENCH_N, BENCH_N);
}

// ============================================================
// main
// ============================================================

int main(void) {
    test_f32_uniform();
    test_i32_uniform();
    test_f16_uniform();
    test_varying();
    test_uniform_load();
    test_gather();
    test_cmp_sel();
    test_conversions();
    test_f16_varying();

    if (g_failed) {
        fprintf(stderr, "FAILED\n");
        return 1;
    }
    printf("all tests passed\n");

    bench_alpha_blend();
    return 0;
}
