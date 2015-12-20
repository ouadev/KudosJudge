/*
 * Author : Ouadim Jamal ouadev@yandex.com
 * Jug Sandbox
 * It does its best to secure the execution of submissions
 * using cgroups, namespaces, setrlimit, chroot.
 * dependencies: libcgroup-dev 0.37, libcgroup1
 * futur dev: implement our own libcgroup, because it just read and write to a mounted filesystem type "cgroup"
 *
 */
//TODO: test cgroup when libcgroup-bin is absent.
#ifndef JUG_SANDBOX
#define JUG_SANDBOX

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libcgroup.h>
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

#include "config.h"
#include "compare.h"


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
	char* chroot_dir;	 ///< path to the root fs to use (to chroot into)
	int use_cgroups; 	///< wether to use cgroups;
	int use_setrlimit;	///< wether to use setrlimit : resources limit in linux.
	int kill_on_compout; ///<flag: kill the submission before it terminate if it is judged wrong.
	int show_submission_output; //<whather to show the submission's output on the screen

	int mem_limit_mb_default;
	int time_limit_ms_default;
	int walltime_limit_ms_default;
	int output_size_limit_mb_default;

	int stack_size_mb; ///< the stack to allocate for the submission watcher
	//sandbox state
	int nbr_instances;
	struct cgroup* sandbox_cgroup;

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

	int fd_output_ref;		///< file descriptor where we get the correct output, the user should manager close() of this fd
	int(*compare_output)(int,char*,int,int);	///< address to a function where the comparaison is done

	//execution state variables (those are moved from globals for thread-safety)
	int watcher_pid; ///< the pid of the parent of the execution process. seen from outside the pid_namespace
	int out_pipe[2];///< the output pipe to the judge daemon. where the submission prints.
	int in_pipe[2]; ///< the input pipe, which we use to feed the submission with data
	int received_bytes; //< the count of received bytes from the executing binary.

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
 * @note They don't present a threat while threading, because
 * 		 they are accessed only from the watcher process. and not from the spawner thread.
 * 		 other global-like variables are inserted in run_params in the 'execution state section'
 */
///< the pid of the binary, seen from inside the namespace.
int binary_pid;
////indicates the state of the execution of the binary
js_bin_state binary_state;



/**
 * @desc init the sandbox
 * @param sandbox_struct a pointer to a sandbox structre to be filled
 * @return 0 if succeded, non-zero otherwise.
 */

int jug_sandbox_init(struct sandbox* sandbox_struct);

/**
 * @desc create the cgroup representing the context of the sandbox.
 * @param sandbox_cgroup libcgroup structure representing a cgroup (read libcgroup doc)
 * @param sandbox_struct the sandbox structure that contains the sandbox parameters
 * @return 0 in success
 */
int jug_sandbox_create_cgroup(struct cgroup* sandbox_cgroup,struct sandbox* sandbox_struct);

/**
 * @desc Run a program inside the sandbox (the main function)
 *
 *
 * @note The Threading Manager should keep a pool of threads to reduce the overhead
 * 		of launching a thread for each submission, and then kill it.
 */


int jug_sandbox_run(struct run_params* run_params_struct,
		struct sandbox* sandbox_struct,
		char* binary_path,
		char*argv[]);


int jug_sandbox_child(void* arg);

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

unsigned long jug_sandbox_memory_usage(pid_t pid);



/**
 * jug_sandbox_print_stats
 * @desc simple function to print spend time and memory by the binary process
 * @note for debug purposes. must be called from the watcher process.
 */

unsigned long jug_sandbox_cputime_usage(pid_t pid);














#endif
