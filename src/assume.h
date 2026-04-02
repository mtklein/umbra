#pragma once

#if defined(__clang__)
    #define assume(x) __builtin_assume(x)
#else
    #define assume(x) if (!(x)) __builtin_unreachable()
#endif
