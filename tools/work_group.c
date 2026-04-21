#include "work_group.h"
#include <pthread.h>
#include <stdlib.h>

struct work_item {
    void (*fn)(void *);
    void  *ctx;
    int   *pending;
};

struct thread_pool {
    pthread_mutex_t   mu;
    pthread_cond_t    cv;
    pthread_t        *thread;
    struct work_item *stack;
    int               threads, top, cap, quit;
};

static _Bool pop(struct thread_pool *p, struct work_item *out) {
    pthread_mutex_lock(&p->mu);
    while (p->top == 0 && p->quit == 0) {
        pthread_cond_wait(&p->cv, &p->mu);
    }
    _Bool const got = p->top > 0;
    if (got) { *out = p->stack[--p->top]; }
    pthread_mutex_unlock(&p->mu);
    return got;
}

static _Bool try_pop(struct thread_pool *p, struct work_item *out) {
    pthread_mutex_lock(&p->mu);
    _Bool const got = p->top > 0;
    if (got) { *out = p->stack[--p->top]; }
    pthread_mutex_unlock(&p->mu);
    return got;
}

static void* worker(void *arg) {
    struct thread_pool *p = arg;
    for (;;) {
        struct work_item w;
        if (!pop(p, &w)) { break; }
        w.fn(w.ctx);
        __atomic_fetch_sub(w.pending, 1, __ATOMIC_RELEASE);
    }
    return NULL;
}

struct thread_pool* thread_pool(int threads) {
    struct thread_pool *p = calloc(1, sizeof *p);
    pthread_mutex_init(&p->mu, NULL);
    pthread_cond_init(&p->cv, NULL);
    p->cap = 64;
    p->stack = malloc((size_t)p->cap * sizeof *p->stack);
    p->threads = threads;
    p->thread = malloc((size_t)threads * sizeof *p->thread);
    for (int i = 0; i < threads; i++) {
        pthread_create(&p->thread[i], NULL, worker, p);
    }
    return p;
}

void thread_pool_free(struct thread_pool *p) {
    pthread_mutex_lock(&p->mu);
    p->quit = 1;
    pthread_cond_broadcast(&p->cv);
    pthread_mutex_unlock(&p->mu);
    for (int i = 0; i < p->threads; i++) {
        pthread_join(p->thread[i], NULL);
    }
    free(p->thread);
    free(p->stack);
    pthread_mutex_destroy(&p->mu);
    pthread_cond_destroy(&p->cv);
    free(p);
}

void work_group_add(struct work_group *wg, void (*fn)(void *), void *ctx) {
    __atomic_fetch_add(&wg->pending, 1, __ATOMIC_RELAXED);
    struct thread_pool *p = wg->pool;
    pthread_mutex_lock(&p->mu);
    if (p->top == p->cap) {
        p->cap *= 2;
        p->stack = realloc(p->stack, (size_t)p->cap * sizeof *p->stack);
    }
    p->stack[p->top++] = (struct work_item){fn, ctx, &wg->pending};
    pthread_cond_signal(&p->cv);
    pthread_mutex_unlock(&p->mu);
}

void work_group_wait(struct work_group *wg) {
    while (__atomic_load_n(&wg->pending, __ATOMIC_ACQUIRE) > 0) {
        struct work_item w;
        if (try_pop(wg->pool, &w)) {
            w.fn(w.ctx);
            __atomic_fetch_sub(w.pending, 1, __ATOMIC_RELEASE);
        }
    }
}
