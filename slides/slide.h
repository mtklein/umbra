#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

// TODO: I want to invert the control flow here somewhat,
//       less introspective of the slides and more feed-forward.
//       The driver program (bench, demo, dump, golden tests)
//       should be able to remain oblivious of slide internals.
//
//       Maybe the driver sets up a builder with prepared dst
//       binding and passes that, dst's fmt, w,h, and a transform
//       matrix to the slide, and if the slide has another draw
//       to make it fills out the builder.  The driver then does
//       its dumps, compiles, draws, timing, whatever.
//
//       Most slides would fill out only one builder and generate
//       one program, but slides like two-pass slug would make
//       more than one, and the driver would compile those into
//       a program each, and dispatch them in order.
//
//       Unsure quite yet how to square this with SDF dispatch.

struct slide {
    char const     *title;
    float           bg[4];

    void (*init)   (struct slide*, int w, int h);
    void (*prepare)(struct slide*, struct umbra_backend*, struct umbra_fmt);
    void (*draw)   (struct slide*, double secs, int l, int t, int r, int b, void *buf);
    void (*free)   (struct slide*);

    int (*get_builders)(struct slide*, struct umbra_fmt,
                        struct umbra_builder **out, int max);

    // Fill builder `b` with the slide's draw IR.  `dst_ptr` is already bound
    // on `b` (via umbra_bind_buf32) and is the final destination; `fmt` is
    // its format.  `(x, y)` are the post-transform dispatch coords -- if the
    // caller wanted a viewport transform applied, they've already issued it.
    // The slide may still issue its own transforms (e.g. an animated
    // perspective matrix) on top before calling umbra_build_draw.
    //
    // NULL means the slide has no composable draw path; consumers fall back
    // to the slide's own prepare/draw cycle or a placeholder.
    void (*build_draw)(struct slide*, struct umbra_builder *b,
                       umbra_ptr32 dst_ptr, struct umbra_fmt fmt,
                       umbra_val32 x, umbra_val32 y);

    // Update animation state (e.g. per-frame matrix uniforms) without
    // emitting any GPU/CPU work.  NULL means static.
    void  (*animate)(struct slide*, double secs);
};

typedef struct slide *(*slide_factory_fn)(void);
void slide_register(slide_factory_fn factory);

#define SLIDE(NAME)                                                              \
    static struct slide* NAME(void);                                            \
    _Pragma("clang diagnostic push")                                             \
    _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"")                \
    __attribute__((constructor)) static void slide_ctor_##NAME(void) {           \
        slide_register(NAME);                                                    \
    }                                                                            \
    _Pragma("clang diagnostic pop")                                              \
    static struct slide* NAME(void)

int           slide_count        (void);
struct slide* slide_get          (int i);
void          slides_init        (int w, int h);
void          slides_cleanup     (void);

void slide_perspective_matrix(struct umbra_matrix *out, float t, int sw, int sh, int bw, int bh);

void slide_bg_prepare(struct umbra_backend *be, struct umbra_fmt fmt, int w, int h);
void slide_bg_draw   (float const bg[4], int l, int t, int r, int b, void *buf);
void slide_bg_cleanup(void);
