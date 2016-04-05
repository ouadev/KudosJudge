
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "queue.h"


//start the queue
int queue_start(){
	int error;
	error=queue_init();
	if(error) return error;
	if(g_queue==NULL){
		debugt("queue","queue not inited correctly");
		return -99;
	}


	error=queue_launch_workers(g_queue);
	return error;

}


//init
int queue_init()
{
	g_queue=(jug_queue*)malloc(sizeof(jug_queue));
	g_queue->head = g_queue->tail = 0;
	pthread_mutex_init(&g_queue->access_mutex,0);
	sem_init(&g_queue->work_semaphore,0,0);

	char* value;
	if( (value=jug_get_config("Queue","num_worker_threads") )==NULL ){
		debugt("queue","cannot read num_worker_threads config");
		return -1;
	}
	g_num_worker_threads=atoi(value);
	if( (value=jug_get_config("Queue","queue_size_limit") )==NULL ){
		debugt("queue","cannot read queue_size_limit config");
		return -1;
	}
	g_queue_size_limit=atoi(value);
	g_workers=(pthread_t*)malloc(g_num_worker_threads*sizeof(pthread_t));
	return 0;
}

//launch_workers
int queue_launch_workers(jug_queue* queue){
	int i=0;
	jug_worker_arg* worker_arg;
	for(i=0;i< g_num_worker_threads;i++)
	{
		worker_arg=(jug_worker_arg*)malloc(sizeof(jug_worker_arg));
		worker_arg->work_queue 	= queue;
		worker_arg->thread_id 	= i;

		int ret = pthread_create(&g_workers[i],NULL,queue_worker_main,(void*)worker_arg);
		if(ret)
		{
			debugt("queue","cannot launch worker number %d",i);
			return -1;
		}
#if DEBUG_THREADING
		thread_debug_snapshot(THREAD_CREATED,"");
#endif

	}
	return 0;
}

//stop workers
int queue_stop_workers(){
	int i;
	for(i=0;i<g_num_worker_threads;i++){
		pthread_cancel(g_workers[i]);
		pthread_join(g_workers[i],NULL);
	}
	return 0;
}
//push
void queue_push_connection (jug_connection* connection)
{
	jug_queue_entry* new_entry = (jug_queue_entry*) malloc(sizeof(jug_queue_entry));
	new_entry->next = 0;
	new_entry->connection = connection;
	if(g_queue->head==0)
	{
		g_queue->tail = g_queue->head = new_entry;
	}
	else
	{
		g_queue->tail->next = new_entry;
		g_queue->tail = new_entry;
	}

	static int i=0;
	sem_post(&g_queue->work_semaphore);

}



jug_connection* queue_pop_connection()
{

	//NOTE : try to do this lock-free using atomic operations
	pthread_mutex_lock(&g_queue->access_mutex);

	if(g_queue->head == 0)
	{
		pthread_mutex_unlock(&g_queue->access_mutex);
		return 0;
	}
	jug_queue_entry* entry = g_queue->head;
	g_queue->head = g_queue->head->next;

	pthread_mutex_unlock(&g_queue->access_mutex);

	jug_connection* result;
	result = entry->connection;
	//free entry memory
	free(entry);

	return result;
}

int factor(int x){
	if(x==0) return 1;
	return x*factor(x-1);
}

//NOTE : here for now
void queue_worker_serv(jug_connection* connection)
{
	int read_size, read_size_acc=0;
	int write_size=0;
	int worker_id;
	int_request request;
	int_response response;
	jug_submission submission;
	jug_verdict_enum verdict;
	//use interface.h/proroco.h to generate a valid submission in case of a judging request
	//compile the submission code
	//run the resulting binary inside the sandbox.
	//send the client the response object
	//close connection
	sleep(0.5);
	worker_id=queue_worker_id();
	//Receive a message from client
	int client_sock=connection->client_socket;


//
//		if(jug_int_recv(client_sock,&request)!=0){
//			debugt("queue","error while receiving, worker %d",queue_worker_id());
//			return;
//		}

	//make a submisison object (in the real scenario, we will need more elaborated data)
	submission.input_filename=(char*)malloc(110);
	submission.output_filename=(char*)malloc(110);
	submission.source=(char*)malloc(50);

	char* jug_root=getenv("JUG_ROOT");


	sprintf(submission.input_filename,"%s/tests/problems/twins/twins.in",jug_root);
	sprintf(submission.output_filename,"%s/tests/problems/twins/twins.out",jug_root);
	sprintf(submission.source,"/opt/twins");
	submission.thread_id=queue_worker_id();

	// SEND TO THE SANDBOX
	verdict=jug_sandbox_judge(&submission);
//	verdict=VERDICT_ACCEPTED;

	free(submission.input_filename);
	free(submission.output_filename);
	free(submission.source);


	//return the verdict
	response.verdict=(int)verdict;
	sprintf(response.verdict_s,"%s (%d)",protocol_verdict_to_string(verdict),worker_id);



	debugt("queue","worker: %d, verdict:%s",queue_worker_id(),protocol_verdict_to_string(verdict));
	write_size=write(client_sock , &response, sizeof(int_response));
	//jug_int_write(client_sock, &response);
	close(client_sock);
}

//get the id of the current worker
int queue_worker_id(){
	pthread_t current=pthread_self();
	int i;
	for(i=0;i<g_num_worker_threads;i++){
		if( pthread_equal(g_workers[i],current))
			return i;
	}
	//no doubt the main process then
	return -1; //equivalent of the main thread, well i think
}



//this is the fucntion that worker is executing
//the worker searches for work, if not available he sleeps until otherwise
void* queue_worker_main(void* data)
{
	jug_worker_arg* worker_data = (jug_worker_arg*) data;
	jug_queue* work_queue = worker_data->work_queue;
	int worker_id=queue_worker_id();

#if DEBUG_THREADING
//	debug_thread_state dumb = {worker_data->thread_id,THREAD_STARTED
//			,thread_debug_get_time()};
//	debug_push_thread_state(dumb);

	thread_debug_snapshot(THREAD_STARTED,"");

#endif

	for(;;)
	{
#if DEBUG_THREADING      
//		debug_thread_state dumb = {worker_data->thread_id,
//				THREAD_WAITING
//				,thread_debug_get_time()};
//		debug_push_thread_state(dumb);
		thread_debug_snapshot(THREAD_WAITING,"");
		sem_wait(&work_queue->work_semaphore);

		int sem_va;
		sem_getvalue(&work_queue->work_semaphore,&sem_va);

		thread_debug_snapshot(THREAD_WAKED_UP,"sem_va %d",sem_va);

#else
		sem_wait(&work_queue->work_semaphore);
#endif
		jug_connection* connection= queue_pop_connection(work_queue);

		if(connection!=0)
		{
#if DEBUG_THREADING
			thread_debug_snapshot(THREAD_WORKING,"");
#endif

			queue_worker_serv(connection);
#if DEBUG_THREADING
			thread_debug_snapshot(THREAD_FINISH,"");
#endif

#if DEBUG_THREADING
			//print thread_debug
			thread_debug_print();
#endif

		}
	}
}


#if DEBUG_THREADING
/**
 * globals
 */
thread_debug_state global_thread_debug_states[MAX_DEBUG_THREAD_STATES_COUNT];
int global_thread_debug_state_count=0;
/////snapshot

void thread_debug_snapshot(int state, char* format, ...){
	thread_debug_state dumb;
	dumb.thread_id=queue_worker_id();
	dumb.state=state;
	dumb.time=thread_debug_get_time();
	//
	char* dbg_msg=(char*)malloc(strlen(format)+200);
	va_list args;
	va_start(args,format);
	vsprintf(dbg_msg,format,args);
	va_end(args);
	//
	strcpy(dumb.msg,dbg_msg);
	thread_debug_push(dumb);
	free(dbg_msg);

}


/////get time

time_t thread_debug_get_time()
{
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);

	return (now.tv_sec);// - debug_wall_clock.tv_sec);
}




//push
void thread_debug_push(thread_debug_state s)
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
	if(global_thread_debug_state_count < MAX_DEBUG_THREAD_STATES_COUNT)
		global_thread_debug_states[global_thread_debug_state_count++] = s;
	//pthread_mutex_unlock(&mutex);
}
///////////////
void  thread_debug_print(){

	int where =0, i;
	debugt("thread-debug","num_saved_states = %d", global_thread_debug_state_count);
	char* state_strs[THREAD_STATE_COUNT] =
	{
			"THREAD_CREATED"
			,"THREAD_STARTED"
			,"THREAD_WORKING_NEW_WORK"
			,"THREAD_WAITING_NO_WORK"
			,"THREAD_WAKED_UP"
			,"THREAD_FINISH_WORK_DONE"
	};
	for(i=where;i<global_thread_debug_state_count;i++)
	{
		struct tm lt = *localtime(&global_thread_debug_states[i].time);

		debugt("thread-debug","%d:%d:%d\tID_%d\t%s\t%s",lt.tm_hour,lt.tm_min,lt.tm_sec,
				global_thread_debug_states[i].thread_id,
				state_strs[global_thread_debug_states[i].state],
				global_thread_debug_states[i].msg);
	}
	///reset the array
	global_thread_debug_state_count=0;
}

#endif
