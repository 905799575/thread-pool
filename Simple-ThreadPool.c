#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

typedef struct NWORKER
{
    pthread_t thread_id;
    int terminated;
    struct NWORKER *next;
    struct NWORKER *prev;
    struct MANAGER *thread_pool;
}Worker;


typedef struct NJOB
{
    void (*func)(struct NJOB*);
    void *user_data;
    struct NJOB* next;
    struct NJOB* prev;
}Job;

typedef struct MANAGER
{
    Job *njobs;
    Worker* workers;
    pthread_mutex_t mutex;
    pthread_cond_t cond_t;
}ThreadPool;

#define ADD_LIST(item, list)            \
do                                      \
{                                       \
    item->next = list->next;            \
    item->prev = list;                  \
    if(list->next)                      \
        list->next->prev = item;        \
    list->next = item;                  \
}while(0)           


// #define REVOME_LIST(item, list)         \
// do                                      \
// {                                       \
//     if(item->next)                      \
//         item->next->prev = list;        \
//     if(list)                            \
//         list->next = item->next;        \
//     if(list == item)                    \
//         list = item->next;              \
//     item->next = item->prev = NULL;     \
// }while(0)


#define REVOME_LIST(item, list)         \
do                                      \
{                                       \
    if(item->next)                      \
        item->next->prev = item->prev;  \
    if(item->prev)                      \
        item->prev->next = item->next;  \
    item->next = item->prev = NULL;     \
}while(0)


void* exe_task(void *arg)
{
    Worker* workers = (Worker*)arg;
    while (1)
    {
        pthread_mutex_lock(&workers->thread_pool->mutex);
        while(workers->thread_pool->njobs->next == NULL) 
        {
            if(workers->terminated == 1)
                break;
            pthread_cond_wait(&workers->thread_pool->cond_t, &workers->thread_pool->mutex); //等待工作队列的任务
        }
        if(workers->terminated == 1)
        {
            pthread_mutex_unlock(&workers->thread_pool->mutex);
            break;
        }
        //从工作队列的队头中取任务
        Job* job = workers->thread_pool->njobs->next;
        if(job != NULL)
        {
            REVOME_LIST(job, workers->thread_pool->njobs);
        }
        pthread_mutex_unlock(&workers->thread_pool->mutex);
        if(job == NULL)
            continue;
        job->func(job); //执行任务
    }
    free(workers);
    pthread_exit(NULL);
}

int ThreadPool_Create(ThreadPool* pool, int nthreads)
{
    if(pool == NULL)
        return -1;
    if(nthreads < 1) //默认创建一个线程
        nthreads = 1;
    memset(pool, 0, sizeof(ThreadPool));
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->mutex, &mutex, sizeof(mutex));
    pthread_cond_t cont_x = PTHREAD_COND_INITIALIZER;
    memcpy(&pool->cond_t, &cont_x, sizeof(cont_x));
    pool->njobs = (Job*)malloc(sizeof(Job)); //Job头节点
    pool->njobs->next = pool->njobs->prev = NULL;
    if(pool->njobs == NULL)
    {
        perror("malloc error");
        return -2;
    }
    memset(pool->njobs, 0, sizeof(Job));
    pool->workers = (Worker*)malloc(sizeof(Worker)); //Workers头结点
    pool->workers->next = pool->workers->prev = NULL;
    pool->workers->thread_pool = pool;
    if(pool->workers == NULL)
    {
        perror("malloc error");
        return -2;
    }    
    memset(pool->workers, 0, sizeof(Worker));
    int i = 0;
    for(; i < nthreads; i++)
    {
        Worker* worker_n = (Worker*)malloc(sizeof(Worker));
        if(worker_n == NULL)
        {
            perror("malloc error");
            return -2;
        }
        memset(worker_n, 0, sizeof(Worker));
        worker_n->thread_pool = pool;
        int ret = pthread_create(&worker_n->thread_id, NULL, exe_task, (void*)worker_n);
        if(ret < 0)
        {
            perror("pthread_create error");
            free(worker_n);
            return -3;
        }
        ADD_LIST(worker_n, pool->workers); //头插入工作队列
    }
}   

int PushJob_to_ThreadPool(ThreadPool *pool, Job* job)
{
    pthread_mutex_lock(&pool->mutex);
    ADD_LIST(job, pool->njobs);
    pthread_mutex_unlock(&pool->mutex);
    pthread_cond_signal(&pool->cond_t);
    return 0;
}

int ThreadPool_Destroy(ThreadPool *pool)
{
    //终止所有线程，并且回收所有线程
    pthread_mutex_lock(&pool->mutex);
    Worker *cur_worker = pool->workers->next;
    for(; cur_worker != NULL; cur_worker = cur_worker->next)
    {
        cur_worker->terminated = 1;
    }
    free(pool->njobs);
    pool->njobs = NULL;
    free(pool->workers);
    pool->workers = NULL;
    pthread_mutex_unlock(&pool->mutex);
    pthread_cond_broadcast(&pool->cond_t); //唤醒所有线程
}

void my_func(Job* jobs)
{
    int index = *((int*)jobs->user_data);
    printf("thread_id:%lu, %d\n", pthread_self(), index);
    free(jobs->user_data);
    free(jobs); //任务执行完毕,free
}

int main()
{
    ThreadPool pool;
    ThreadPool_Create(&pool, 5);
    int i = 0;
    for(; i < 40; i++)
    {
        Job* jobs = (Job*)malloc(sizeof(Job));
        memset(jobs, 0, sizeof(Job));
        jobs->func = my_func;
        jobs->user_data = malloc(sizeof(int));
        *((int *)(jobs->user_data)) = i;
        PushJob_to_ThreadPool(&pool, jobs);
    }
    getchar();
	printf("\n");
    ThreadPool_Destroy(&pool);
    return 0;
}