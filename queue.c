
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
		debug_thread_state dumb = {i,THREAD_CREATED,debug_get_time()};
		debug_push_thread_state(dumb);
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
	sprintf(submission.source,"/bin/ls");
	debugt("judge","in:%s, out:%s",submission.input_filename,submission.output_filename );
	submission.thread_id=queue_worker_id();
	verdict=jug_sandbox_judge(&submission, global_sandbox);
	//verdict=VERDICT_ACCEPTED;

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
	debugt("queue","queue_worker_id failed ");
	return -1; //equivalent of the main thread, well i think
}



//this is the fucntion that worker is executing
//the worker searches for work, if not available he sleeps until otherwise
void* queue_worker_main(void* data)
{
	jug_worker_arg* worker_data = (jug_worker_arg*) data;
	jug_queue* work_queue = worker_data->work_queue;

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
		jug_connection* connection = queue_pop_connection(work_queue);

		if(connection!=0)
		{
#if DEBUG_THREADING
			debug_thread_state dumb =
			{worker_data->thread_id,THREAD_WORKING,
					debug_get_time(),submission->source};
			debug_push_thread_state(dumb);
#endif

			queue_worker_serv(connection);
			return NULL;
		}
	}
}




#if DEBUG_THREADING

time_t thread_debug_get_time()
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
		,"THREAD_WAKED_UP"
};

//push
void thread_debug_push(debug_thread_state s)
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

///////////////
void  thread_debug_print(){

	int where =0;
	debugt("thread-debug","num_saved_states = %d\n", debug_thread_states_count);
	for(int i=where;i<debug_thread_states_count;i++)
	{
		tm lt = *localtime(&debug_thread_states[i].time);

		debugt("thread-debug","%d:%d:%d - ID_%d - %s - %s\n",lt.tm_hour,lt.tm_min,lt.tm_sec,
				debug_thread_states[i].thread_id,
				debug_thread_state_names[debug_thread_states[i].state],
				debug_thread_states[i].msg);
	}
	where = debug_thread_states_count;
}

#endif
