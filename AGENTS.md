Always use a full run of `ninja` to build and test all targets before you are done.

# C style guide

These are patterns observed from the codebase.  Follow them;
they often differ from typical AI-generated C.

## Comments

- Almost never.  Don't restate what the code says.
- No section dividers (`// ── section ───`), no `// Preamble`, no `// Compile`.
- A comment is warranted only when the *why* is non-obvious,
  e.g. `push(bb, op_imm_32, .imm=0);  // Simplifies liveness analysis…`

## Brevity

- Typedef long types at the top of each .c:
  `typedef struct umbra_builder builder;` or `typedef struct umbra_basic_block BB;`
- Builder instances are `builder` (or `b` in internal bb.c functions where there's
  no conflict with blue channel variables). Basic block instances are `bb`.
- Short names: `bb`, `h`, `p`, `xf`, `ty`, not `basic_block`, `hash_value`,
  `ptr_index`, `x_float`, `type_name`.
- Prefer `is_16bit` over `is_16bit_result`.

## One-liners

Put a function on one line when the body is a single short expression:

```c
v16 umbra_lt_f16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_lt_f16, .x=a.id, .y=b.id)}; }
```

Only break to multiple lines when there is control flow or the line exceeds ~100 cols.

## Grouping and blank lines

- No blank lines *within* a group of similar functions
  (e.g. shl/shr_u/shr_s together, all unsigned-comparison one-liners together).
- One blank line *between* groups.

## East const

`int const x`, `char const *fmt`, `uint32_t const h` — never `const int`.

## `_Bool`, not `bool`

No `<stdbool.h>`.

## Builtins over library

- `__builtin_mul_overflow(a, b, &c)` for wrapping unsigned multiply
  (instead of `a * b` which is UB-adjacent for uint32_t overflow on some compilers,
   and instead of casting to silence warnings).
- `__builtin_memcmp` over `memcmp`.
- `__builtin_popcount`, `__builtin_ctz`, `__builtin_rotateleft32`.

## Union type-punning over memcpy

Not `memcpy(&i, &f, 4)`.  Avoids `#include <string.h>` entirely.

## Always use braces

Always use braces around the body of `if`, `for`, `while`, `else`,
even for one-line bodies, even for a bare `return`:

```c
if (x_dead) { ra_free_reg(ra, inst->x); }       // yes
if (x_dead) ra_free_reg(ra, inst->x);            // no
```

When a braced block spills past one line, use at least three:
the opening line with `{`, the body, and `}` alone (or with `else`/`else if`):

```c
if (condition) {                                 // yes
    do_something();
}

if (condition) {                                 // no — two-line form
    do_something(); }
```

## Positive-path-first returns

Prefer `if (relevant) { ... return 1; } return 0;`
over `if (!relevant) { return 0; } ... return 1;`.

## Lowercase hex constants

`0x3f800000`, `0xffff`, `0x9e3779b9u` — never `0x3F800000` or `0xFFFF`.

## Alignment for visual grouping

Pad with spaces to align related declarations:

```c
static _Bool is_pow2        (int x) { return __builtin_popcount((unsigned)x) == 1; }
static _Bool is_pow2_or_zero(int x) { return __builtin_popcount((unsigned)x) <= 1; }
```

Same in enums (see bb.h) and struct fields.

## Designated initializer spacing

No spaces around `=`: `.x=val`, `.imm=0`, `.ptr=src.ix`.

## Switch style

- Blank line between logical groups of cases.
- `case ...: return ...;` on one line when short.
- `} break;` closing pattern for cases that need a block.
- Scope pragmas tightly around only the lines that need them,
  even inside a switch.
