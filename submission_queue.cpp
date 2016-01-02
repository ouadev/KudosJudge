#include "submission_queue.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#if DEBUG_THREADING

time_t debug_get_time()
{
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    
    return (now.tv_sec);// - debug_wall_clock.tv_sec);
}

debug_thread_state debug_thread_states[MAX_DEBUG_THREAD_STATES_COUNT];
int debug_thread_states_count = 0;
char* debug_thread_state_names[THREAD_STATE_COUNT] =
{
    "THREAD_CREATED"
    ,"THREAD_STARTED"
    ,"THREAD_WORKING_NEW_WORK"
    ,"THREAD_WAITING_NO_WORK"
    ,"THREAD_WAKED_UP"};
    
void debug_push_thread_state(debug_thread_state s)
{
    /*
    //DEBUG!!
    static pthread_mutex_t mutex;
    static int first = 1;
    if(first)
    {
        pthread_mutex_init(&mutex,0);
        first = 0;
    }
    pthread_mutex_lock(&mutex);*/
    if(debug_thread_states_count < MAX_DEBUG_THREAD_STATES_COUNT)
    debug_thread_states[debug_thread_states_count++] = s;
    //pthread_mutex_unlock(&mutex);
}

#endif









void init_submission_queue(jug_submission_queue* queue)
{
    queue->head = queue->tail = 0;
    pthread_mutex_init(&queue->access_mutex,0);
    sem_init(&queue->work_semaphore,0,0);
}


void push_submission(jug_submission_queue* queue, jug_submission* submission)
{
    jug_submission_queue_entry* new_entry = (jug_submission_queue_entry*) malloc(sizeof(jug_submission_queue_entry));
    new_entry->next = 0;
    new_entry->submission = submission;
    if(queue->head==0)
    {
        queue->tail = queue->head = new_entry;
    }
    else
    {
        queue->tail->next = new_entry;
        queue->tail = new_entry;
    }

    static int i=0;
    sem_post(&queue->work_semaphore);

}

jug_submission* pop_submission(jug_submission_queue* queue)
{
    
    //NOTE : try to do this lock-free using atomic operations
    pthread_mutex_lock(&queue->access_mutex);

    if(queue->head == 0)
    {
        pthread_mutex_unlock(&queue->access_mutex);
        return 0;
    }
        jug_submission_queue_entry* entry = queue->head;
    queue->head = queue->head->next;
    
    pthread_mutex_unlock(&queue->access_mutex);

    jug_submission* result;
    result = entry->submission;
    //free entry memory
    free(entry);


    return result;
}



//NOTE : here for now
void judge_submission(jug_submission* submission)
{
//    sleep(1);
}

//this is the fucntion that worker is executing
//the worker searches for work, if not available he sleeps until otherwise
void* judge_worker_proc(void* data)
{
    submission_worker_data* worker_data = (submission_worker_data*) data;
    jug_submission_queue* work_queue = worker_data->work_queue;

#if DEBUG_THREADING
    debug_thread_state dumb = {worker_data->thread_id,THREAD_STARTED
                               ,debug_get_time()};
    debug_push_thread_state(dumb);
#endif
    
    for(;;)
    {
#if DEBUG_THREADING      
        debug_thread_state dumb = {worker_data->thread_id,
                                   THREAD_WAITING
                                   ,debug_get_time()};
        debug_push_thread_state(dumb);

        sem_wait(&work_queue->work_semaphore);

        int sem_va;
        sem_getvalue(&work_queue->work_semaphore,&sem_va);
        char *mem_leak_msg = (char*)malloc(20);
        sprintf(mem_leak_msg,"sem_va %d",sem_va);
        dumb = {worker_data->thread_id,THREAD_WAKED_UP
                ,debug_get_time(),mem_leak_msg};
        debug_push_thread_state(dumb);
#else
        sem_wait(&work_queue->work_semaphore);        
#endif
        jug_submission* submission = pop_submission(work_queue);

        if(submission!=0)
        {
#if DEBUG_THREADING
            debug_thread_state dumb =
                {worker_data->thread_id,THREAD_WORKING,
                 debug_get_time(),submission->source};
            debug_push_thread_state(dumb);            
#endif

            judge_submission(submission);
        }
    }
}
