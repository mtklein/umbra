#pragma once


struct thread_pool* thread_pool(int threads);
void   thread_pool_free(struct thread_pool*);

typedef struct {
    struct thread_pool *pool;
    int                 pending, :32;
} work_group;

void work_group_add (work_group*, void (*fn)(void *ctx), void *ctx);
void work_group_wait(work_group*);
