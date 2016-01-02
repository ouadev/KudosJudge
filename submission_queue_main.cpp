#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "submission_queue.h"

enum {LANG_CPP,LANG_C,LANG_JAVA};

#define NUM_SUBMISSIONS 200
#define NUM_WORKER_THREADS 16

int main()
{
    jug_submission submissions[NUM_SUBMISSIONS];
    char sub_names[NUM_SUBMISSIONS][128];

    for(int i=0;i<NUM_SUBMISSIONS;i++)

    {
        sprintf(sub_names[i],"source %d",i);
        submissions[i].source = sub_names[i];
    }

    jug_submission_queue queue;
    init_submission_queue(&queue);
    
    pthread_t workers[NUM_WORKER_THREADS];
    submission_worker_data wd[NUM_WORKER_THREADS];
    
    for(int i=0;i<NUM_WORKER_THREADS;i++)
    {
        wd[i].work_queue = &queue;
        wd[i].thread_id = i;

        int ret = pthread_create(&workers[i],NULL,judge_worker_proc,(void*)&wd[i]);
#if DEBUG_THREADING
        debug_thread_state dumb = {i,THREAD_CREATED,debug_get_time()};
        debug_push_thread_state(dumb);
#endif
        if(ret)
        {
            printf("Error - pthread_create() return code: %d\n",ret);
        }
    }
  
    for(int i=0;i<NUM_SUBMISSIONS;i++)
    {
        push_submission(&queue,&submissions[i]);
    }

    printf("all submissions pushed\n");
    fflush(stdout);
        
    //wait for all threads to start
    printf("sleep wait for threads to start\n");
    sleep(10);
    printf("I'm awake \n");   

#if DEBUG_THREADING
    printf("num_saved_states = %d\n", debug_thread_states_count);
#endif
    
    int where = 0;
    for(;;)// main loop
    {
#if DEBUG_THREADING
        for(int i=where;i<debug_thread_states_count;i++)
        {
            tm lt = *localtime(&debug_thread_states[i].time);
        
            printf("%d:%d:%d - ID_%d - %s - %s\n",lt.tm_hour,lt.tm_min,lt.tm_sec,
                   debug_thread_states[i].thread_id,
                   debug_thread_state_names[debug_thread_states[i].state],
                   debug_thread_states[i].msg);
        }
        where = debug_thread_states_count;
#endif
        
    }
    
    return 0;
}
