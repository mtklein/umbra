#include "gpu_buf_cache.h"
#include <stdlib.h>

int gpu_buf_cache_get(struct gpu_buf_cache *c, void *host, size_t bytes,
                      uint8_t rw) {
    _Bool const writable = rw & BUF_WRITTEN,
                sealed   = rw & BUF_SEALED;
    for (int i = 0; i < c->n; i++) {
        struct gpu_cache_entry *ce = &c->entry[i];
        if (ce->host == host && ce->buf.size >= bytes) {
            if (host && bytes
                    && !ce->nocopy                        // Zero-copy: GPU reads host directly.
                    && !ce->sealed                        // Host promised not to mutate.
                    && !(ce->uploaded && ce->writable)) { // Umbra owns writable bufs within a batch.
                fingerprint const fp = fingerprint_hash(host, bytes);
                if (!ce->host_bytes || !fingerprint_eq(ce->fp, fp)) {
                    c->ops.upload(ce->buf, host, bytes, c->ctx);
                    c->upload_bytes += bytes;
                    ce->fp         = fp;
                    ce->host_bytes = bytes;
                }
                ce->uploaded = 1;
            }
            if (writable && !ce->copy_tracked && host && bytes) {
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
            ce->writable = writable;
            ce->nocopy   = 1;
            ce->uploaded = 1;
            if (writable) { ce->copy_tracked = 1; }
            return idx;
        }
    }

    size_t const alloc_size = bytes ? bytes : 4;
    ce->buf = c->ops.alloc(alloc_size, c->ctx);
    ce->host     = host;
    ce->writable = writable;
    ce->sealed   = sealed;
    if (host && bytes) {
        c->ops.upload(ce->buf, host, bytes, c->ctx);
        c->upload_bytes += bytes;
        ce->fp         = sealed ? (fingerprint){0} : fingerprint_hash(host, bytes);
        // Always record the requested bytes -- the skip-re-upload check is
        // gated on !ce->sealed independently, but copyback needs the original
        // size so it doesn't download the alloc-rounded buf.size into a
        // smaller host buffer.
        ce->host_bytes = bytes;
        ce->uploaded   = 1;
    }
    if (writable && host && bytes) {
        ce->copy_tracked = 1;
    }
    return idx;
}

void gpu_buf_cache_copyback(struct gpu_buf_cache *c) {
    for (int i = 0; i < c->n; i++) {
        struct gpu_cache_entry *ce = &c->entry[i];
        if (ce->copy_tracked && !ce->nocopy && ce->host) {
            size_t const bytes = ce->host_bytes;
            c->ops.download(ce->buf, ce->host, bytes, c->ctx);
            if (!ce->sealed) {
                // Host now matches GPU.  Refresh fingerprint so next batch can
                // skip the upload if the user doesn't modify the buffer.
                ce->fp = fingerprint_hash(ce->host, bytes);
            }
        }
    }
}

void gpu_buf_cache_end_batch(struct gpu_buf_cache *c) {
    for (int i = 0; i < c->n; i++) {
        c->entry[i].copy_tracked = 0;
        c->entry[i].uploaded     = 0;
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
