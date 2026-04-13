#include "../tests/test.h"
#include <string.h>

int main(int argc, char *argv[]) {
    char const *match = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            test_debug = 1;
        } else if (strcmp(argv[i], "--match") == 0 && i + 1 < argc) {
            match = argv[++i];
        } else {
            match = argv[i];
        }
    }
    test_run(match);
    return 0;
}
