#include "test.h"

enum { TEST_CAP = 4096 };

static test_fn registry[TEST_CAP];
static int     test_count;

void test_register(test_fn fn) {
    if (test_count < TEST_CAP) {
        registry[test_count++] = fn;
    }
}

int main(void) {
    for (int i = 0; i < test_count; i++) {
        registry[i]();
    }
    return 0;
}
