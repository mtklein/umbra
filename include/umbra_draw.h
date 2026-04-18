#pragma once
#include "umbra.h"
#include "umbra_interval.h"

typedef struct {
    float x, y;
} umbra_point;

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

struct umbra_bitmap {
    struct umbra_buf buf;
    int              w,h;
};

struct umbra_fmt {
    char const *name;
    size_t      bpp;
    int         planes, :32;
    umbra_color_val32 (*load) (struct umbra_builder*, int ptr_ix);
    void              (*store)(struct umbra_builder*, int ptr_ix, umbra_color_val32);
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

struct umbra_shader {
    umbra_color_val32 (*build)(struct umbra_shader*,
                               struct umbra_builder*,
                               umbra_ptr32 uniforms,
                               umbra_val32 x, umbra_val32 y);
    void             (*free)(struct umbra_shader*);
    struct umbra_buf   uniforms;
};
void umbra_shader_free(struct umbra_shader*);

struct umbra_coverage {
    umbra_val32 (*build)(struct umbra_coverage*,
                         struct umbra_builder*,
                         umbra_ptr32 uniforms,
                         umbra_val32 x, umbra_val32 y);
    void             (*free)(struct umbra_coverage*);
    struct umbra_buf   uniforms;
};
void umbra_coverage_free(struct umbra_coverage*);

// Signed distance function returning f(x,y) where f < 0 means inside.
struct umbra_sdf {
    umbra_interval (*build)(struct umbra_sdf*,
                            struct umbra_builder*,
                            umbra_ptr32 uniforms,
                            umbra_interval x, umbra_interval y);
    void             (*free)(struct umbra_sdf*);
    struct umbra_buf   uniforms;
};
void umbra_sdf_free(struct umbra_sdf*);

// Compute the umbra_buf for a concrete effect whose base vtable is at offset 0.
// The data starts right after the base and runs to the end of the struct.
// `self` is a pointer to the concrete effect.  Use at construction time to set
// `.base.uniforms`.  The effect must stay put after construction — no move, no
// by-value copy — since `.ptr` is an absolute address.  Forwarding wrappers
// (umbra_shader_supersample, umbra_sdf_coverage) set `.uniforms` to the wrapped
// effect's resolved uniforms buf instead.
#define UMBRA_UNIFORMS_OF(self)                                                 \
    ((struct umbra_buf){                                                        \
        .ptr   = (char*)(self) + sizeof((self)->base),                          \
        .count = (int)((sizeof *(self) - sizeof((self)->base)) / 4),            \
    })

// TODO: bool hard_edge -> int quality

struct umbra_coverage* umbra_sdf_coverage(struct umbra_sdf*, _Bool hard_edge);

// Blend fns take void *ctx for the flat-effect convention.  Built-in blends
// are stateless and ignore it; old callers of umbra_draw_builder still pass
// them by name -- the old middleware just threads NULL as ctx through to
// the underlying blend call.
typedef umbra_color_val32 (*umbra_blend_fn)(void *ctx, struct umbra_builder*,
                                            umbra_color_val32 src, umbra_color_val32 dst);
umbra_color_val32 umbra_blend_src     (void *ctx, struct umbra_builder*,
                                       umbra_color_val32 src, umbra_color_val32 dst);
umbra_color_val32 umbra_blend_srcover (void *ctx, struct umbra_builder*,
                                       umbra_color_val32 src, umbra_color_val32 dst);
umbra_color_val32 umbra_blend_dstover (void *ctx, struct umbra_builder*,
                                       umbra_color_val32 src, umbra_color_val32 dst);
umbra_color_val32 umbra_blend_multiply(void *ctx, struct umbra_builder*,
                                       umbra_color_val32 src, umbra_color_val32 dst);

struct umbra_builder* umbra_draw_builder(struct umbra_coverage*,
                                         struct umbra_shader*,
                                         umbra_blend_fn,
                                         struct umbra_fmt);

// New middleware: flat effect fn pointers that take a caller-owned context
// pointer as first arg, register their uniforms via umbra_uniforms(), and
// compose via umbra_draw_builder2().  The old struct-based path above stays
// in place; effects migrate over one at a time.  These typedefs live in the
// ordinary-identifier namespace, coexistent with `struct umbra_shader` etc.
// in the tag namespace.
typedef umbra_color_val32 (*umbra_shader)  (void *ctx, struct umbra_builder*,
                                            umbra_val32 x, umbra_val32 y);
typedef umbra_val32       (*umbra_coverage)(void *ctx, struct umbra_builder*,
                                            umbra_val32 x, umbra_val32 y);
typedef umbra_color_val32 (*umbra_blend)   (void *ctx, struct umbra_builder*,
                                            umbra_color_val32 src,
                                            umbra_color_val32 dst);

struct umbra_builder* umbra_draw_builder2(
    umbra_coverage coverage_fn, void *coverage_ctx,
    umbra_shader   shader_fn,   void *shader_ctx,
    umbra_blend    blend_fn,    void *blend_ctx,
    struct umbra_fmt);

struct umbra_sdf_draw_config {
    _Bool hard_edge;
};

struct umbra_sdf_draw* umbra_sdf_draw(struct umbra_backend*,
                                      struct umbra_sdf*,
                                      struct umbra_sdf_draw_config,
                                      struct umbra_shader*,
                                      umbra_blend_fn,
                                      struct umbra_fmt);
void umbra_sdf_draw_queue(struct umbra_sdf_draw const*,
                              int l, int t, int r, int b, struct umbra_buf[]);
void umbra_sdf_draw_free(struct umbra_sdf_draw*);

// Flat shader: caller owns an umbra_color; pass &color as the shader_ctx to
// umbra_draw_builder2 (or wrap via umbra_shader_wrap for old-middleware
// composers).  Mutating *color between queue() calls is reflected on next
// dispatch.
umbra_color_val32 umbra_shader_solid(void *ctx, struct umbra_builder*,
                                     umbra_val32 x, umbra_val32 y);

// Wrap any flat shader (fn, ctx) into a struct umbra_shader* for composers
// that haven't migrated yet.  Caller retains ownership of ctx storage until
// the returned wrapper is freed via umbra_shader_free().
struct umbra_shader* umbra_shader_wrap(umbra_shader fn, void *ctx);

// A gradient is (x,y) -> t -> color.  umbra_gradient_coords is the first leg,
// a first-class effect in the same shape as umbra_shader / umbra_coverage /
// umbra_sdf: a vtable + its own uniform buffer.  Colorizer shaders hold a
// coords pointer and reference coords->uniforms through a buf-handle slot,
// so the coords's uniform layout stays completely private to the coords
// implementation -- the shader never touches it directly.
//
// The `t` callback emits IR for t (clamped to [0, 1]) from xy, reading
// whatever state it needs out of `uniforms` at slots relative to its own
// subclass layout.
struct umbra_gradient_coords {
    umbra_val32 (*t)(struct umbra_gradient_coords*,
                     struct umbra_builder*,
                     umbra_ptr32 uniforms,
                     umbra_point_val32 xy);
    void             (*free)(struct umbra_gradient_coords*);
    struct umbra_buf   uniforms;
};
void umbra_gradient_coords_free(struct umbra_gradient_coords*);

struct umbra_gradient_coords* umbra_gradient_linear(umbra_point p0, umbra_point p1);
struct umbra_gradient_coords* umbra_gradient_radial(umbra_point center, float radius);

// Colorizer constructors take ownership of the coords pointer; freeing the
// returned shader also frees the coords.  Don't share one coords between
// shaders -- each shader that needs one should get its own.
struct umbra_shader* umbra_shader_gradient_two_stops(
    struct umbra_gradient_coords*, umbra_color c0, umbra_color c1);

struct umbra_shader* umbra_shader_gradient_evenly_spaced_stops(
    struct umbra_gradient_coords*, struct umbra_buf colors);

struct umbra_shader* umbra_shader_gradient_lut(
    struct umbra_gradient_coords*, struct umbra_buf lut);

struct umbra_shader* umbra_shader_gradient(struct umbra_gradient_coords*,
                                           struct umbra_buf colors,
                                           struct umbra_buf pos);

struct umbra_shader* umbra_shader_supersample(struct umbra_shader *inner, int samples);

// Flat coverage: caller owns an umbra_rect; pass &rect as coverage_ctx to
// umbra_draw_builder2 (or wrap via umbra_coverage_wrap for old-middleware
// composers).  Mutation between queue() calls is reflected on next dispatch.
umbra_val32 umbra_coverage_rect(void *ctx, struct umbra_builder*,
                                 umbra_val32 x, umbra_val32 y);

// Wrap any flat coverage (fn, ctx) into a struct umbra_coverage* for composers
// that haven't migrated yet.  Caller retains ownership of ctx storage until
// the returned wrapper is freed via umbra_coverage_free().
struct umbra_coverage* umbra_coverage_wrap(umbra_coverage fn, void *ctx);

struct umbra_sdf* umbra_sdf_rect(umbra_rect);

struct umbra_coverage* umbra_coverage_bitmap(struct umbra_buf bmp);

struct umbra_coverage* umbra_coverage_sdf(struct umbra_buf bmp);

struct umbra_coverage* umbra_coverage_bitmap_matrix(struct umbra_matrix, struct umbra_bitmap);
struct umbra_coverage* umbra_coverage_winding(struct umbra_buf winding);

void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, umbra_color const *colors);
void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        umbra_color const *colors);
