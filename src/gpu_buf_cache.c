#include "gpu_buf_cache.h"
#include <stdio.h>
#include <stdlib.h>

#if (defined(__has_feature) \
        && (__has_feature(address_sanitizer) || __has_feature(thread_sanitizer))) \
 || defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    #define UMBRA_SANITIZER_BUILD 1
#else
    #define UMBRA_SANITIZER_BUILD 0
#endif

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
                if (!ce->hashed_size || !fingerprint_eq(ce->fp, fp)) {
                    c->ops.upload(ce->buf, host, bytes, c->ctx);
                    c->upload_bytes += bytes;
                    ce->fp          = fp;
                    ce->hashed_size = bytes;
                }
                ce->uploaded = 1;
            }
            if (UMBRA_SANITIZER_BUILD && ce->sealed && !ce->writable && host && bytes) {
                // Trip-wire: a sealed read-only buffer's host bytes must not
                // have changed since the last time we saw them.  Live only
                // under sanitizers -- in production the seal is trusted and
                // hashing every lookup would defeat the optimization.  Skip
                // for sealed+writable because the GPU itself writes between
                // dispatches (we'd need an explicit eviction API to tell
                // host-mutation from realloc-to-same-address).
                fingerprint const fp = fingerprint_hash(host, bytes);
                if (!fingerprint_eq(ce->fp, fp)) {
                    fprintf(stderr,
                            "sealed buffer %p (%zu bytes) was modified after binding\n",
                            host, bytes);
                    __builtin_trap();
                }
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
        // Sealed read-only entries hash the seed under sanitizers so the
        // trip-wire has a baseline to compare against; sealed writable and
        // production sealed skip the hash.  hashed_size stays 0 for any
        // sealed entry to gate the re-upload check.
        _Bool const seed_hash = !sealed
                             || (UMBRA_SANITIZER_BUILD && !writable);
        ce->fp          = seed_hash ? fingerprint_hash(host, bytes) : (fingerprint){0};
        ce->hashed_size = sealed ? 0 : bytes;
        ce->uploaded = 1;
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
            // TODO: for sealed entries, hashed_size is 0 (we skip
            // hashing) so we fall back to ce->buf.size, which the backend's
            // alloc may have rounded up (wgpu rounds to 4 bytes).  Downloading
            // the rounded size into a smaller host buffer overflows -- ASAN
            // catches it in wgpu_cache_download's final memcpy.  Currently
            // unreached because no shipping caller pairs sealed with
            // writable + non-4-byte-aligned size on wgpu, but it's a real
            // latent bug.  Fix: track the original requested bytes alongside
            // ce->buf.size and use that for the download cap.
            size_t const bytes = ce->hashed_size ? ce->hashed_size : ce->buf.size;
            c->ops.download(ce->buf, ce->host, bytes, c->ctx);
            if (!ce->sealed) {
                // Host now matches GPU.  Refresh fingerprint so next batch can
                // skip the upload if the user doesn't modify the buffer.
                ce->fp          = fingerprint_hash(ce->host, bytes);
                ce->hashed_size = bytes;
            }
            // Sealed+writable doesn't refresh fp: the trip-wire skips it
            // anyway (host vs GPU mutation isn't distinguishable by
            // fingerprint), so the extra hash cost would be wasted.
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
