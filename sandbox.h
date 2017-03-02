/*
 * Author : Ouadim Jamal ouadev@yandex.com
 * Jug Sandbox
 * It does its best to secure the execution of submissions
 * using namespaces, setrlimit, chroot.
 * dependencies: libcgroup-dev 0.37, libcgroup1
 *
 */
//TODO: test cgroup when libcgroup-bin is absent.
#ifndef JUG_SANDBOX
#define JUG_SANDBOX

#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
//#include <libcgroup.h>
#include <pwd.h>
#include <grp.h>

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



#define PIPE_WRITETO 1
#define PIPE_READFROM 0

/**
 * enumeration of the Executer's different returns
 */
typedef enum{
	JS_CORRECT,
	JS_WRONG,
	JS_TIMELIMIT,
	JS_WALL_TIMELIMIT,
	JS_MEMLIMIT,
	JS_OUTPUTLIMIT,
	JS_RUNTIME,
	JS_PIPE_ERROR,
	JS_COMP_ERROR,
	JS_UNKNOWN
}
jug_sandbox_result;

/*
 * enumaration of the states of the execution of the binary process
 */
typedef enum{
	JS_BIN_NOTSTARTED,
	JS_BIN_RUNNING,
	JS_BIN_WALLTIMEOUT,
	JS_BIN_TIMEOUT,
	JS_BIN_COMPOUT
}js_bin_state;

/**
 * Sandbox structure
 * holds all the basic information about the sandbox
 */
struct sandbox{
	//parameters
	int use_ERFS;
	char* chroot_dir;	 ///< path to the root fs to use (to chroot into)
	int use_cgroups; 	///< wether to use cgroups;
	int use_setrlimit;	///< wether to use setrlimit : resources limit in linux.
//	int kill_on_compout; ///<flag: kill the submission before it terminate if it is judged wrong.
	//int show_submission_output; //<whether to show the submission's output on the screen

	int mem_limit_mb_default;
	int time_limit_ms_default;
	int walltime_limit_ms_default;
	int output_size_limit_mb_default;

//	int stack_size_mb; ///< the stack to allocate for the submission watcher
	//sandbox state
//	int nbr_instances;
	struct cgroup* sandbox_cgroup;
//	int count_submissions;
//	int count_submissions_success;

};

/**
 * parameter of run() function
 * a set of parameters to be handed to the runner, in order to judge a program's output.
 * this structure is to be filled with the user of the sandbox, and not the sandbox itself.
 */
struct run_params{
	//limits ans stuff
	int mem_limit_mb;	 ///< limit of memory usage per process, in megabytes
	int time_limit_ms;	 ///< limit of time assigned to the process, in miliseconds

	int fd_datasource;	///< where the submission acquire its inputs, it can be either in or out
	int fd_datasource_dir;	///< is it a fd where the user writes (1), or an fd to be read from by the Executer (0)

	int datasource_size; ///< size of the file where we get the input.

	int fd_output_ref;		///< file descriptor where we get the correct output, the user should manager close() of this fd
	//if template mode is used, this function should be
	// mapped in main process VM before calling jug_sandbox_template_init()
	//int(*compare_output)(int,char*,int,int);	///< address to a function where the comparaison is done

	//execution state variables (those are moved from globals for thread-safety)
	int watcher_pid; ///< the pid of the parent of the execution process. seen from outside the pid_namespace
	int out_pipe[2];///< the output pipe to the judge daemon. where the submission prints.
	int in_pipe[2]; ///< the input pipe, which we use to feed the submission with data
	int received_bytes; //< the count of received bytes from the executing binary.
	int thread_order;  //< thread identifier, used to give the watcher a hint about the thread that is running it.

	//the result of Execution
	jug_sandbox_result result;		//<  where we should put the result of the execution
};

/**
 * medium structure
 * @desc the structre we pass to the process we create inside the sandbox (watcher)
 */
struct clone_child_params{
	struct run_params* run_params_struct; ///< instance of run_params structure.
	struct sandbox* sandbox_struct; ///< a pointer to the sandbox structure.
	char* binary_path;	///< a path relative to the ERFS, where the binary resides
	char**argv; ///< a NULL-terminated list of arguments passed to the binary.
};



/**
 * Global Variables of the watcher process
 * @note They don't represent a threat while threading, because
 * 		 they are accessed only from the watcher process. and not from the spawner thread.
 * 		 other global-like variables are inserted in run_params in the 'execution state section'
 */
///< the global sandbox instance
struct sandbox* global_sandbox;
///< the pid of the binary, seen from inside the namespace.
int binary_pid;
///< indicates the state of the execution of the binary
js_bin_state binary_state;




/**
 * @desc	init the sandbox. By design this function should be called once.
 * @param sandbox_struct a pointer to a sandbox structer to be filled.
 * @return 0 if succeded, non-zero otherwise.
 */

int jug_sandbox_init();


struct sandbox* jug_sandbox_global();

/**
 * jug_sandbox_child
 * @desc the code that will be executed in the child (watcher)
 */

int jug_sandbox_watcher(void* arg);

/**
* jug_sandbox_binary
*
*/

int jug_sandbox_binary( struct clone_child_params* ccp, int binary_pipe[2] );

//5 seconds timelimit handler
void watcher_sig_handler(int sig);

/*
 * @desc print the jug_sandbox_result value in human-readible format
 */
const char* jug_sandbox_result_str(jug_sandbox_result result);
/*
 * jug_sandbox_memory_usage()
 * @desc calculate how much memory a process has used, reading the /proc/[pid]/statm file
 * @return the sum of data and stack usage in Bytes
 * @note this call is done inside the sandbox.
 */

 long jug_sandbox_memory_usage(pid_t pid);



/**
 * jug_sandbox_print_stats
 * @desc simple function to print spend time and memory by the binary process
 * @note for debug purposes. must be called from the watcher process.
 */

 long jug_sandbox_cputime_usage(pid_t pid);






#endif
