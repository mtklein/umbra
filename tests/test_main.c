#include "test.h"
#include <stdlib.h>

static struct test_alloc const test_mem = { malloc, free };

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

int main(void) {
    for (int i = 0; i < test_count; i++) {
        registry[i].fn(&test_mem);
    }
    return 0;
}
