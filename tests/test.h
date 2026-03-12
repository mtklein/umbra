#pragma once

int dprintf(int, char const[], ...);
#define here || (dprintf(1, "%s:%d failed\n",__FILE__,__LINE__),__builtin_debugtrap(),0)

static inline _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}
