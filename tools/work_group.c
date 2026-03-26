#include "work_group.h"
#include <pthread.h>
#include <stdlib.h>


typedef struct {
    void (*fn)(void *);
    void  *ctx;
    int   *pending;
} work_item;

struct thread_pool {
    pthread_mutex_t mu;
    pthread_cond_t  cv;
    work_item      *stack;
    int             top, cap;
    int             n_threads, :32;
    pthread_t      *threads;
    int             quit, :32;
};

static _Bool pop(thread_pool *p, work_item *out) {
    pthread_mutex_lock(&p->mu);
    while (p->top == 0 && !p->quit) {
        pthread_cond_wait(&p->cv, &p->mu);
    }
    _Bool got = p->top > 0;
    if (got) { *out = p->stack[--p->top]; }
    pthread_mutex_unlock(&p->mu);
    return got;
}

static _Bool try_pop(thread_pool *p, work_item *out) {
    _Bool got = 0;
    pthread_mutex_lock(&p->mu);
    if (p->top > 0) { *out = p->stack[--p->top]; got = 1; }
    pthread_mutex_unlock(&p->mu);
    return got;
}

static void *worker(void *arg) {
    thread_pool *p = arg;
    for (;;) {
        work_item w;
        if (!pop(p, &w)) { break; }
        w.fn(w.ctx);
        __atomic_fetch_sub(w.pending, 1, __ATOMIC_RELEASE);
    }
    return NULL;
}

thread_pool *pool_create(int n) {
    thread_pool *p = calloc(1, sizeof *p);
    pthread_mutex_init(&p->mu, NULL);
    pthread_cond_init(&p->cv, NULL);
    p->cap = 64;
    p->stack = malloc((size_t)p->cap * sizeof *p->stack);
    p->n_threads = n;
    p->threads = malloc((size_t)n * sizeof *p->threads);
    for (int i = 0; i < n; i++) {
        pthread_create(&p->threads[i], NULL, worker, p);
    }
    return p;
}

void pool_destroy(thread_pool *p) {
    pthread_mutex_lock(&p->mu);
    p->quit = 1;
    pthread_cond_broadcast(&p->cv);
    pthread_mutex_unlock(&p->mu);
    for (int i = 0; i < p->n_threads; i++) {
        pthread_join(p->threads[i], NULL);
    }
    free(p->threads);
    free(p->stack);
    pthread_mutex_destroy(&p->mu);
    pthread_cond_destroy(&p->cv);
    free(p);
}

void work_group_add(work_group *wg, void (*fn)(void *), void *ctx) {
    __atomic_fetch_add(&wg->pending, 1, __ATOMIC_RELAXED);
    thread_pool *p = wg->pool;
    pthread_mutex_lock(&p->mu);
    if (p->top == p->cap) {
        p->cap *= 2;
        p->stack = realloc(p->stack, (size_t)p->cap * sizeof *p->stack);
    }
    p->stack[p->top++] = (work_item){fn, ctx, &wg->pending};
    pthread_cond_signal(&p->cv);
    pthread_mutex_unlock(&p->mu);
}

void work_group_wait(work_group *wg) {
    while (__atomic_load_n(&wg->pending, __ATOMIC_ACQUIRE) > 0) {
        work_item w;
        if (try_pop(wg->pool, &w)) {
            w.fn(w.ctx);
            __atomic_fetch_sub(w.pending, 1, __ATOMIC_RELEASE);
        }
    }
}
