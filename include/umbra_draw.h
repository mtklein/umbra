#pragma once
#include "umbra.h"
#include "umbra_interval.h"

typedef struct { float       x, y; } umbra_point;
typedef struct { umbra_val32 x, y; } umbra_point_val32;

typedef struct {
    float l, t, r, b;
} umbra_rect;

typedef struct { float       r, g, b, a; } umbra_color;
typedef struct { umbra_val32 r, g, b, a; } umbra_color_val32;

struct umbra_matrix {
    float sx, kx, tx,
          ky, sy, ty,
          p0, p1, p2;
};

typedef struct {
    umbra_val32 sx, kx, tx,
                ky, sy, ty,
                p0, p1, p2;
} umbra_matrix_val32;

// Apply a 3x3 perspective matrix to (x, y), emitting the usual
// w = p0*x + p1*y + p2; x' = (sx*x + kx*y + tx) / w; y' = (ky*x + sy*y + ty) / w.
// Ad-hoc shared helper used by perspective-transformed shaders/coverage today;
// will become the body of the affine/perspective umbra_transform impls once
// that framework lands.
umbra_point_val32 umbra_apply_matrix(struct umbra_builder*, umbra_matrix_val32,
                                      umbra_val32 x, umbra_val32 y);

struct umbra_bitmap {
    struct umbra_buf buf;
    int              w,h;
};

struct umbra_fmt {
    char const *name;
    size_t      bpp;
    int         planes, :32;
    umbra_color_val32 (*load) (struct umbra_builder*, void const *umbra_ptr);
    void              (*store)(struct umbra_builder*, void const *umbra_ptr, umbra_color_val32);
};
extern struct umbra_fmt const umbra_fmt_8888,
                              umbra_fmt_565,
                              umbra_fmt_1010102,
                              umbra_fmt_fp16,
                              umbra_fmt_fp16_planar;

umbra_color_val32 umbra_load_8888        (struct umbra_builder*, umbra_ptr32);
void              umbra_store_8888       (struct umbra_builder*, umbra_ptr32, umbra_color_val32);
umbra_color_val32 umbra_load_565         (struct umbra_builder*, umbra_ptr16);
void              umbra_store_565        (struct umbra_builder*, umbra_ptr16, umbra_color_val32);
umbra_color_val32 umbra_load_1010102     (struct umbra_builder*, umbra_ptr32);
void              umbra_store_1010102    (struct umbra_builder*, umbra_ptr32, umbra_color_val32);
umbra_color_val32 umbra_load_fp16        (struct umbra_builder*, umbra_ptr64);
void              umbra_store_fp16       (struct umbra_builder*, umbra_ptr64, umbra_color_val32);
umbra_color_val32 umbra_load_fp16_planar (struct umbra_builder*, umbra_ptr16);
void              umbra_store_fp16_planar(struct umbra_builder*, umbra_ptr16, umbra_color_val32);

// Effects emit IR into a builder.  Any uniforms registered with
// umbra_bind_uniforms32() must outlive the builder, flat_ir, and program.  These are
// function types (not pointer types), so declarations read as
// `umbra_blend umbra_blend_src, umbra_blend_srcover, ...;` and struct fields
// carrying them write as `umbra_shader *inner_fn`.
typedef umbra_color_val32 umbra_shader  (void *ctx, struct umbra_builder*,
                                         umbra_val32 x, umbra_val32 y);
typedef umbra_val32       umbra_coverage(void *ctx, struct umbra_builder*,
                                         umbra_val32 x, umbra_val32 y);
typedef umbra_interval    umbra_sdf     (void *ctx, struct umbra_builder*,
                                         umbra_interval x, umbra_interval y);
typedef umbra_color_val32 umbra_blend   (void *ctx, struct umbra_builder*,
                                         umbra_color_val32 src,
                                         umbra_color_val32 dst);

// Shade a single color; ctx is an umbra_color*.
umbra_color_val32 umbra_shader_color(void *umbra_color, struct umbra_builder*,
                                     umbra_val32 x, umbra_val32 y);

umbra_blend umbra_blend_src,
            umbra_blend_srcover,
            umbra_blend_dstover,
            umbra_blend_multiply;

// Compose coverage, shader, and blend into an IR builder that reads and writes
// dst_fmt at the bound dst buf.  Any of the effects may be NULL for default
// behavior: coverage=1, shader={0,0,0,0}, blend=src.  dst must outlive the
// builder, flat_ir, and program; its contents are dereferenced at queue time.
struct umbra_builder* umbra_draw_builder(umbra_coverage, void *coverage_ctx,
                                         umbra_shader  , void *shader_ctx,
                                         umbra_blend   , void *blend_ctx,
                                         struct umbra_buf *dst,
                                         struct umbra_fmt  dst_fmt);

// Draw using an umbra_sdf as coverage.  hard_edge=1 gives a binary mask;
// hard_edge=0 clamps -sdf into [0, 1] for a 1px AA ramp.
// TODO: _Bool hard_edge -> int quality
struct umbra_sdf_draw* umbra_sdf_draw(struct umbra_backend*,
                                      umbra_sdf, void *sdf_ctx,
                                      _Bool hard_edge,
                                      umbra_shader, void *shader_ctx,
                                      umbra_blend,  void *blend_ctx,
                                      struct umbra_fmt);
void umbra_sdf_draw_queue(struct umbra_sdf_draw*, int l, int t, int r, int b,
                          struct umbra_buf dst);
void umbra_sdf_draw_free(struct umbra_sdf_draw*);


// Adapt an umbra_sdf as umbra_coverage.
struct umbra_coverage_from_sdf {
    umbra_sdf *sdf_fn;
    void      *sdf_ctx;
    int        hard_edge, :32;
};
umbra_val32 umbra_coverage_from_sdf(void *ctx, struct umbra_builder*,
                                     umbra_val32 x, umbra_val32 y);

// Supersample a wrapped shader.
struct umbra_supersample {
    umbra_shader *inner_fn;
    void         *inner_ctx;
    int           samples, :32;
};
umbra_color_val32 umbra_shader_supersample(void *umbra_supersample, struct umbra_builder*,
                                            umbra_val32 x, umbra_val32 y);

// Cover a rectangle, ctx is an umbra_rect*.
umbra_val32 umbra_coverage_rect(void *umbra_rect, struct umbra_builder*,
                                 umbra_val32 x, umbra_val32 y);

// A rectangle SDF, ctx is an umbra_rect*.
umbra_interval umbra_sdf_rect(void *umbra_rect, struct umbra_builder*,
                              umbra_interval x, umbra_interval y);

