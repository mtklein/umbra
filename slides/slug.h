#pragma once
#include "../include/umbra_draw.h"

struct slug_curves {
    float *data;
    int    count;
    int    pad;
    float  w, h;
};

struct slug_curves slug_extract(char const *text, float font_size);
void               slug_free   (struct slug_curves *sc);

// Flattened-polyline view of the same text.  Each Bezier curve from the font
// is chopped into `samples_per_curve` chord segments; straight lines are
// emitted as a single segment.  Segments are stored as (p0x, p0y, p1x, p1y).
struct slug_polyline {
    float *data;
    int    count;
    int    pad;
    float  w, h;
};

struct slug_polyline slug_polyline_extract(char const *text, float font_size,
                                           int samples_per_curve);
void                 slug_polyline_free   (struct slug_polyline *sp);
