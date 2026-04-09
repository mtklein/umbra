#pragma once
#include <stddef.h>

struct uniform_ring_chunk {
    void  *handle;
    void  *mapped;
    size_t cap, used;
};

struct uniform_ring_loc {
    void  *handle;
    size_t offset;
};

struct uniform_ring {
    struct uniform_ring_chunk *chunks;
    int    n, cap;
    int    cur, :32;
    size_t align;
    void  *ctx;
    struct uniform_ring_chunk (*new_chunk)(size_t min_bytes, void *ctx);
    void                      (*free_chunk)(void *handle, void *ctx);
};

struct uniform_ring_loc uniform_ring_alloc(struct uniform_ring *, void const *bytes, size_t len);
size_t                  uniform_ring_used (struct uniform_ring const *);
void                    uniform_ring_reset(struct uniform_ring *);

// Frees every chunk via free_chunk(handle, ctx). Backends typically pass the
// device pointer as ctx, so callers MUST invoke uniform_ring_free BEFORE
// destroying that device — otherwise free_chunk will dereference a dead
// device handle.
void                    uniform_ring_free (struct uniform_ring *);
