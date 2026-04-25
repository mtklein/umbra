#pragma once
#include "fingerprint.h"
#include "flat_ir.h"
#include <stddef.h>
#include <stdint.h>

struct deref_info { int buf_idx, src_buf, off; };

// Backend-specific handle for a GPU buffer.
typedef struct { void *ptr; size_t size; } gpu_buf;

// Callbacks the cache uses to talk to the backend.
struct gpu_buf_ops {
    gpu_buf (*alloc)     (size_t size, void *ctx);
    void    (*upload)    (gpu_buf, void const *host, size_t bytes, void *ctx);
    void    (*download)  (gpu_buf, void       *host, size_t bytes, void *ctx);
    gpu_buf (*try_import)(void *host, size_t bytes, void *ctx);
    void    (*release)   (gpu_buf, void *ctx);
};

struct gpu_cache_entry {
    gpu_buf      buf;
    void        *host;
    size_t       hashed_size;
    fingerprint  fp;
    _Bool        writable, copy_tracked, uploaded, nocopy;
    _Bool        sealed; int :24;
};

struct gpu_buf_cache {
    struct gpu_cache_entry *entry;
    int                     n, cap;
    struct gpu_buf_ops      ops;
    void                   *ctx;
    size_t                  upload_bytes;
};

// Lookup or create a cache entry for `host`.  Uploads if needed.
int  gpu_buf_cache_get      (struct gpu_buf_cache *, void *host, size_t bytes, uint8_t rw);

// Download writable buffers from GPU back to their host pointers.
void gpu_buf_cache_copyback (struct gpu_buf_cache *);

// Reset per-batch flags.  Fingerprints persist across batches so unmodified
// writable buffers can skip re-upload on the next batch.
void gpu_buf_cache_end_batch(struct gpu_buf_cache *);

// Release all GPU buffers and free the cache.
void gpu_buf_cache_free     (struct gpu_buf_cache *);
