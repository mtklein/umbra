#include "test.h"
#include <stdlib.h>
#ifndef __wasm__
    #include <unistd.h>
#endif

#ifdef __wasm__
static void *aligned_alloc_(size_t sz) {
    return calloc(1, sz);
}
#else
static void *aligned_alloc_(size_t sz) {
    void *p = 0;
    posix_memalign(&p, (size_t)sysconf(_SC_PAGESIZE), sz);
    if (p) { __builtin_memset(p, 0, sz); }
    return p;
}
static void *misaligned_alloc_(size_t sz) {
    size_t pg = (size_t)sysconf(_SC_PAGESIZE);
    void *p = 0;
    posix_memalign(&p, pg, sz + 16);
    if (!p) { return 0; }
    __builtin_memset(p, 0, sz + 16);
    return (char*)p + 16;
}
static void misaligned_free_(void *p) {
    if (p) { free((char*)p - 16); }
}
#endif

static struct test_alloc const test_aligned    = { aligned_alloc_,    free };
#ifndef __wasm__
static struct test_alloc const test_misaligned = { misaligned_alloc_, misaligned_free_ };
#endif

enum { TEST_CAP = 4096 };

struct test_entry {
    char const *name;
    test_fn     fn;
};

static struct test_entry registry[TEST_CAP];
static int               test_count;

void test_register(char const *name, test_fn fn) {
    if (test_count < TEST_CAP) {
        registry[test_count].name = name;
        registry[test_count].fn   = fn;
        test_count++;
    }
}

static void run_all(struct test_alloc const *mem) {
    for (int i = 0; i < test_count; i++) {
        registry[i].fn(mem);
    }
}

int main(void) {
    run_all(&test_aligned);
#ifndef __wasm__
    run_all(&test_misaligned);
#endif
    return 0;
}
