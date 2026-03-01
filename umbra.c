#include "umbra.h"
#include <stdlib.h>

typedef union {
    short  i16;
    int    i32;
    __fp16 f16;
    float  f32;
} val;

struct inst {
    int  (*fn)(struct inst const *ip, val *v, int i, void* ptr[]);
    int    x,y,z;
    int    imm;
    size_t offset;
};

struct umbra_program {
    int         insts;
    int         :32;
    struct inst inst[];
};

#define op(name) int name(struct inst const *ip, val *v, int i, void* ptr[])
#define next return ip[1].fn(ip+1, v+1, i, ptr)

static op(imm) {
    v->i32 = ip->imm;
    next;
}

static op(uni) {
    v->i32 = ((int*)ptr[ip->x])[0];
    next;
}

static op(gather) {
    v->i32 = ((int*)ptr[ip->x])[v[ip->y].i32];
    next;
}

static op(load) {
    v->i32 = ((int*)ptr[ip->x])[i];
    next;
}

static op(store) {
    ((int*)ptr[ip->x])[i] = v[-1].i32;
    next;
}

static op(add_f32) {
    v->f32 = v[ip->x].f32 + v[ip->y].f32;
    next;
}

static op(sub_f32) {
    v->f32 = v[ip->x].f32 - v[ip->y].f32;
    next;
}

static op(mul_f32) {
    v->f32 = v[ip->x].f32 * v[ip->y].f32;
    next;
}

static op(div_f32) {
    v->f32 = v[ip->x].f32 / v[ip->y].f32;
    next;
}

static op(done) {
    (void)ip;
    (void)v;
    (void)i;
    (void)ptr;
    return 0;
}

struct umbra_program* umbra_program(struct umbra_inst const inst[], int insts) {
    struct umbra_program *p = malloc(sizeof *p + (size_t)(insts+1) * sizeof *p->inst);
    for (int i = 0; i < insts; i++) {
        switch (inst[i].op) {
            case umbra_imm_32:
                p->inst[i] = (struct inst){.fn=imm, .imm=inst[i].immi};
                break;
            case umbra_uni_32:
                p->inst[i] = (struct inst){.fn=uni, .x=inst[i].ptr};
                break;
            case umbra_gather_32:
                p->inst[i] = (struct inst){.fn=gather, .x=inst[i].ptr, .y=inst[i].y - i};
                break;
            case umbra_load_32:
                p->inst[i] = (struct inst){.fn=load, .x=inst[i].ptr};
                break;
            case umbra_store_32:
                p->inst[i] = (struct inst){.fn=store, .x=inst[i].ptr};
                break;

            case umbra_add_f32:
                p->inst[i] = (struct inst){.fn=add_f32, .x=inst[i].x - i, .y=inst[i].y - i};
                break;
            case umbra_sub_f32:
                p->inst[i] = (struct inst){.fn=sub_f32, .x=inst[i].x - i, .y=inst[i].y - i};
                break;
            case umbra_mul_f32:
                p->inst[i] = (struct inst){.fn=mul_f32, .x=inst[i].x - i, .y=inst[i].y - i};
                break;
            case umbra_div_f32:
                p->inst[i] = (struct inst){.fn=div_f32, .x=inst[i].x - i, .y=inst[i].y - i};
                break;
        }
    }
    p->inst[insts] = (struct inst){.fn=done};
    p->insts = insts+1;
    return p;
}

void umbra_program_run(struct umbra_program const *p, int n, void *ptr[]) {
    val *v = malloc((size_t)p->insts * sizeof *v);
    for (int i = 0; i < n; i++) {
        p->inst[0].fn(p->inst, v, i, ptr);
    }
    free(v);
}

void umbra_program_free(struct umbra_program *p) {
    free(p);
}
