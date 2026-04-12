#include "dispatch_overlap.h"

_Bool dispatch_overlap_check(struct dispatch_overlap const *d,
                             void *buf, int l, int t, int r, int b) {
    if (d->n >= DISPATCH_OVERLAP_CAP) {
        return 1;
    }
    for (int j = 0; j < d->n; j++) {
        if (buf == d->writes[j].buf
                && l < d->writes[j].r && d->writes[j].l < r
                && t < d->writes[j].b && d->writes[j].t < b) {
            return 1;
        }
    }
    return 0;
}

void dispatch_overlap_record(struct dispatch_overlap *d,
                             void *buf, int l, int t, int r, int b) {
    if (d->n < DISPATCH_OVERLAP_CAP) {
        d->writes[d->n++] = (struct dispatch_write){
            .buf = buf, .l = l, .t = t, .r = r, .b = b,
        };
    }
}

void dispatch_overlap_reset(struct dispatch_overlap *d) {
    d->n = 0;
}
