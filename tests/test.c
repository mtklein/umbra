#include "test.h"
#include "../src/assume.h"
#include <string.h>

enum { TEST_CAP = 4096 };

static test_fn    registry[TEST_CAP];
static char const *names[TEST_CAP];
static int         test_count;

void test_register(test_fn fn, char const *name) {
    assume(test_count < TEST_CAP);
    registry[test_count] = fn;
    names[test_count] = name;
    test_count++;
}

void test_run(char const *match, int shards, int shard) {
    for (int i = 0; i < test_count; i++) {
        if ((shards <= 1 || i % shards == shard)
                && (!match || strstr(names[i], match))) {
            registry[i]();
        }
    }
}

_Bool equiv(float x, float y) {
    return (x <= y && y <= x) || (x != x && y != y);
}
_Bool val_eq(umbra_val32 a, umbra_val32 b) { return a.id == b.id && a.chan == b.chan; }
_Bool val_lt(umbra_val32 a, umbra_val32 b) {
    return a.id < b.id || (a.id == b.id && a.chan < b.chan);
}

struct test_backends test_backends_make(struct umbra_flat_ir const *ir) {
    struct test_backends B;
    B.be[0] = umbra_backend_interp();
    B.be[1] = umbra_backend_jit();
    B.be[2] = umbra_backend_metal();
    B.be[3] = umbra_backend_vulkan();
    B.be[4] = umbra_backend_wgpu();
    for (int i = 0; i < NUM_BACKENDS; i++) {
        B.p[i] = B.be[i] ? B.be[i]->compile(B.be[i], ir) : NULL;
    }
    B.p[0] != 0 here;
#if defined(__aarch64__) || defined(__AVX2__)
    B.p[1] != 0 here;
#endif
#if (defined(__APPLE__) && defined(__clang__)) && !defined(__wasm__)
    B.p[2] != 0 here;
#endif
#if defined(__APPLE__) && defined(__aarch64__) && !defined(__wasm__)
    B.p[4] != 0 here;
#endif
    return B;
}

_Bool test_backends_run(struct test_backends *B, int bi, int r, int b) {
    if (B->p[bi]) {
        B->p[bi]->queue(B->p[bi], 0, 0, r, b, 0, NULL);
        B->be[bi]->flush(B->be[bi]);
        return 1;
    }
    return 0;
}

void test_backends_free(struct test_backends *B) {
    for (int i = 0; i < NUM_BACKENDS; i++) {
        umbra_program_free(B->p[i]);
        umbra_backend_free(B->be[i]);
    }
}
