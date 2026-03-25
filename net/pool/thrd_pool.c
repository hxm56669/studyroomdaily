#include "thrd_pool.h"
#include "spinlock.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct spinlock spinlock_t;

/*
typedef struct taskex_s 
{
    void* pnext;
    handler_pt func;
    void* arg;
}taskex_t;
*/


typedef struct task_s
{
    void* next;//链接下一个任务
    handler_pt func;
    void* arg; //上下文，一般维护在堆上
} task_t;

typedef struct task_queue_s
{
    void* head;
    void** tail;
    int block;
    spinlock_t lock;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} task_queue_t;

struct thread_pool_s
{
    task_queue_t* task_queue;
    atomic_int quit;
    int thread_count;
    pthread_t* threads;
} ;
//对称思想：创建和销毁是成对出现的
static task_queue_t* //只能自己源文件使用，不能被外部调用
__taskqueue_create()
{

}

static void
__nonblock(task_queue_t* queue)
{

}

static inline void *
__pop_task(task_queue_t* queue)
{

}

static inline void*
__get_task(task_queue_t* queue)
{

}

static void 
__taskqueue_destroy(task_queue_t* queue)
{
    task_t* task;
    while((task = __pop_task(queue)))
    {
        free(task);
    }
    spinlock_destroy(&queue->lock);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    free(queue);
}
