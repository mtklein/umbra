#pragma once

struct dispatch_write {
    void *buf;
    int l, t, r, b;
};

enum { DISPATCH_OVERLAP_CAP = 64 };

struct dispatch_overlap {
    struct dispatch_write writes[DISPATCH_OVERLAP_CAP];
    int n, :32;
};

// Check whether `buf` at rect (l,t,r,b) overlaps any previously recorded write.
_Bool dispatch_overlap_check(struct dispatch_overlap const *,
                             void *buf, int l, int t, int r, int b);

// Record a write to `buf` at rect (l,t,r,b).
void dispatch_overlap_record(struct dispatch_overlap *,
                             void *buf, int l, int t, int r, int b);

// Reset tracking (after a barrier or new batch).
void dispatch_overlap_reset(struct dispatch_overlap *);
