#pragma once
#include "bb.h"
#include <stdint.h>

typedef void (*ra_spill_fn)(int reg, int slot, void *ctx);
typedef void (*ra_fill_fn) (int reg, int slot, void *ctx);

struct ra_config {
    int8_t const *pool;
    ra_spill_fn   spill;
    ra_fill_fn    fill;
    void         *ctx;
    int           nregs, max_reg, has_pairs, :32;
};

struct ra {
    int    *last_use;
    int8_t *reg;
    int8_t *reg_hi;
    _Bool  *is_pair;
    int    *owner;
    int8_t *free_stack;
    struct ra_config cfg;
    int     nfree, :32;
};

struct ra* ra_create (struct umbra_basic_block const *bb, struct ra_config const *cfg);
void       ra_destroy(struct ra *ra);
void       ra_free_reg(struct ra *ra, int val);
int8_t     ra_alloc  (struct ra *ra, int *sl, int *ns);
int8_t     ra_ensure (struct ra *ra, int *sl, int *ns, int val);
int8_t     ra_claim  (struct ra *ra, int old_val, int new_val);
