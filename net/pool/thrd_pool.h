#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifndef THREAD_POOL_H__
    #define THREAD_POOL_H__

    typedef struct thread_pool_s thread_pool_t;
    typedef void (*handler_pt)(void *arg);

    #ifdef __cplusplus
    extern "C" 
    {
    #endif

        thread_pool_t *thread_pool_create(int thread_num);

        void thread_pool_terminate(thread_pool_t *pool);

        int thread_pool_post(thread_pool_t *pool, handler_pt handler, void *arg);
        
        void thread_pool_waitdone(thread_pool_t *pool);
       
    #ifdef __cplusplus 
    {
    #endif
#endif