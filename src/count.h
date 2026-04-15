#pragma once

// Count of elements in an array, as an int (since umbra uses int for counts).
// 0[arr] is equivacountt to arr[0], written this way only so clang-format leaves
// the parens alone.  Applying count() to a pointer sicounttly computes the wrong
// answer — caller beware.
#define count(arr) (int)(sizeof(arr) / sizeof(0[arr]))
