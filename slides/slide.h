#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

struct slide {
    char const     *title;
    float           bg[4];

    void (*init)   (struct slide*, int w, int h);
    void (*prepare)(struct slide*, struct umbra_backend*, struct umbra_fmt);
    void (*draw)   (struct slide*, int frame, int l, int t, int r, int b, void *buf);
    void (*free)   (struct slide*);

    struct umbra_builder *(*get_builder)(struct slide*, struct umbra_fmt);
    int (*get_builders)(struct slide*, struct umbra_fmt,
                        struct umbra_builder **out, char const **names, int max);
};

typedef struct slide *(*slide_factory_fn)(void);
void slide_register(slide_factory_fn factory);

#define SLIDE(NAME)                                                              \
    static struct slide *NAME(void);                                            \
    _Pragma("clang diagnostic push")                                             \
    _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"")                \
    __attribute__((constructor)) static void slide_ctor_##NAME(void) {           \
        slide_register(NAME);                                                    \
    }                                                                            \
    _Pragma("clang diagnostic pop")                                              \
    static struct slide *NAME(void)

int           slide_count        (void);
struct slide *slide_get          (int i);
void          slides_init        (int w, int h);
void          slides_cleanup     (void);

void slide_perspective_matrix(struct umbra_matrix *out, float t, int sw, int sh, int bw, int bh);

void slide_bg_prepare(struct umbra_backend *be, struct umbra_fmt fmt, int w, int h);
void slide_bg_draw   (float const bg[4], int l, int t, int r, int b, void *buf);
void slide_bg_cleanup(void);
