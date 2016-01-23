/**
 * queue.h
 * Connection queue and threading stuff.
 * implementation of a queue holding the list of connection handles, these handles are to be
 * retrieved by the Worker threads and processed.
 */


#ifndef H_JUG_QUEUE
#define H_JUG_QUEUE
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "config.h"
#include "interface.h"
#include "protocol.h"

#define DEBUG_THREADING 0
#define MAX_DEBUG_THREAD_STATES_COUNT 10000


/**
 * Qeue entry
 */
typedef struct jug_queue_entry
{
	jug_connection* connection;
	struct jug_queue_entry* next;
} jug_queue_entry;

/**
 * the Queue main structure
 */
typedef struct jug_queue
{
	jug_queue_entry* head;
	jug_queue_entry* tail;
	pthread_mutex_t access_mutex;
	sem_t work_semaphore;

} jug_queue;

/**
 * the data we pass to the worker thread (shouldn't be here)
 */
typedef struct jug_worker_arg
{
	jug_queue* work_queue;
	int thread_id;
	int state;
} jug_worker_arg;

/**
 * queue globals
 * read from the config file.
 */
int g_num_worker_threads;
int g_queue_size_limit;
pthread_t* g_workers;
jug_queue* g_queue;



/**
 * @desc call by the daemon when starting
 */
int queue_start();
/**
 */
int queue_init();

/**
 * @desc launch the workers
 */
int queue_launch_workers(jug_queue* queue);
/**
 * shutdown workers
 */
int queue_stop_workers();
/**
 *
 */
void queue_push_connection(jug_connection* connection);
/**
 *
 */
jug_connection* queue_pop_connection();
/**
 *
 */
void* queue_worker_main(void* data);
/**
 * the function the worker use to handle the connection affected to him.
 */
void queue_worker_serv(jug_connection* connection);
/**
 * queue_worker_id
 * @desc get the id of the current thread executing this part
 */
int queue_worker_id();
/**
 * workers debugging stuff
 */
#if DEBUG_THREADING
/**
 * state of the thread
 */
typedef struct debug_thread_state
{
	int thread_id;
	int state;
	time_t time;
	char* msg;
}debug_thread_state;
/**
 * globals
 */
enum {THREAD_CREATED,THREAD_STARTED,THREAD_WORKING,THREAD_WAITING,THREAD_WAKED_UP,THREAD_STATE_COUNT};
extern char* debug_thread_state_names[THREAD_STATE_COUNT];
extern debug_thread_state debug_thread_states[MAX_DEBUG_THREAD_STATES_COUNT];
extern int debug_thread_states_count;
/**
 * get time
 */
time_t thread_debug_get_time();


/**
 * @desc push a state
 */
void thread_debug_push(debug_thread_state s);

/**
 * @desc print the result of thread debugging
 */

void  thread_debug_print();


#endif

#endif
