#include "umbra_draw.h"

typedef struct umbra_basic_block BB;

// --- Pipeline builder ---

struct umbra_basic_block* umbra_draw_build(umbra_shader_fn   shader,
                                           umbra_coverage_fn coverage,
                                           umbra_blend_fn    blend,
                                           umbra_load_fn     load,
                                           umbra_store_fn    store) {
    BB *bb = umbra_basic_block();
    umbra_v32 ix = umbra_lane(bb);

    // x = x0 + lane (x0 is uniform from p1)
    umbra_v32 x0 = umbra_load_32(bb, (umbra_ptr){1}, ix);
    umbra_v32 x  = umbra_add_i32(bb, x0, ix);
    umbra_v32 xf = umbra_f32_from_i32(bb, x);

    // y is uniform from p2
    umbra_v32 y  = umbra_load_32(bb, (umbra_ptr){2}, ix);
    umbra_v32 yf = umbra_f32_from_i32(bb, y);

    // Coverage first — enables future early-out if all zero.
    umbra_half cov = {0};
    if (coverage) {
        cov = coverage(bb, xf, yf);
        // TODO: early-out when coverage is all zero (ptest)
    }

    // Source color
    umbra_color src = {
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
    };
    if (shader) {
        src = shader(bb, xf, yf);
    }

    // Destination
    umbra_color dst = {
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
    };
    if (load) {
        dst = load(bb, (umbra_ptr){0}, ix);
    }

    // Blend
    umbra_color out;
    if (blend) {
        out = blend(bb, src, dst);
    } else {
        out = src;
    }

    // Apply coverage: lerp(dst, out, cov)  →  dst + (out - dst) * cov
    if (coverage) {
        out.r = umbra_add_half(bb, dst.r, umbra_mul_half(bb, umbra_sub_half(bb, out.r, dst.r), cov));
        out.g = umbra_add_half(bb, dst.g, umbra_mul_half(bb, umbra_sub_half(bb, out.g, dst.g), cov));
        out.b = umbra_add_half(bb, dst.b, umbra_mul_half(bb, umbra_sub_half(bb, out.b, dst.b), cov));
        out.a = umbra_add_half(bb, dst.a, umbra_mul_half(bb, umbra_sub_half(bb, out.a, dst.a), cov));
    }

    // Store
    if (store) {
        store(bb, (umbra_ptr){0}, ix, out);
    }

    return bb;
}

// --- Built-in shaders ---

umbra_color umbra_shader_solid(BB *bb, umbra_v32 x, umbra_v32 y) {
    (void)x; (void)y;
    // Color from uniforms: p3 = packed RGBA as fp16x4 (r,g,b,a at indices 0-3)
    umbra_half r = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 0));
    umbra_half g = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 1));
    umbra_half b = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 2));
    umbra_half a = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 3));
    return (umbra_color){r, g, b, a};
}

// --- Built-in blend modes ---

umbra_color umbra_blend_src(BB *bb, umbra_color src, umbra_color dst) {
    (void)bb; (void)dst;
    return src;
}

umbra_color umbra_blend_srcover(BB *bb, umbra_color src, umbra_color dst) {
    umbra_half one   = umbra_imm_half(bb, 0x3c00);
    umbra_half inv_a = umbra_sub_half(bb, one, src.a);
    return (umbra_color){
        umbra_add_half(bb, src.r, umbra_mul_half(bb, dst.r, inv_a)),
        umbra_add_half(bb, src.g, umbra_mul_half(bb, dst.g, inv_a)),
        umbra_add_half(bb, src.b, umbra_mul_half(bb, dst.b, inv_a)),
        umbra_add_half(bb, src.a, umbra_mul_half(bb, dst.a, inv_a)),
    };
}

// --- Built-in pixel formats ---

umbra_color umbra_load_8888(BB *bb, umbra_ptr ptr, umbra_v32 ix) {
    umbra_v16 ch[4];
    umbra_load_8x4(bb, ptr, ix, ch);
    umbra_half inv255 = umbra_imm_half(bb, 0x1C04);  // 1/255 in fp16
    return (umbra_color){
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[0]), inv255),
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[1]), inv255),
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[2]), inv255),
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[3]), inv255),
    };
}

void umbra_store_8888(BB *bb, umbra_ptr ptr, umbra_v32 ix, umbra_color c) {
    umbra_half scale = umbra_imm_half(bb, 0x5BF8);  // 255.0 in fp16
    umbra_half half_ = umbra_imm_half(bb, 0x3800);   // 0.5 in fp16
    umbra_v16 ch[4] = {
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.r, scale), half_)),
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.g, scale), half_)),
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.b, scale), half_)),
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.a, scale), half_)),
    };
    umbra_store_8x4(bb, ptr, ix, ch);
}

umbra_color umbra_load_fp16(BB *bb, umbra_ptr ptr, umbra_v32 ix) {
    // fp16x4 layout: r at ptr+0, g at ptr+1, b at ptr+2, a at ptr+3 (interleaved)
    // We need 4 separate half loads at strided offsets.
    // Use ptr as base, index*4+ch for each channel.
    umbra_v32 ix4 = umbra_shl_i32(bb, ix, umbra_imm_32(bb, 2));
    return (umbra_color){
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 0))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 1))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 2))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 3))),
    };
}

void umbra_store_fp16(BB *bb, umbra_ptr ptr, umbra_v32 ix, umbra_color c) {
    umbra_v32 ix4 = umbra_shl_i32(bb, ix, umbra_imm_32(bb, 2));
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 0)), c.r);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 1)), c.g);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 2)), c.b);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 3)), c.a);
}
