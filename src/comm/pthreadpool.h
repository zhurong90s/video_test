#ifndef PTHREADPOOL_H
#define PTHREADPOOL_H

#include <stdlib.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct _task_node{
    uint32_t id;
    uint32_t timestamp;
    void (*task)(void *);
    void * ptr;
    struct _task_node *next;
}task_node;

#define TASK_FREE_LIST_MAX 100

typedef struct _task_manager{
    uint16_t timestamp;
    uint16_t tk_count;
    uint32_t task_id;
    task_node * tk_list_h;
    uint16_t tk_free_count;
    task_node * tk_list_free;
}task_manager;

typedef struct _pthreadpool_manager{
    task_manager  *tsk_m;
    pthread_mutex_t pth_mutex;
    pthread_cond_t pth_cond;
    uint16_t pth_count;
    pthread_t pth_id[0];
}pthreadpool_manager;

pthreadpool_manager * create_threadpool(uint16_t min,uint16_t max);
int add_task_to_threadpool(pthreadpool_manager *pth_m,void (*task)(void *),void *ptr,uint16_t delay,uint32_t *task_id);
void destory_threadpool(pthreadpool_manager *pth_m);
int del_task_from_threadpool(pthreadpool_manager *pth_m,uint32_t task_id);

#endif // PTHREADPOOL_H
