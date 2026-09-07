#include "threadpool.h"
#include "broadcast.h"

static void *threadpool_worker(void *arg)
{
    threadpool_t *tp = (threadpool_t *)arg;
    task_t *task = NULL;
    while(1)
    {
        pthread_mutex_lock(&tp->lock);
        while(tp->shutdown == 0 && tp->head == NULL)
        {
            pthread_cond_wait(&tp->not_empty, &tp->lock);
        }
        if(tp->shutdown == 1 && tp->head == NULL)
        {
            pthread_mutex_unlock(&tp->lock);
            return NULL;
        }
        task = tp->head;
        tp->head = tp->head->next;
        if(tp->head == NULL)
        {
            tp->tail = NULL;
        }
        pthread_mutex_unlock(&tp->lock);
        if(task -> sender_fd >= 0)
        {
            broadcast_text(task->sender_fd, task->body);
        }
        else
        {
            broadcast_system(task->body);
        }
        free(task);
    }
}

threadpool_t *threadpool_create(int thread_count)
{
    threadpool_t *tp = malloc(sizeof(threadpool_t));
    if(tp == NULL || thread_count <= 0)
    {
        return NULL;
    }
    tp->threads = malloc(sizeof(pthread_t) * thread_count);
    if(tp->threads == NULL)
    {
        free(tp);
        return NULL;
    }
    tp->thread_count = thread_count;
    tp->head = tp->tail = NULL;
    tp->shutdown = 0;
    if(pthread_mutex_init(&tp->lock, NULL) != 0)
    {
        free(tp->threads);
        free(tp);
        return NULL;
    }
    if(pthread_cond_init(&tp->not_empty, NULL) != 0)
    {
        pthread_mutex_destroy(&tp->lock);
        free(tp->threads);
        free(tp);
        return NULL;
    }
    for(int i = 0; i < thread_count; i++)
    {
        if(pthread_create(&tp->threads[i], NULL, threadpool_worker, tp) != 0)
        {
            pthread_mutex_lock(&tp->lock);
            tp->shutdown = 1;
            pthread_cond_broadcast(&tp->not_empty);
            pthread_mutex_unlock(&tp->lock);
            for(int j = 0; j < i; j++)
            {
                pthread_join(tp->threads[j], NULL);
            }
            pthread_mutex_destroy(&tp->lock);
            pthread_cond_destroy(&tp->not_empty);
            free(tp->threads);
            free(tp);
            return NULL;
        }
    }
    return tp;
}

int threadpool_submit(threadpool_t *tp, int sender_fd, int type, const char *body, int body_len)
{
    if(tp == NULL)
    {
        return -EINVAL;
    }
    task_t *task = malloc(sizeof(task_t));
    if(task == NULL)
    {
        return -ENOMEM;
    }
    if(body_len > PROTOCOL_MAX_BODY_SIZE)
    {
        free(task);
        return -EINVAL;
    }
    task->sender_fd = sender_fd;
    task->type = type;
    task->body_len = body_len;
    memcpy(task->body, body, body_len);
    task->next = NULL;
    pthread_mutex_lock(&tp->lock);
    if(tp->head == NULL)
    {
        tp->head = task;
        tp->tail = task;
    }
    else
    {
        tp->tail->next = task;
        tp->tail = task;
    }
    pthread_mutex_unlock(&tp->lock);
    pthread_cond_signal(&tp->not_empty);
    return 0;
}

void threadpool_destroy(threadpool_t *tp)           // 置 shutdown + broadcast 唤醒 + 排干 + join + 释放
{
    if(tp == NULL)
    {
        return;
    }
    pthread_mutex_lock(&tp->lock);
    tp->shutdown = 1;
    pthread_cond_broadcast(&tp->not_empty);
    pthread_mutex_unlock(&tp->lock);
    for(int i = 0; i < tp->thread_count; i++)
    {
        pthread_join(tp->threads[i], NULL);
    }
    pthread_mutex_destroy(&tp->lock);
    pthread_cond_destroy(&tp->not_empty);
    free(tp->threads);
    free(tp);
}
