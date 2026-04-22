#include "uniform_ring.h"
#include <stdlib.h>

static size_t align_up(size_t x, size_t a) { return (x + a - 1) & ~(a - 1); }

struct uniform_ring_loc uniform_ring_alloc(struct uniform_ring *r, void const *bytes, size_t size) {
    size_t const reserved = align_up(size, r->align);
    for (;;) {
        if (r->cur < r->n) {
            struct uniform_ring_chunk *c = &r->chunks[r->cur];
            if (c->used + reserved <= c->cap) {
                size_t const off = c->used;
                if (bytes && size) { __builtin_memcpy((char*)c->mapped + off, bytes, size); }
                c->used = off + reserved;
                return (struct uniform_ring_loc){.handle=c->handle, .offset=off};
            }
            r->cur++;
        } else {
            if (r->n >= r->cap) {
                r->cap = r->cap ? 2 * r->cap : 4;
                r->chunks = realloc(r->chunks, (size_t)r->cap * sizeof *r->chunks);
            }
            r->chunks[r->n++] = r->new_chunk(reserved, r->ctx);
        }
    }
}

size_t uniform_ring_used(struct uniform_ring const *r) {
    size_t total_bytes = 0;
    for (int i = 0; i < r->n; i++) { total_bytes += r->chunks[i].used; }
    return total_bytes;
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

void uniform_ring_pool_free(struct uniform_ring_pool *p) {
    for (int i = 0; i < p->n; i++) { uniform_ring_free(&p->rings[i]); }
}

struct uniform_ring_loc uniform_ring_pool_alloc(struct uniform_ring_pool *p,
                                                void const *bytes, size_t size) {
    return uniform_ring_alloc(&p->rings[p->cur], bytes, size);
}

_Bool uniform_ring_pool_should_rotate(struct uniform_ring_pool const *p) {
    return uniform_ring_used(&p->rings[p->cur]) > p->high_water;
}

void uniform_ring_pool_rotate(struct uniform_ring_pool *p) {
    p->cur = (p->cur + 1) % p->n;
    p->rotations++;
    p->wait_frame(p->cur, p->ctx);
    uniform_ring_reset(&p->rings[p->cur]);
}

void uniform_ring_pool_drain_all(struct uniform_ring_pool *p) {
    for (int i = 0; i < p->n; i++) {
        p->wait_frame(i, p->ctx);
        uniform_ring_reset(&p->rings[i]);
    }
    p->cur = 0;
}
