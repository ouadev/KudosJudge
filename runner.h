#ifndef H_RUNNER
#define H_RUNNER

#define _GNU_SOURCE
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
//#include <libcgroup.h>
#include <pwd.h>
#include <grp.h>
#include <sched.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <errno.h>
#include <memory.h>

#include <signal.h>
#include <time.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/time.h>
#include <sys/sendfile.h>

#include <sys/ipc.h>
#include <sys/shm.h>


#include "config.h"
#include "compare.h"


#include "def.h"
#include "sandbox.h"


#define PIPE_WRITETO 1
#define PIPE_READFROM 0
#define THREADS_MAX 24 //maximum number of threads, used to create the same number of pipes (template mode)
#define TEMPLATE_MAX_TX  1024 //1KB


/**
* Runner structure
* holds the basic Runner configuration.
*/
typedef struct Runner{
	int kill_on_compout; ///<flag: kill the submission before it terminate if it is judged wrong.
	int show_submission_output; //<whether to show the submission's output on the screen
	int stack_size_mb; ///< the stack to allocate for the submission watcher
	int output_size_limit_mb_default; //< maximum of the output a submission can generate
	int nbr_instances;
	
	int count_submissions;
	int count_submissions_success;

		//if template mode is used, this function should be
	// mapped in main process VM before calling jug_sandbox_template_init()
	int(*compare_output)(int,char*,int,int);	///< address to a function where the comparaison is done



}Runner;



/**
 * the structure that represents the submission. made during communication.
 */

typedef struct jug_submission
{
    char* source;
    char* input_filename;
    char* output_filename;
    int   language;
    int   time_limit;
    int   mem_limit;
    int   thread_id; //propagated to the other parts of the judge (debug only)
    //exec stuff
    int   interpreted;
    char* bin_cmd; //the command to execute
    char* bin_path; //the path to the file to be executed/interpreted
} jug_submission;


///< The pid of the template process, accessed only for read.
int template_pid;
///< pipe used to retreive the pid of the cloned process by the template.
int template_pipe_tx[2];
///< pipe used to send (void*arg) argument of clone(2) to the template process
int template_pipe_rx[2];
///< a mutex to use for concurrent access to the template process's pipe;
pthread_mutex_t template_pipe_mutex;
///< pool of pipes, each calling thread uses one. the same meaning as normal mode
int io_pipes_out[THREADS_MAX][2];  //retreive from the submission
int io_pipes_in [THREADS_MAX][2]; //feed the submission

//shared memory
char* template_shm;

//global Runner variable
Runner* g_runner;

//< number of workers.
int g_runner_number_workers;



/**
 * jug_runner_start
 * @desc called by the daemon to start the sandbox( template process, init , ...)
 *
 */

int jug_runner_start(int num_workers);

/**
 * jug_sandbox_stop
 * @desc called by the daemon before exiting to kill the template process and clean
 */

int jug_runner_stop();



/**
*jug_runner_init()
* init the Runner parameters
*/
int jug_runner_init();


/**
* jug_runner_number_workers
* return the number of workers
*/
int jug_runner_number_workers();

/**
 * jug_sandbox_init_template
 * @desc	spawns an empty process (light Virtual Memory), which is used as a template
 * 			for forked submissions (watcher).
 * 			the need for such process comes from the complications that goes with mixing pthreads and forking.
 * 			this process receives commands by signal delivery (SIGUSR1),
 * 			 then clones itself with the flag CLONE_PARENT is set, that way
 * 			 the cloned process will be attached to the calling process (signal sender) as a child.
 * 			 Doing this gives nearly the same effect as the direct clone from the main Address Space.
 * @return	0 if it succeeded
 */

int jug_runner_template_init();


/**
 * jug_sandbox_template
 * @desc the code that will be executed in the template process.
 */
int jug_runner_template(void*arg);

/**
 * jug_sandbox_template_sighandler
 * @desc handler of signals. it's used to respond to forking requests from the parent.
 * 		 the code that will be executed in the child is jug_sandbox_child function
 */

void jug_runner_template_sighandler(int sig);

/**
 * jug_sandbox_template_clone
 * @desc	request the template process to spawn (clone) a new process.
 * @param	arg pointer to data the cloned process needs. (equivalent of arg argument in clone(3))
 * @param	len length of data pointer by arg. needed because data will be transmitted over a pipe.
 * @return	the pid of the launched process, or <0 otherwise.
 */

pid_t jug_runner_template_clone(void*arg,int len);

/**
 * jug_sandbox_run_tpl
 * @desc run a submission with template mode
 * @param thread_order an integer that designates the caller thread. needed to route the data from
 * 			the template clone using a pool of pipes.
 */

int jug_runner_run(
		struct run_params* run_params_struct,
	
		char* binary_path,
		char*argv[],
		int thread_order);


/**
 * jug_sandbox_template_term
 * @desc terminates the template process
 */
void jug_runner_template_term();

/**
 * jug_sandbox_template_serialize
 * @desc	serialize the ccp in order to transmit over a pipe.
 * @return 	length of resulted serial
 * @note	struct sandbox is already in vm. most of struct run_params is values rather than pointers
 */
int jug_runner_template_serialize(void**p_serial, struct clone_child_params* ccp);

/**
 * jug_sandbox_template_unserialize
 * @desc unserialize
 */
struct clone_child_params* jug_runner_template_unserialize(void*serial);



/**
 * @desc free
 */
void jug_runner_template_freeccp(struct clone_child_params* ccp);

/**
 * jug_sandbox_judge
 * @description takes a submission as a parameter and try to judge it.
 * @note	in future we will need to add compiling layer.
 */
jug_verdict_enum jug_runner_judge(jug_submission* submission);



void jug_runner_free_submission(jug_submission* submission);


#endif