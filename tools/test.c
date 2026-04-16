#include "../tests/test.h"
#include <string.h>

int main(int argc, char *argv[]) {
    char const *match  = NULL;
    int         shards = 1;
    int         shard  = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--match") == 0 && i + 1 < argc) {
            match = argv[++i];
        } else if (strcmp(argv[i], "--shards") == 0 && i + 1 < argc) {
            shards = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--shard") == 0 && i + 1 < argc) {
            shard = atoi(argv[++i]);
        } else {
            match = argv[i];
        }
    }
    test_run(match, shards, shard);
    return 0;
}
