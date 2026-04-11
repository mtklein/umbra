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

    int i0 = gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    i0 == 0 here;
    m.allocs  == 1 here;
    m.uploads == 1 here;

    // Second get: same host pointer → hit, uploaded=1, skip.
    int i1 = gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
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

    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    m.uploads == 1 here;

    // End batch resets uploaded flag.
    gpu_buf_cache_end_batch(&c);

    // Next get re-checks fingerprint.  Data unchanged → skip upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    m.uploads == 1 here; // fingerprint matched, no re-upload

    // Modify data → fingerprint changes → re-upload.
    data[0] = 0x7F;
    gpu_buf_cache_end_batch(&c);
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    m.uploads == 2 here;

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_writable_invalidates_fp) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char data[64];
    memset(data, 0x42, sizeof data);

    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN);
    m.uploads == 1 here;
    c.entry[0].copy_tracked here;

    // Within batch: uploaded flag skips.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN);
    m.uploads == 1 here;

    // End batch: writable entries get fp invalidated.
    gpu_buf_cache_end_batch(&c);
    !c.entry[0].copy_tracked here;
    c.entry[0].fp_bytes == 0 here;

    // Next get must re-upload (fp invalid even though data unchanged).
    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN);
    m.uploads == 2 here;

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_null_host) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    // Dummy buffer for unbound slots.
    int i = gpu_buf_cache_get(&c, NULL, 0, BUF_READ);
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

    int ia = gpu_buf_cache_get(&c, a, sizeof a, BUF_READ);
    int ib = gpu_buf_cache_get(&c, b, sizeof b, BUF_WRITTEN);
    ia == 0 here;
    ib == 1 here;
    c.n == 2 here;

    // Hits on both.
    gpu_buf_cache_get(&c, a, sizeof a, BUF_READ) == 0 here;
    gpu_buf_cache_get(&c, b, sizeof b, BUF_WRITTEN) == 1 here;
    m.uploads == 2 here; // only the initial uploads

    gpu_buf_cache_free(&c);
}

TEST(test_gpu_buf_cache_read_modified_within_batch) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = make_cache(&m);

    char data[64];
    memset(data, 0x42, sizeof data);

    // First get: allocate + upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    m.uploads == 1 here;
    memcmp(c.entry[0].buf.ptr, data, sizeof data) == 0 here;

    // User modifies the buffer (e.g. updates uniforms) between queue() calls.
    data[0] = 0x7F;

    // Second get within the same batch must notice the change and re-upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
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
    int i = gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    i == 0 here;
    m.imports == 1 here;
    m.allocs  == 0 here;
    m.uploads == 0 here;
    c.entry[0].nocopy   here;
    c.entry[0].uploaded here;
    c.entry[0].buf.ptr == data here;

    // Cache hit within batch: still no upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    m.uploads == 0 here;

    gpu_buf_cache_free(&c);
    m.releases == 1 here;
}

TEST(test_gpu_buf_cache_import_nocopy_writable) {
    struct mock_ctx m = {0};
    struct gpu_buf_cache c = {
        .ops = {mock_alloc, mock_upload, mock_download, mock_import_ok, mock_release_nofree},
        .ctx = &m,
    };

    char data[64];
    memset(data, 0x42, sizeof data);

    gpu_buf_cache_get(&c, data, sizeof data, BUF_WRITTEN);
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
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    gpu_buf_cache_copyback(&c);
    m.downloads == 0 here;

    // Writable buffer: copyback should download.
    char dst[64] = {0};
    gpu_buf_cache_get(&c, dst, sizeof dst, BUF_WRITTEN);

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

    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    c.upload_bytes == 100 here;

    // Hit within batch: no additional upload.
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    c.upload_bytes == 100 here;

    // After end_batch + data change: new upload.
    data[0] = 0x7F;
    gpu_buf_cache_end_batch(&c);
    gpu_buf_cache_get(&c, data, sizeof data, BUF_READ);
    c.upload_bytes == 200 here;

    gpu_buf_cache_free(&c);
}
