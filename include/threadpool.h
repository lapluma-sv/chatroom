#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "protocol.h"

typedef struct task {
    int sender_fd;
    int type;
    int body_len;
    char body[PROTOCOL_MAX_BODY_SIZE];
    struct task *next;
} task_t;

typedef struct threadpool {
    pthread_t *threads;
    int thread_count;
    task_t *head, *tail;                 // 带尾指针，O(1) 入队
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    int shutdown;                        // 0 运行 / 1 要求退出
} threadpool_t;

threadpool_t *threadpool_create(int thread_count);
int threadpool_submit(threadpool_t *tp, int sender_fd, int type, const char *body, int body_len);
void threadpool_destroy(threadpool_t *tp);


#endif
