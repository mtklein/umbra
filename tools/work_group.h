#pragma once

typedef struct thread_pool thread_pool;

thread_pool *pool_create(int n_threads);
void         pool_destroy(thread_pool *);

typedef struct {
    thread_pool *pool;
    int          pending, :32;
} work_group;

void work_group_add(work_group *, void (*fn)(void *), void *ctx);
void work_group_wait(work_group *);
