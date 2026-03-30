// sRGB transfer function survey: compare decode/encode at various cost levels.
//
// Build: clang -O2 -lm tools/srgb_survey.c -o srgb_survey

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t ftou(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

// ── Reference ──────────────────────────────────────────────────────

static double spec_decode(int i) {
    double s = i / 255.0;
    return s <= 0.04045 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4);
}
static double spec_encode(double l) {
    return l <= 0.0031308 ? 12.92 * l : 1.055 * pow(l, 1.0/2.4) - 0.055;
}

// ── Decode: sRGB byte value s ∈ [0,1] → linear float ──────────────
// All: lo = s/12.92 for s < threshold, hi = polynomial for s ≥ threshold.

// D1: cubic (5 flops)
// From the original Skia-style approximation.
static float d_cubic(float s) {
    float lo = s * (1.0f/12.92f);
    float hi = s*s * (s * 0.3f + 0.6975f) + 0.0025f;
    return s < 0.055f ? lo : hi;
}

// D2: quintic (10 flops)
// From commit 8deece3. Structure: s2*(a*s3 + b*s2 + c*s + d) + e.
static float d_quintic(float s) {
    float lo = s * (1.0f/12.92f);
    float s2 = s * s, s3 = s2 * s;
    float hi = s2 * (-0.03423264f * s3
                   +  0.02881829f * s2
                   +  0.31312484f * s
                   +  0.68812025f)
             + 0.00333771f;
    return s < 0.09870274f ? lo : hi;
}

// D3: sextic (12 flops)
// From commit 2149d2a. Constraint: f = 1-(a+b+c+d+e).
static float d_sextic(float s) {
    float lo = s * (1.0f/12.92f);
    float a=0.2456940264f, b=-0.7771521211f, c=0.8481360674f,
          d=-0.07268945128f, e=0.7527475953f;
    float f = 1.0f - (a+b+c+d+e);
    float s2=s*s, s4=s2*s2;
    float hi = a*s4*s2 + b*s4*s + c*s4 + d*s2*s + e*s2 + f;
    return s < 0.05796349049f ? lo : hi;
}

// D4: septic (14 flops) — current production
// From commit 5873f8d. Constraint: g = 1-(a+b+c+d+e+f).
static float d_septic(float s) {
    float a=-4.82083022594e-01f, b=1.84310853481e+00f,
          c=-2.79252314568e+00f, d=2.05758404732e+00f,
          e=-4.18130934238e-01f, f=7.89776027203e-01f;
    float g = 1.0f - (a+b+c+d+e+f);
    float lo = s * (1.0f/12.92f);
    float inner = ((a*s+b)*s+c)*s+d;
    float mid = (inner*s+e)*s+f;
    float s2 = s*s;
    float hi = mid*s2 + g;
    return s < 5.76281473041e-02f ? lo : hi;
}

// D5: log2/exp2 with degree-4 polynomial refinements (~20 flops)
// log2(x): extract exponent + minimax poly on mantissa
// exp2(x): reconstruct from floor + minimax poly on fraction
static float approx_log2(float x) {
    uint32_t xi = ftou(x);
    float e = (float)((int)(xi >> 23) - 127);
    float m = u2f((xi & 0x7FFFFF) | 0x3F800000) - 1.0f;
    // Minimax log2(1+m)/m on [0,1), degree 3
    float p = 1.44269502f + m*(-0.72054651f + m*(0.45326680f + m*-0.18235789f));
    return e + m * p;
}
static float approx_exp2(float x) {
    float fi = floorf(x);
    float f = x - fi;
    int ei = (int)fi + 127;
    if (ei < 1) ei = 1; if (ei > 254) ei = 254;
    // Minimax 2^f on [0,1), degree 4
    float p = 1.0f + f*(0.69314718f + f*(0.24022650f
            + f*(0.05550410f + f*0.00961812f)));
    return u2f((uint32_t)ei << 23) * p;
}
static float d_logexp(float s) {
    float lo = s * (1.0f/12.92f);
    float base = (s + 0.055f) * (1.0f/1.055f);
    if (base <= 0) return lo;
    float hi = approx_exp2(2.4f * approx_log2(base));
    return s < 0.04045f ? lo : hi;
}

// ── Encode: linear float l → sRGB float ───────────────────────────
// All: lo = 12.92*l for l < threshold,
//      hi = (C + t*(k1 + t*(k2+...))) / (D + t) where t = rsqrt(l).

// E1: rat2 — rsqrt + 2 k-terms (~8 flops + rsqrt)
// From commit 972580d (tuned for exact 8-bit roundtrip with cubic decode).
static float e_rat2(float l) {
    l = fmaxf(l, 0);
    float lo = l * 12.92f;
    float t = 1.0f / sqrtf(fmaxf(l, 1e-30f));
    float hi = (1.12732994556f + t*(0.01347202249f + t*-0.00233423407f))
             / (0.13738775253f + t);
    return l < 0.00465985f ? lo : hi;
}

// E2: rat3 — rsqrt + 3 k-terms (~10 flops + rsqrt)
// From commit 8deece3.
static float e_rat3(float l) {
    l = fmaxf(l, 0);
    float lo = l * 12.92f;
    float t = 1.0f / sqrtf(fmaxf(l, 1e-30f));
    float hi = (1.09732234f + t*(0.02995744f + t*(-0.00546762f + t*0.00012954f)))
             / (0.12201570f + t);
    return l < 0.00898038f ? lo : hi;
}

// E3: rat4 — rsqrt + 4 k-terms (~12 flops + rsqrt) — current production
// From commit 5873f8d.
static float e_rat4(float l) {
    l = fmaxf(l, 0);
    float lo = l * 12.92f;
    float t = 1.0f / sqrtf(fmaxf(l, 1e-30f));
    float hi = (1.0545324087e+00f + t*(5.8207426220e-02f
              + t*(-1.2198361568e-02f + t*(7.9244317021e-04f
              + t*-2.0467568902e-05f))))
             / (1.0131348670e-01f + t);
    return l < 4.5700869523e-03f ? lo : hi;
}

// E4: log2/exp2 encode (~20 flops)
static float e_logexp(float l) {
    l = fmaxf(l, 0);
    float lo = l * 12.92f;
    float hi = 1.055f * approx_exp2((5.0f/12.0f) * approx_log2(fmaxf(l, 1e-30f))) - 0.055f;
    return l < 0.0031308f ? lo : hi;
}

// ── Evaluation ─────────────────────────────────────────────────────

typedef float (*dec_fn)(float);
typedef float (*enc_fn)(float);

static void eval(char const *name, dec_fn dec, enc_fn enc) {
    double dec_max = 0, enc_max = 0;
    int dec_exact = 0, enc_byte_err = 0;
    int self_rt = 0, cross_de = 0, cross_ed = 0;
    int f0 = (ftou(dec(0)) == 0);
    int f1 = (ftou(dec(1)) == ftou(1.0f));

    for (int i = 0; i < 256; i++) {
        float s = (float)i / 255.0f;
        float spec_f = (float)spec_decode(i);

        // decode vs spec
        float dl = dec(s);
        double de = fabs((double)dl - (double)spec_f);
        if (de > dec_max) dec_max = de;
        if (ftou(dl) == ftou(spec_f)) dec_exact++;

        // encode fed spec linear → byte
        float es = enc(spec_f);
        int eb = (int)roundf(fminf(fmaxf(es, 0), 1) * 255.0f);
        if (eb != i) enc_byte_err++;
        double ee = fabs((double)es - i/255.0);
        if (ee > enc_max) enc_max = ee;

        // self roundtrip
        float rt = enc(dl);
        if ((int)roundf(fminf(fmaxf(rt, 0), 1) * 255.0f) != i) self_rt++;

        // cross roundtrip
        float cs = enc(spec_f);
        if ((int)roundf(fminf(fmaxf(cs, 0), 1) * 255.0f) != i) cross_de++;
        if ((int)round(fmin(fmax(spec_encode((double)dl), 0), 1) * 255.0) != i) cross_ed++;
    }

    printf("  %-20s |", name);
    printf(" dec: max=%.1e exact=%3d %s%s |", dec_max, dec_exact,
           f0?"":"f(0)! ", f1?"":"f(1)! ");
    printf(" enc: max=%.1e err=%d |", enc_max, enc_byte_err);
    printf(" rt=%d cross=%d/%d\n", self_rt, cross_de, cross_ed);
}

int main(void) {
    printf("  %-20s | %-33s | %-18s | %s\n",
           "name", "decode (vs spec)", "encode (byte err)", "roundtrips");
    printf("  %-20s | %-33s | %-18s | %s\n",
           "----", "---", "---", "---");

    eval("D1+E1 cubic/rat2",   d_cubic,   e_rat2);
    eval("D1+E2 cubic/rat3",   d_cubic,   e_rat3);
    eval("D1+E3 cubic/rat4",   d_cubic,   e_rat4);
    eval("D2+E2 quintic/rat3", d_quintic, e_rat3);
    eval("D2+E3 quintic/rat4", d_quintic, e_rat4);
    eval("D3+E3 sextic/rat4",  d_sextic,  e_rat4);
    eval("D4+E3 sept/rat4 ★",  d_septic,  e_rat4);
    eval("D4+E1 sept/rat2",    d_septic,  e_rat2);
    eval("D4+E2 sept/rat3",    d_septic,  e_rat3);
    eval("D5+E4 log/log",      d_logexp,  e_logexp);
    eval("D5+E3 log/rat4",     d_logexp,  e_rat4);
    eval("D4+E4 sept/log",     d_septic,  e_logexp);

    return 0;
}
