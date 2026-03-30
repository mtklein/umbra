// Minimal repro: pack_float_to_unorm4x8 vs CPU rounding strategies
//
// The multiply blend produces alpha = 0x3f404040 (0.7509803772).
// Mathematically: 0.7509803772 * 255 = 191.4999961853...
// The correct integer is 191.
//
// CPU fma path:  fmaf(v, 255, 0.5) = 192.0 (float32)
//                trunc(192.0) = 192           ← WRONG (double-rounded)
//
// Metal path:    pack_float_to_unorm4x8 → 191  ← CORRECT
//
// Why?  The FMA computes the exact sum 191.9999961853... then rounds
// to float32.  The nearest float32 is 192.0 (3.8e-6 away) vs
// 191.99998 (1.5e-5 away), so IEEE rounds to 192.0.
//
// pack_float_to_unorm4x8 never forms a float32 intermediate — it
// sees the true product 191.4999961853... and rounds directly to 191.
//
// Dekker fix:  Use FMA to recover the rounding error of the multiply,
// then decide which side of the midpoint we're on.
//
//   float hi   = v * 255;              // 191.5 (rounded up)
//   float lo   = fma(v, 255, -hi);     // -0.0000038... (exact error)
//   int   n    = trunc(hi);            // 191
//   float frac = (hi - float(n)) + lo; // 0.4999961... (true fractional part)
//   int result = n + (frac >= 0.5);    // 191 ✓
//
// Build: clang -fobjc-arc -framework Metal -framework Foundation
//        tools/pack_repro.m -o pack_repro

#import <Metal/Metal.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float    u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

// Dekker-corrected rounding: matches pack_float_to_unorm4x8
static int pack_unorm8_dekker(float v, float scale) {
    float hi   = v * scale;
    float lo   = fmaf(v, scale, -hi);
    int   n    = (int)hi;
    float frac = (hi - (float)n) + lo;
    return n + (frac >= 0.5f);
}

// fma(x, 256, 0.5 - x): since x*256 is exact (power of 2),
// all the rounding pressure is on the 0.5-x addend.
static int pack_unorm8_256(float v) {
    return (int)fmaf(v, 256.0f, 0.5f - v);
}

// fma(x, 256, magic - x): adds 2^23 to force ULP=1, so the
// FMA's single rounding lands the mantissa bits on the integer.
// No separate truncation/rounding step needed.
static int pack_unorm8_magic(float v) {
    float const magic = 8388608.0f;
    float val = fmaf(v, 256.0f, magic - v);
    uint32_t bits;
    memcpy(&bits, &val, 4);
    return (int)(uint8_t)bits;
}

static NSString *shader = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"kernel void test(device uint *out [[buffer(0)]],\n"
"                 device uint *in  [[buffer(1)]],\n"
"                 uint tid [[thread_position_in_grid]]) {\n"
"    float v = as_type<float>(in[tid]);\n"
"    float scale = 255.0;\n"
"\n"
"    // Method A: pack builtin (ground truth)\n"
"    out[tid*5 + 0] = pack_float_to_unorm4x8(float4(v,v,v,v)) & 0xFF;\n"
"\n"
"    // Method B: fma + trunc (current CPU path)\n"
"    out[tid*5 + 1] = uint(trunc(fma(v, scale, 0.5)));\n"
"\n"
"    // Method C: rint\n"
"    out[tid*5 + 2] = uint(rint(v * scale));\n"
"\n"
"    // Method D: Dekker-corrected on GPU\n"
"    float hi   = v * scale;\n"
"    float lo   = fma(v, scale, -hi);\n"
"    int   n    = int(hi);\n"
"    float frac = (hi - float(n)) + lo;\n"
"    out[tid*5 + 3] = uint(n + int(frac >= 0.5));\n"
"\n"
"    // Method E: integer mantissa approach\n"
"    uint bits = as_type<uint>(v);\n"
"    uint m    = (bits & 0x7FFFFFu) | 0x800000u;\n"
"    int  sh   = 150 - int((bits >> 23) & 0xFFu);\n"
"    uint prod = m * 255u;\n"
"    out[tid*5 + 4] = (sh >= 1 && sh <= 31)\n"
"                   ? (prod + (1u << uint(sh - 1))) >> uint(sh)\n"
"                   : 0u;\n"
"}\n";

int main(void) {
    @autoreleasepool {
        // --- Single value deep-dive ---
        float v = u2f(0x3f404040);
        printf("=== Deep dive: v = 0x%08x = %.20e ===\n\n", f2u(v), v);

        double exact = (double)v * 255.0;
        printf("  Exact product (f64):  %.20e\n", exact);
        printf("  float32 product:      %.10e  (0x%08x)\n",
               (double)(v * 255.0f), f2u(v * 255.0f));
        printf("  Correct integer:      %d\n\n", (int)exact);

        printf("  fma(v,255,0.5)+trunc: %d  (double-rounded)\n",
               (int)truncf(fmaf(v, 255, 0.5f)));
        printf("  rint(v*255):          %d  (double-rounded)\n",
               (int)rintf(v * 255.0f));
        printf("  Dekker-corrected:     %d  ✓\n",
               pack_unorm8_dekker(v, 255.0f));
        printf("  fma(v,256,0.5-v):     %d\n",
               pack_unorm8_256(v));
        printf("  fma(v,256,magic-v):   %d\n\n",
               pack_unorm8_magic(v));

        // Show Dekker intermediates
        {
            float hi = v * 255.0f;
            float lo = fmaf(v, 255.0f, -hi);
            int   n  = (int)hi;
            float frac = (hi - (float)n) + lo;
            printf("  Dekker intermediates:\n");
            printf("    hi   = %.10e  (v*255 in float32)\n", hi);
            printf("    lo   = %.10e  (exact rounding error)\n", lo);
            printf("    n    = %d  (trunc of hi)\n", n);
            printf("    frac = %.10e  (true fractional part)\n", frac);
            printf("    frac >= 0.5? %s\n\n", frac >= 0.5f ? "yes → round up" : "no → keep n");
        }

        // --- Exhaustive comparison over all float32 in [0,1] ---
        printf("=== Exhaustive CPU scan: all float32 in [0,1] ===\n");
        int fma_wrong = 0, rint_wrong = 0, dekker_wrong = 0, f256_wrong = 0, magic_wrong = 0;
        uint32_t fma_ex = 0, rint_ex = 0, f256_ex = 0, magic_ex = 0;
        for (uint32_t bits = 0; bits <= 0x3F800000u; bits++) {
            float f = u2f(bits);
            if (f != f || f < 0 || f > 1) continue;
            int correct = (int)((double)f * 255.0);
            double rem  = (double)f * 255.0 - correct;
            if (rem >= 0.5) correct++;

            int fma_r   = (int)truncf(fmaf(f, 255, 0.5f));
            int rint_r  = (int)rintf(f * 255.0f);
            int dek_r   = pack_unorm8_dekker(f, 255.0f);
            int f256_r  = pack_unorm8_256(f);
            int magic_r = pack_unorm8_magic(f);

            if (fma_r != correct)   { fma_wrong++;   if (!fma_ex)   fma_ex   = bits; }
            if (rint_r != correct)  { rint_wrong++;  if (!rint_ex)  rint_ex  = bits; }
            if (dek_r != correct)   { dekker_wrong++; }
            if (f256_r != correct)  { f256_wrong++;  if (!f256_ex)  f256_ex  = bits; }
            if (magic_r != correct) { magic_wrong++; if (!magic_ex) magic_ex = bits; }
        }
        printf("  fma+trunc mismatches:    %d", fma_wrong);
        if (fma_ex)   printf("  (first: 0x%08x)", fma_ex);
        printf("\n");
        printf("  rint mismatches:         %d", rint_wrong);
        if (rint_ex)  printf("  (first: 0x%08x)", rint_ex);
        printf("\n");
        printf("  fma(v,256,0.5-v):       %d", f256_wrong);
        if (f256_ex)  printf("  (first: 0x%08x)", f256_ex);
        printf("\n");
        printf("  fma(v,256,magic-v):      %d", magic_wrong);
        if (magic_ex) printf("  (first: 0x%08x)", magic_ex);
        printf("\n");
        printf("  Dekker mismatches:       %d\n", dekker_wrong);
        printf("  (out of ~1065353216 values in [0,1])\n");

        // --- GPU comparison ---
        printf("\n=== GPU comparison: pack builtin vs all methods ===\n");
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (!dev) { printf("no metal\n"); return 1; }
        NSError *err = nil;
        id<MTLLibrary> lib = [dev newLibraryWithSource:shader options:nil error:&err];
        if (!lib) { printf("compile: %s\n", err.localizedDescription.UTF8String); return 1; }
        id<MTLFunction> fn = [lib newFunctionWithName:@"test"];
        id<MTLComputePipelineState> pso = [dev newComputePipelineStateWithFunction:fn error:&err];

        // Test a selection of interesting values
        uint32_t test_values[] = {
            0x00000000,  // 0.0
            0x3f800000,  // 1.0
            0x3f404040,  // 0.75098... (the multiply blend case)
            0x3f000000,  // 0.5
            0x3e800000,  // 0.25
            0x3f008081,  // 128/255 (unorm8 midpoint candidate)
            0x3e808081,  // 128/255 * 0.5
            0x3f7f7f80,  // 254.5/255 (another midpoint)
        };
        int N = (int)(sizeof test_values / sizeof *test_values);

        id<MTLBuffer> in_buf  = [dev newBufferWithLength:(NSUInteger)(N*4) options:MTLResourceStorageModeShared];
        id<MTLBuffer> out_buf = [dev newBufferWithLength:(NSUInteger)(N*5*4) options:MTLResourceStorageModeShared];
        memcpy(in_buf.contents, test_values, (size_t)(N*4));

        id<MTLCommandQueue> q = [dev newCommandQueue];
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
        [enc setComputePipelineState:pso];
        [enc setBuffer:out_buf offset:0 atIndex:0];
        [enc setBuffer:in_buf  offset:0 atIndex:1];
        [enc dispatchThreads:MTLSizeMake((NSUInteger)N,1,1)
         threadsPerThreadgroup:MTLSizeMake((NSUInteger)N,1,1)];
        [enc endEncoding];
        [cb commit];
        [cb waitUntilCompleted];

        uint32_t *out = out_buf.contents;
        printf("  %-12s %-6s %-6s %-6s %-6s %-6s  %-6s\n",
               "value", "pack", "fma+t", "rint", "dekGPU", "intGPU", "dekCPU");
        for (int i = 0; i < N; i++) {
            float f = u2f(test_values[i]);
            printf("  0x%08x  %-6u %-6u %-6u %-6u %-6u  %-6d\n",
                   test_values[i],
                   out[i*5+0], out[i*5+1], out[i*5+2],
                   out[i*5+3], out[i*5+4],
                   pack_unorm8_dekker(f, 255.0f));
        }
    }
    return 0;
}
