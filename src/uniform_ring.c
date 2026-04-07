#include "uniform_ring.h"
#include <stdlib.h>

static size_t align_up(size_t x, size_t a) { return (x + a - 1) & ~(a - 1); }

struct uniform_ring_loc uniform_ring_alloc(struct uniform_ring *r, void const *bytes, size_t len) {
    size_t reserved = align_up(len, r->align);
    for (;;) {
        if (r->cur < r->n) {
            struct uniform_ring_chunk *c = &r->chunks[r->cur];
            if (c->used + reserved <= c->cap) {
                size_t off = c->used;
                if (bytes && len) { __builtin_memcpy((char*)c->mapped + off, bytes, len); }
                c->used = off + reserved;
                return (struct uniform_ring_loc){.handle=c->handle, .offset=off};
            }
            r->cur++;
            continue;
        }
        if (r->n >= r->cap) {
            r->cap = r->cap ? 2 * r->cap : 4;
            r->chunks = realloc(r->chunks, (size_t)r->cap * sizeof *r->chunks);
        }
        r->chunks[r->n++] = r->new_chunk(reserved, r->ctx);
    }
}

size_t uniform_ring_used(struct uniform_ring const *r) {
    size_t total = 0;
    for (int i = 0; i < r->n; i++) { total += r->chunks[i].used; }
    return total;
}

void uniform_ring_reset(struct uniform_ring *r) {
    for (int i = 0; i < r->n; i++) { r->chunks[i].used = 0; }
    r->cur = 0;
}

void uniform_ring_free(struct uniform_ring *r) {
    for (int i = 0; i < r->n; i++) { r->free_chunk(r->chunks[i].handle, r->ctx); }
    free(r->chunks);
    r->chunks = 0;
    r->n = r->cap = r->cur = 0;
}
