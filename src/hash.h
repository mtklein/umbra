#pragma once

struct hash {
    unsigned vals, slots;
    void    *data;
};

void  hash_insert(struct hash *, unsigned hash, int val);
_Bool hash_lookup(struct hash,   unsigned hash, _Bool (*match)(int val, void *ctx), void *ctx);
