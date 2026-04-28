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

struct uniform_ring_loc uniform_ring_alloc(struct uniform_ring *, void const *bytes, size_t size);
size_t                  uniform_ring_used (struct uniform_ring const *);
void                    uniform_ring_reset(struct uniform_ring *);
void                    uniform_ring_free (struct uniform_ring *);


enum { UNIFORM_RING_POOL_MAX_FRAMES = 4 };

struct uniform_ring_pool {
    struct uniform_ring rings[UNIFORM_RING_POOL_MAX_FRAMES];
    int    n;
    int    cur;
    int    rotations, :32;
    size_t high_water;
    void  *ctx;
    void  (*wait_frame)(int frame, void *ctx);
};
struct uniform_ring_loc uniform_ring_pool_alloc(struct uniform_ring_pool*,
                                                void const *bytes, size_t size);

_Bool uniform_ring_pool_should_rotate(struct uniform_ring_pool const *);
void uniform_ring_pool_rotate(struct uniform_ring_pool*);

void uniform_ring_pool_drain_all(struct uniform_ring_pool*);  // Wait on in-flight work and reset.
void uniform_ring_pool_purge    (struct uniform_ring_pool*);  // uniform_ring_free() all rings.
