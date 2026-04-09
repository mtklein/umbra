#pragma once
#include <stddef.h>

// TODO: the ring assumes single-threaded encode (uniform_ring_pool's cur and
// chunk allocation are unguarded). If a client ever wanted to encode
// dispatches from multiple threads, callers would need locks around the
// pool and its rings.

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

// uniform_ring_pool: a small fixed-size set of rings with a rotating frame
// index, a high-water-mark backpressure policy, and a rotation counter.
//
// The pool centralizes the parts of the GPU encode lifecycle that are common
// across backends: which ring the next allocation lands in (`cur`), when the
// caller should rotate to relieve memory pressure (`should_rotate`), and how
// many rotations have happened (`rotations`, useful for tests).
//
// What it does NOT know about: the backend-specific GPU primitives — command
// buffers, fences, encoders. Those stay in the backend. When the pool needs
// to wait on the in-flight work for a frame whose ring is about to be reset,
// it calls back to the backend via `wait_frame`.
//
// Initialize via designated initializer; per-ring fields (align, ctx,
// new_chunk, free_chunk) live on each `rings[i]` exactly as for a standalone
// uniform_ring.
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

// Free every ring in the pool. Same lifetime caveat as uniform_ring_free:
// invoke before destroying any device the per-ring ctx still references.
void                    uniform_ring_pool_free          (struct uniform_ring_pool *);

struct uniform_ring_loc uniform_ring_pool_alloc         (struct uniform_ring_pool *,
                                                         void const *bytes, size_t len);

// True when the current frame's ring usage has exceeded high_water.
_Bool                   uniform_ring_pool_should_rotate (struct uniform_ring_pool const *);

// Advance to the next frame, bump `rotations`, call wait_frame on the new
// cur, and reset its ring. The caller is responsible for committing/stashing
// any in-flight cmdbuf for the OLD cur BEFORE calling rotate — the pool only
// owns the index and the ring lifecycle.
void                    uniform_ring_pool_rotate        (struct uniform_ring_pool *);

// Wait on every frame's in-flight work and reset every ring. Used by flush.
void                    uniform_ring_pool_drain_all     (struct uniform_ring_pool *);
