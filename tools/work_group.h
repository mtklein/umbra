#pragma once

struct thread_pool* thread_pool(int threads);
void   thread_pool_free(struct thread_pool*);

struct work_group {
    struct thread_pool *pool;
    int                 pending, :32;
};

void work_group_add (struct work_group*, void (*fn)(void *ctx), void *ctx);
void work_group_wait(struct work_group*);
