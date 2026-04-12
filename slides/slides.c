#include "slide.h"
#include "text.h"
#include <math.h>
#include <stdlib.h>

void slide_perspective_matrix(float out[11], float t, int sw, int sh, int bw, int bh) {
    float cx = (float)sw * 0.5f;
    float cy = (float)sh * 0.5f;
    float bx = (float)bw * 0.5f;
    float by = (float)bh * 0.5f;
    float angle = t * 0.3f;
    float tilt  = sinf(t * 0.7f) * 0.0008f;
    float sc    = 1.0f + 0.2f * sinf(t * 0.5f);
    float ca = cosf(angle), sa = sinf(angle);
    float w0 = 1.0f - tilt * cx;
    out[0] = ca * sc;     out[1] = sa * sc;
    out[2] = -cx*ca*sc - cy*sa*sc + bx*w0;
    out[3] = -sa * sc;    out[4] = ca * sc;
    out[5] = cx*sa*sc - cy*ca*sc + by*w0;
    out[6] = tilt;  out[7] = 0.0f;  out[8] = w0;
    out[9] = (float)bw; out[10] = (float)bh;
}

enum { MAX_SLIDES = 32 };

struct slide *make_overview(void);

static slide_factory_fn registry[MAX_SLIDES];
static int              registry_count;

static struct slide *all[MAX_SLIDES];
static int           count;

void slide_register(slide_factory_fn factory) {
    if (registry_count < MAX_SLIDES) {
        registry[registry_count++] = factory;
    }
}

int           slide_count(void) { return count; }
struct slide *slide_get(int i)  { return all[i]; }

static void add_slide(struct slide *s, int w, int h) {
    all[count] = s;
    if (s->init) { s->init(s, w, h); }
    count++;
}

void slides_init(int w, int h) {
    text_shared_init(w, h, (float)h * 0.15f);

    count = 0;
    for (int i = 0; i < registry_count; i++) {
        add_slide(registry[i](), w, h);
    }
    add_slide(make_overview(), w, h);
}

void slides_cleanup(void) {
    for (int i = 0; i < count; i++) {
        if (all[i]->free) { all[i]->free(all[i]); }
    }
    text_shared_cleanup();
    count = 0;
}
