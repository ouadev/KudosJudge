#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define DEBUG_THREADING 0
#define MAX_DEBUG_THREAD_STATES_COUNT 10000

#if DEBUG_THREADING

time_t debug_get_time();

struct debug_thread_state
{
    int thread_id;
    int state;
    time_t time;
    char* msg;
};

enum {THREAD_CREATED,THREAD_STARTED,THREAD_WORKING,THREAD_WAITING,THREAD_WAKED_UP,THREAD_STATE_COUNT};
extern char* debug_thread_state_names[THREAD_STATE_COUNT];
extern debug_thread_state debug_thread_states[MAX_DEBUG_THREAD_STATES_COUNT];
extern int debug_thread_states_count;

void debug_push_thread_state(debug_thread_state s);

#endif









struct jug_submission
{
    char* source;
    char* input_filename;
    char* output_filename;
    int   language;
    int   time_limit;
    int   mem_limit;
};

struct jug_submission_queue_entry
{
    jug_submission* submission;
    jug_submission_queue_entry* next;
};

struct jug_submission_queue
{
    jug_submission_queue_entry* head;
    jug_submission_queue_entry* tail;
    pthread_mutex_t access_mutex;
    sem_t work_semaphore;
    
};

struct submission_worker_data
{
    jug_submission_queue* work_queue;
    int thread_id;
    int state;
};


void init_submission_queue(jug_submission_queue* queue);
void push_submission(jug_submission_queue* queue, jug_submission* submission);
jug_submission* pop_submission(jug_submission_queue* queue);
void* judge_worker_proc(void* data);
