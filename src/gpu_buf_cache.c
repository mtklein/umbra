#include "gpu_buf_cache.h"
#include <stdlib.h>

int gpu_buf_cache_get(struct gpu_buf_cache *c, void *host, size_t bytes,
                      uint8_t rw) {
    for (int i = 0; i < c->n; i++) {
        struct gpu_cache_entry *ce = &c->entry[i];
        if (ce->host == host && ce->buf.size >= bytes) {
            if (host && bytes
                    && !ce->nocopy              // Zero-copy: GPU reads host directly.
                    && !(ce->uploaded && ce->writable)) {  // Umbra owns writable bufs.
                fingerprint const fp = fingerprint_hash(host, bytes);
                if (!ce->fp_bytes || !fingerprint_eq(ce->fp, fp)) {
                    c->ops.upload(ce->buf, host, bytes, c->ctx);
                    c->upload_bytes += bytes;
                    ce->fp       = fp;
                    ce->fp_bytes = bytes;
                }
                ce->uploaded = 1;
            }
            if ((rw & BUF_WRITTEN) && !ce->copy_tracked && host && bytes) {
                ce->copy_tracked = 1;
            }
            return i;
        }
    }

    // Miss — grow if needed.
    if (c->n >= c->cap) {
        c->cap = c->cap ? 2 * c->cap : 16;
        c->entry = realloc(c->entry, (size_t)c->cap * sizeof *c->entry);
    }
    int const idx = c->n++;
    struct gpu_cache_entry *ce = &c->entry[idx];
    *ce = (struct gpu_cache_entry){0};

    // Try zero-copy import.
    if (c->ops.try_import && host && bytes) {
        gpu_buf imported = c->ops.try_import(host, bytes, c->ctx);
        if (imported.ptr) {
            ce->buf      = imported;
            ce->host     = host;
            ce->writable = rw & BUF_WRITTEN;
            ce->nocopy   = 1;
            ce->uploaded = 1;
            if (rw & BUF_WRITTEN) { ce->copy_tracked = 1; }
            return idx;
        }
    }

    // Allocate and upload.
    size_t const alloc_size = bytes ? bytes : 4;
    ce->buf = c->ops.alloc(alloc_size, c->ctx);
    ce->host     = host;
    ce->writable = rw & BUF_WRITTEN;
    if (host && bytes) {
        c->ops.upload(ce->buf, host, bytes, c->ctx);
        c->upload_bytes += bytes;
        ce->fp       = fingerprint_hash(host, bytes);
        ce->fp_bytes = bytes;
        ce->uploaded = 1;
    }
    if ((rw & BUF_WRITTEN) && host && bytes) {
        ce->copy_tracked = 1;
    }
    return idx;
}

void gpu_buf_cache_copyback(struct gpu_buf_cache *c) {
    for (int i = 0; i < c->n; i++) {
        struct gpu_cache_entry *ce = &c->entry[i];
        if (ce->copy_tracked && !ce->nocopy && ce->host) {
            size_t const bytes = ce->fp_bytes ? ce->fp_bytes : ce->buf.size;
            c->ops.download(ce->buf, ce->host, bytes, c->ctx);
        }
    }
}

void gpu_buf_cache_end_batch(struct gpu_buf_cache *c) {
    for (int i = 0; i < c->n; i++) {
        c->entry[i].copy_tracked = 0;
        c->entry[i].uploaded     = 0;
        if (c->entry[i].writable) {
            c->entry[i].fp_bytes = 0;
        }
    }
}

void gpu_buf_cache_free(struct gpu_buf_cache *c) {
    for (int i = 0; i < c->n; i++) {
        c->ops.release(c->entry[i].buf, c->ctx);
    }
    free(c->entry);
    c->entry = NULL;
    c->n = c->cap = 0;
}
