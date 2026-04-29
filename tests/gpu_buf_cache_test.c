#include "../src/gpu_buf_cache.h"
#include "test.h"
#include <string.h>

struct mock_ctx {
    int allocs, uploads, downloads, imports, releases;
};

static gpu_buf mock_alloc(size_t size, void *ctx) {
    ((struct mock_ctx *)ctx)->allocs++;
    return (gpu_buf){.ptr = malloc(size), .size = size};
}

static void mock_upload(gpu_buf buf, void const *host, size_t bytes, void *ctx) {
    ((struct mock_ctx *)ctx)->uploads++;
    memcpy(buf.ptr, host, bytes);
}

static void mock_download(gpu_buf buf, void *host, size_t bytes, void *ctx) {
    ((struct mock_ctx *)ctx)->downloads++;
    memcpy(host, buf.ptr, bytes);
}

static gpu_buf mock_import(void *host, size_t bytes, void *ctx) {
    ((struct mock_ctx *)ctx)->imports++;
    (void)host; (void)bytes;
    return (gpu_buf){0}; // always fail
}

static void mock_release(gpu_buf buf, void *ctx) {
    ((struct mock_ctx *)ctx)->releases++;
    free(buf.ptr);
}

static struct gpu_buf_cache make_cache(struct mock_ctx *m) {
    return (struct gpu_buf_cache){
        .ops = {mock_alloc, mock_upload, mock_download, mock_import, mock_release},
        .ctx = m,
    };
}

TEST(test_gpu_buf_cache_miss_then_hit) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char data[64];
    memset(data, 0x42, sizeof data);

    int i0 = gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    i0 == 0 here;
    m.allocs  == 1 here;
    m.uploads == 1 here;

    // Second get: same host pointer → hit, uploaded=1, skip.
    int i1 = gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    i1 == 0 here;
    m.uploads == 1 here; // no re-upload

    gpu_buf_cache_free(&c);
    m.releases == 1 here;
}

TEST(test_gpu_buf_cache_end_batch_resets_uploaded) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char data[64];
    memset(data, 0x42, sizeof data);

    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    m.uploads == 1 here;

    // End batch resets uploaded flag.
    gpu_buf_cache_end_batch(&c);

    // Next get re-checks fingerprint.  Data unchanged → skip upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    m.uploads == 1 here; // fingerprint matched, no re-upload

    // Modify data → fingerprint changes → re-upload.
    data[0] = 0x7F;
    gpu_buf_cache_end_batch(&c);
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    m.uploads == 2 here;

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_writable_skip_reupload) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char data[64];
    memset(data, 0x42, sizeof data);

    // First access: seed upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN, 0);
    m.uploads == 1 here;
    c.entry[0].copy_tracked here;

    // Within batch: no re-upload, no re-hash (uploaded flag skips block).
    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN, 0);
    m.uploads == 1 here;

    // Simulate a flush: GPU wrote new bytes, copyback brings them to host and
    // refreshes the fingerprint so it reflects post-copyback host state.
    memset(c.entry[0].buf.ptr, 0xAB, sizeof data);
    gpu_buf_cache_copyback(&c);
    m.downloads == 1 here;

    gpu_buf_cache_end_batch(&c);

    // Next batch: host unchanged since copyback → hash matches → skip upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN, 0);
    m.uploads == 1 here;

    // If user modifies host between flushes, hash mismatches → re-upload.
    data[0] = 0x7F;
    gpu_buf_cache_end_batch(&c);
    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN, 0);
    m.uploads == 2 here;

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_null_host) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    // Dummy buffer for unbound slots.
    int i = gpu_buf_cache_get(&c, NULL, 0, BUF_READ, 0);
    i == 0 here;
    m.allocs == 1 here;
    m.uploads == 0 here; // no data to upload

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_multiple_buffers) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char a[32], b[64];
    memset(a, 1, sizeof a);
    memset(b, 2, sizeof b);

    int ia = gpu_buf_cache_get(&c, a, sizeof a, BUF_READ, 0);
    int ib = gpu_buf_cache_get(&c, b, sizeof b, BUF_WRITTEN, 0);
    ia == 0 here;
    ib == 1 here;
    c.entries == 2 here;

    // Hits on both.
    gpu_buf_cache_get(&c, a, sizeof a, BUF_READ, 0) == 0 here;
    gpu_buf_cache_get(&c, b, sizeof b, BUF_WRITTEN, 0) == 1 here;
    m.uploads == 2 here; // only the initial seeds

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_sealed_skips_hash_and_reupload) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char data[64];
    memset(data, 0x42, sizeof data);

    // First access: one seed upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 1);
    m.uploads == 1 here;
    c.entry[0].sealed here;

    // Subsequent accesses within batch: skip.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 1);
    m.uploads == 1 here;

    // Even if data changes, sealed entries don't re-upload.
    data[0] = 0x7F;
    gpu_buf_cache_end_batch(&c);
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 1);
    m.uploads == 1 here;

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_read_modified_within_batch) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char data[64];
    memset(data, 0x42, sizeof data);

    // First get: allocate + upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    m.uploads == 1 here;
    memcmp(c.entry[0].buf.ptr, data, sizeof data) == 0 here;

    // User modifies the buffer (e.g. updates uniforms) between dispatch() calls.
    data[0] = 0x7F;

    // Second get within the same batch must notice the change and re-upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    m.uploads == 2 here;
    memcmp(c.entry[0].buf.ptr, data, sizeof data) == 0 here;

    gpu_buf_cache_free(&c);
}

static gpu_buf mock_import_ok(void *host, size_t bytes, void *ctx) {
    ((struct mock_ctx *)ctx)->imports++;
    return (gpu_buf){.ptr = host, .size = bytes};
}

static void mock_release_nofree(gpu_buf buf, void *ctx) {
    ((struct mock_ctx *)ctx)->releases++;
    (void)buf;
}

TEST(test_gpu_buf_cache_import_nocopy) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = {
        .ops = {mock_alloc, mock_upload, mock_download, mock_import_ok, mock_release_nofree},
        .ctx = &m,
    };

    char data[64];
    memset(data, 0x42, sizeof data);

    // Successful import: no alloc, no upload.
    int i = gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    i == 0 here;
    m.imports == 1 here;
    m.allocs  == 0 here;
    m.uploads == 0 here;
    c.entry[0].nocopy   here;
    c.entry[0].uploaded here;
    c.entry[0].buf.ptr == data here;

    // Cache hit within batch: still no upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    m.uploads == 0 here;

    gpu_buf_cache_free(&c);
    m.releases == 1 here;
}

// Backends like wgpu round their GPU alloc up to a 4-byte boundary.  Make sure
// copyback caps the host download at the requested size, not the rounded-up
// alloc size, so a sealed+writable buffer with a non-multiple-of-4 byte count
// doesn't have copyback overrun the host buffer.  ASan catches the overrun.
static gpu_buf mock_alloc_round4(size_t size, void *ctx) {
    ((struct mock_ctx *)ctx)->allocs++;
    size_t aligned = (size + 3) & ~(size_t)3;
    return (gpu_buf){.ptr = malloc(aligned), .size = aligned};
}

TEST(test_gpu_buf_cache_copyback_respects_requested_size) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = {
        .ops = {mock_alloc_round4, mock_upload, mock_download, NULL, mock_release},
        .ctx = &m,
    };

    char data[6];   // not a multiple of 4
    memset(data, 0x42, sizeof data);

    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN, 1);
    c.entry[0].buf.size == 8 here;

    // Pretend the GPU wrote across the rounded buffer.
    memset(c.entry[0].buf.ptr, 0xAB, c.entry[0].buf.size);

    // Copyback must download only sizeof data bytes to the host buffer; if it
    // downloaded the rounded 8, ASan would catch the 2-byte stack overrun.
    gpu_buf_cache_copyback(&c);
    m.downloads == 1 here;
    data[0] == (char)0xAB here;
    data[5] == (char)0xAB here;

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_import_nocopy_writable) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = {
        .ops = {mock_alloc, mock_upload, mock_download, mock_import_ok, mock_release_nofree},
        .ctx = &m,
    };

    char data[64];
    memset(data, 0x42, sizeof data);

    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN, 0);
    m.allocs  == 0 here;
    m.uploads == 0 here;
    c.entry[0].nocopy       here;
    c.entry[0].copy_tracked here;
    c.entry[0].writable     here;

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_copyback) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char data[64];
    memset(data, 0x42, sizeof data);

    // Read-only buffer: copyback should skip it.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    gpu_buf_cache_copyback(&c);
    m.downloads == 0 here;

    // Writable buffer: copyback should download.
    char dst[64] = {0};
    gpu_buf_cache_get(&c, dst, sizeof dst, BUF_WRITTEN, 0);

    // Simulate GPU writing to the buffer.
    memset(c.entry[1].buf.ptr, 0xAB, sizeof dst);

    gpu_buf_cache_copyback(&c);
    m.downloads == 1 here;
    (unsigned char)dst[0] == 0xAB here;

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_upload_bytes_tracked) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char data[100];
    memset(data, 0x42, sizeof data);

    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    c.upload_bytes == 100 here;

    // Hit within batch: no additional upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    c.upload_bytes == 100 here;

    // After end_batch + data change: new upload.
    data[0] = 0x7F;
    gpu_buf_cache_end_batch(&c);
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ, 0);
    c.upload_bytes == 200 here;

    gpu_buf_cache_free(&c);
}
