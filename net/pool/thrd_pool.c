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
//只能自己源文件使用，不能被外部调用
static task_queue_t*   //资源的创建：回滚式编程，先假设每个行为都能成功   业务逻辑的书写：防御式编程
__taskqueue_create()
{
    int ret;
    task_queue_t* queue = (task_queue_t*)malloc(sizeof(task_queue_t)); //sizeof 是操作符，编译期行为
    if(queue)
    {
        ret = pthread_mutex_init(&queue->mutex, NULL);
        if(ret == 0)
        {
            ret = pthread_cond_init(&queue->cond, NULL);
            if(ret == 0)
            {
                spinlock_init(&queue->lock);//用户态行为，肯定会成功
                queue->head = NULL;
                queue->tail = &queue->head;
                queue->block = 1;
                return queue;
            }
            pthread_mutex_destroy(&queue->mutex);
        }
        
        free(queue);
    }
    return NULL;
    
}

static void
__nonblock(task_queue_t* queue)
{
    pthread_mutex_lock(&queue->mutex);
    queue->block = 0;
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->cond);
}

static inline void
__add_task(task_queue_t* queue, task_t* task)
{
    void **link = (void**)task;  //二维指针的作用：不限定任务的类型，只要任务的结构起始内存是一个用于连接下一个任务的指针
    *link = NULL; //把新加任务的next指针赋值为NULL

    spinlock_lock(&queue->lock);
    *queue->tail = task;  //将当前队列最后一个节点的next指针，指向的新加任务
                          /*先取成员，再解引用: *queue->tail = queue->tail->next  ;
                        c语言中void*，可以用来被任意类型指针指向*，所以这里void* = void**（隐式类型转换）
                        又因为link和task指向的对象的初始地址相同，只是解释方式不同，所以不管这里用
                        task还是link都是对的 */
     queue->tail = link;//更新当前队列的tail指针，指向新任务的next指针
     spinlock_unlock(&queue->lock);
     pthread_cond_signal(&queue->cond);
}
static inline void *
__pop_task(task_queue_t* queue)
{
    spinlock_lock(&queue->lock);
    if (queue->head ==NULL)
    {
        spinlock_unlock(&queue->lock);
        return NULL;
    }
    task_t* task = (task_t*)queue->head;
    void** link = (void**)task; //link的值是task的next指针的地址
    queue->head = *link;        //*link是task的next指针的值
    if(queue->head == NULL)
    {
        queue->tail = &queue->head;
    }
    spinlock_unlock(&queue->lock);
    return task;
}

static inline void*
__get_task(task_queue_t* queue)
{
    task_t* task;
    //虚假唤醒：当生产者线程唤醒消费者线程时，消费者线程可能没有任务可执行，需要继续等待眠
    while((task = __pop_task(queue)) == NULL)
    {
       pthread_mutex_lock(&queue->mutex);
       if(queue->block == 0)
       {
         pthread_mutex_unlock(&queue->mutex);   
         return NULL;   
       }
       //先解锁，再休眠，等待生产者线程条件变量唤醒，再加锁
       pthread_cond_wait(&queue->cond, &queue->mutex);
       pthread_mutex_unlock(&queue->mutex);
    }
    
    spinlock_unlock(&queue->lock);
    return task;
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
