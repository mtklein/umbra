#include "dispatch_overlap.h"

_Bool dispatch_overlap_check(struct dispatch_overlap const *d,
                             void *buf, int l, int t, int r, int b) {
    if (d->writes >= DISPATCH_OVERLAP_CAP) {
        return 1;
    }
    for (int j = 0; j < d->writes; j++) {
        if (buf == d->write[j].buf
                && l < d->write[j].r && d->write[j].l < r
                && t < d->write[j].b && d->write[j].t < b) {
            return 1;
        }
    }
    return 0;
}

void dispatch_overlap_record(struct dispatch_overlap *d,
                             void *buf, int l, int t, int r, int b) {
    if (d->writes < DISPATCH_OVERLAP_CAP) {
        d->write[d->writes++] = (struct dispatch_write){
            .buf = buf, .l = l, .t = t, .r = r, .b = b,
        };
    }
}

void dispatch_overlap_reset(struct dispatch_overlap *d) {
    d->writes = 0;
}
