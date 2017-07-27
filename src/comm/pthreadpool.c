#include "pthreadpool.h"

static inline uint16_t alloc_task_manager(task_manager **ptsk_m)
{
    if(ptsk_m == NULL) return -1;

    *ptsk_m = (task_manager *)malloc(sizeof(task_manager));
    if((*ptsk_m) == NULL) return -1;
    memset(*ptsk_m,0,sizeof(task_manager));

    (*ptsk_m)->tk_list_h = (task_node *)malloc(sizeof(task_node));
    if((*ptsk_m)->tk_list_h == NULL){
        free(*ptsk_m);
        return -1;
    }
    memset((*ptsk_m)->tk_list_h,0,sizeof(task_node));

    (*ptsk_m)->tk_list_free = (task_node *)malloc(sizeof(task_node));
    if((*ptsk_m)->tk_list_free == NULL){
        free((*ptsk_m)->tk_list_h);
        free((*ptsk_m)->tk_list_free);
        return -1;
    }

    return 0;
}

static inline void free_task_manager(task_manager *tsk_m)
{
    if(tsk_m == NULL) return ;

    task_node *tsk_n,*tsk_node = tsk_m->tk_list_h;
    do{
        tsk_n = tsk_node->next;
        free(tsk_node);
        tsk_node = tsk_n;
    }while(tsk_node != NULL);

    tsk_node = tsk_m->tk_list_free;
    do{
        tsk_n = tsk_node->next;
        free(tsk_node);
        tsk_node = tsk_n;
    }while(tsk_node != NULL);

    free(tsk_m);
}

static inline uint16_t has_task_ready(task_manager *tsk_m,void (**task)(void *),void **ptr)
{
    if((tsk_m == NULL)||(task == NULL)||(ptr == NULL)) return -1;

    task_node *tsk_node = tsk_m->tk_list_h->next;
    if((tsk_node == NULL)||(tsk_node->timestamp > tsk_m->timestamp)) return -1;

    tsk_m->tk_list_h->next = tsk_node->next;
    *task = tsk_node->task;
    *ptr = tsk_node->ptr;
    tsk_m->tk_count --;

    if(tsk_m->tk_free_count > TASK_FREE_LIST_MAX){
        free(tsk_node);
        return 0;
    }

    tsk_node->next = tsk_m->tk_list_free->next;
    tsk_m->tk_list_free->next = tsk_node;
    tsk_m->tk_free_count ++;
    return 0;
}

static inline uint16_t add_task_to_manager(task_manager *tsk_m, void (*task)(void *),void *ptr,uint16_t delay,uint32_t *task_id)
{
    if((tsk_m == NULL)||(task == NULL)) return -1;

    task_node *tsk_n = tsk_m->tk_list_free->next;
    if(tsk_n == NULL){
        tsk_n = (task_node *)malloc(sizeof(task_node));
        if(tsk_n == NULL) return -1;
    }else{
        tsk_m->tk_list_free->next = tsk_n->next;
        tsk_m->tk_free_count--;
    }

    tsk_n->timestamp = tsk_m->timestamp + delay;
    tsk_n->task = task;
    tsk_n->ptr = ptr;
    tsk_n->id = tsk_m->task_id;
    if(task_id != NULL) *task_id = tsk_m->task_id;

    task_node *tsk_node = tsk_m->tk_list_h;
    while((tsk_node->next != NULL)&&(tsk_node->next->timestamp <= tsk_n->timestamp)){
        tsk_node = tsk_node->next;
    }
    tsk_n->next = tsk_node->next;
    tsk_node->next = tsk_n;
    tsk_m->tk_count++;
    tsk_m->task_id++;

    return 0;
}
static inline void del_task_from_manager(task_manager *tsk_m,uint32_t id)
{
    if(tsk_m == NULL) return ;


    task_node *tsk_n,*tsk = tsk_m->tk_list_h;
    while((tsk->next != NULL)&&(tsk->next->id != id)){
        tsk = tsk->next;
    }

    if(tsk->next == NULL) return;
    tsk_n = tsk->next;
    tsk->next = tsk->next->next;

    if(tsk_m->tk_free_count > TASK_FREE_LIST_MAX){
        free(tsk_n);
        return;
    }

    tsk_n->next = tsk_m->tk_list_free->next;
    tsk_m->tk_list_free->next = tsk_n;
    tsk_m->tk_free_count ++;

    return;
}

static inline void update_task_manager_timstamp(task_manager *tsk_m)
{
    if((tsk_m->timestamp + 1) != 0){
        tsk_m->timestamp++;
        return;
    }

    task_node *tsk = tsk_m->tk_list_h->next;
    while(tsk != NULL){
        if(tsk->timestamp > tsk_m->timestamp){
            tsk->timestamp -= tsk_m->timestamp;
        }else{
            tsk->timestamp = 0;
        }
    }

    tsk_m->timestamp = 0;
    return;
}

static void pth_exit(void *ptr)
{
    pthreadpool_manager *pth_m = (pthreadpool_manager *) ptr;
    //pthread_t self = pthread_self();
    pth_m->pth_count --;
    pthread_cond_signal(&pth_m->pth_cond);
    pthread_mutex_unlock(&pth_m->pth_mutex);
}

static void *start_run(void *ptr)
{
    //ptr == NULL
    pthreadpool_manager *pth_m = (pthreadpool_manager *) ptr;
    void (*task)(void *) = NULL;
    void * tsk_ptr = NULL;
    uint16_t result;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
    pthread_mutex_lock(&pth_m->pth_mutex);
    pthread_cleanup_push(pth_exit,ptr);
    pth_m->pth_count++;

    do{
        pthread_cond_wait(&pth_m->pth_cond,&pth_m->pth_mutex);

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
        pthread_testcancel();
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);

        result = has_task_ready(pth_m->tsk_m,&task,&tsk_ptr);

        if(result != 0) continue;

        pthread_cond_signal(&pth_m->pth_cond);
        pthread_mutex_unlock(&pth_m->pth_mutex);

        task(tsk_ptr);

        pthread_mutex_lock(&pth_m->pth_mutex);

    }while(1);

    pthread_cleanup_pop(1);
    pthread_mutex_unlock(&pth_m->pth_mutex);
    return NULL;
}

static void *start_run_check(void *ptr)
{
    //ptr == NULL
    pthreadpool_manager *pth_m = (pthreadpool_manager *) ptr;
    void (*task)(void *) = NULL;
    void * tsk_ptr = NULL;
    //struct timespec tsp = {.tv_sec = 1,.tv_nsec = 0};
    struct timespec tsp;
    int16_t result;
    tsp.tv_sec = 0;
    tsp.tv_nsec = 500*1000*1000;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
    pthread_mutex_lock(&pth_m->pth_mutex);
    pthread_cleanup_push(pth_exit,ptr);
    pth_m->pth_count++;

    do{
        pthread_cond_timedwait(&pth_m->pth_cond,&pth_m->pth_mutex,&tsp);

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
        pthread_testcancel();
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);

        update_task_manager_timstamp(pth_m->tsk_m);
        result = has_task_ready(pth_m->tsk_m,&task,&tsk_ptr);

        if(result != 0) continue;

        pthread_cond_signal(&pth_m->pth_cond);
        pthread_mutex_unlock(&pth_m->pth_mutex);

        task(tsk_ptr);

        pthread_mutex_lock(&pth_m->pth_mutex);

    }while(1);
    pthread_cleanup_pop(1);
    pthread_mutex_unlock(&pth_m->pth_mutex);
    return NULL;
}

pthreadpool_manager * create_threadpool(uint16_t min,uint16_t max)
{
    int result = 0;
    pthreadpool_manager * pth_m = NULL;

    if((min == 0)||(min > max)) goto parm_malloc_failed;

    pth_m = (pthreadpool_manager *) malloc(sizeof(pthreadpool_manager) + sizeof(pthread_t)*max);
    if(pth_m == NULL) goto parm_malloc_failed;
    memset(pth_m,0,sizeof(pthreadpool_manager) + sizeof(pthread_t)*max);

    if(alloc_task_manager(&(pth_m->tsk_m)) != 0){
        perror(strerror(result));
        goto alloc_tskm_failed;
    }

    result = pthread_mutex_init(&pth_m->pth_mutex,NULL);
    if(result < 0){
        perror(strerror(result));
        goto init_mutex_failed;
    }
    result = pthread_mutex_lock(&pth_m->pth_mutex);
    if(result < 0){
        perror(strerror(result));
        goto mutex_lock_init_cond_failed;
    }

    result = pthread_cond_init(&pth_m->pth_cond,NULL);
    if(result < 0){
        perror(strerror(result));
        goto mutex_lock_init_cond_failed;
    }

    result = pthread_create(&pth_m->pth_id[0],NULL,start_run_check,(void *)pth_m);
    if(result < 0){
        perror(strerror(result));
        goto create_first_thread_failed;
    }

    int i = 1;
    int j = 1;
    do{
        result = pthread_create(&pth_m->pth_id[j],NULL,start_run,(void *)pth_m);
        if(result < 0){
            perror(strerror(result));
            i++;
            continue;
        }
        i++;
        j++;
    }while(((min > j)&&(min > i))||((min > j)&&(max > i)));


    result = pthread_mutex_unlock(&pth_m->pth_mutex);
    if(result < 0) {
        perror(strerror(result));
        goto unlock_mutex_failed;
    }

    do{
        usleep(1000);
        pthread_mutex_lock(&pth_m->pth_mutex);
        i = pth_m->pth_count;
        pthread_mutex_unlock(&pth_m->pth_mutex);
    }while(i != j);

    return pth_m;

unlock_mutex_failed:
    for(i=0;i<j;i++){
        pthread_cancel(pth_m->pth_id[i]);
    }
create_first_thread_failed:
    result = pthread_cond_destroy(&pth_m->pth_cond);
    if(result < 0) perror(strerror(result));
    result = pthread_mutex_unlock(&pth_m->pth_mutex);
    if(result < 0) perror(strerror(result));

mutex_lock_init_cond_failed:
    result = pthread_mutex_destroy(&pth_m->pth_mutex);
    if(result < 0) perror(strerror(result));
init_mutex_failed:
    free_task_manager(pth_m->tsk_m);
alloc_tskm_failed:
    free(pth_m);
parm_malloc_failed:
    return NULL;
}

int add_task_to_threadpool(pthreadpool_manager *pth_m,void (*task)(void *),void *ptr,uint16_t delay,uint32_t *task_id)
{
    int result;

    if((pth_m == NULL)||(task == NULL)) return -1;

    result = pthread_mutex_lock(&pth_m->pth_mutex);
    if(result != 0) return result;

    result = add_task_to_manager(pth_m->tsk_m,task,ptr,delay,task_id);
    pthread_cond_signal(&pth_m->pth_cond);

    int result_;
    do{
        result_ = pthread_mutex_unlock(&pth_m->pth_mutex);
    }while(result_ != 0);

    return result;
}


void destory_threadpool(pthreadpool_manager *pth_m)
{
    uint16_t i;
    pthread_mutex_lock(&pth_m->pth_mutex);
    for(i = 0;i < (pth_m->pth_count);i++){
        pthread_cancel(pth_m->pth_id[i]);
        pthread_cond_signal(&pth_m->pth_cond);
    }
    pthread_mutex_unlock(&pth_m->pth_mutex);

    do{
        usleep(5000);
        pthread_mutex_lock(&pth_m->pth_mutex);
        i = pth_m->pth_count;
        pthread_cond_signal(&pth_m->pth_cond);
        pthread_mutex_unlock(&pth_m->pth_mutex);
    }while(i != 0);

    pthread_mutex_lock(&pth_m->pth_mutex);
    free_task_manager(pth_m->tsk_m);
    pthread_mutex_unlock(&pth_m->pth_mutex);

    pthread_cond_destroy(&pth_m->pth_cond);
    pthread_mutex_unlock(&pth_m->pth_mutex);

    free(pth_m);
}

int del_task_from_threadpool(pthreadpool_manager *pth_m,uint32_t task_id)
{
    int result;
    result = pthread_mutex_lock(&pth_m->pth_mutex);
    if(result != 0) return result;

    del_task_from_manager(pth_m->tsk_m,task_id);

    do{
        pthread_mutex_unlock(&pth_m->pth_mutex);
    }while(result != 0);

    return 0;
}

void print_pth_pool_info(pthreadpool_manager *pth_m)
{
    pthread_mutex_lock(&pth_m->pth_mutex);
    printf("\npthread NUM:%d\n",pth_m->pth_count);
    printf("task count:%u\n",pth_m->tsk_m->tk_count);
    printf("free task list count:%u\n\n",pth_m->tsk_m->tk_free_count);
    pthread_mutex_unlock(&pth_m->pth_mutex);
}

/*
void hello(void *ptr)
{
    printf("%s",(char *)ptr);
    printf("Hello:%lu\n\n",pthread_self());
    sleep(1);

}

int main(void)
{

    char data[]="this is a test message!\n";
    printf("main\n");
    pthreadpool_manager *pthpool = create_threadpool(10,11);

    print_pth_pool_info(pthpool);
    for(int i=0;i<100;i++){
        add_task_to_threadpool(pthpool,hello,data,0,NULL);
    }

    print_pth_pool_info(pthpool);
    sleep(3);
    print_pth_pool_info(pthpool);
    sleep(3);
    for(int i=0;i<90;i++){
        add_task_to_threadpool(pthpool,hello,data,0,NULL);
    }
    print_pth_pool_info(pthpool);
    destory_threadpool(pthpool);

    printf("main exit\n");
    return 0;
}

*/
