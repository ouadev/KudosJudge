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
 #include <sys/signalfd.h>

 #include "config.h"

 /**
 * Global Variables
 */
 //the pid of the parent of the execution process.
int innerwatcher_pid;
//the output pipe to the judge daemon.
int out_pipe[2];
//file descriptor to receive signal from the executing submission
int sigxcpu_handler_fd;
//indicates the state of the execution of the submission process.
//0:not started, 1:started, 2:cannot be killed, 3: killed
int execution_state;
/**
* Sandbox structure
* holds all the basic information about the sandbox
*/
struct sandbox{
	//parameters
	char* chroot_dir;	 ///< path to the root fs to use (to chroot into)
	int use_cgroups; 	///< wether to use cgroups;
	int use_setrlimit;	///< wether to use setrlimit : resources limit in linux.
	int mem_limit_mb_default;
	int time_limit_ms_default;
	int stack_size_mb_default;
	//sandbox state
	int nbr_instances;
	struct cgroup* sandbox_cgroup;
};

/**
* parameter of run() functio
*/
struct run_params{
	int mem_limit_mb;	 ///< limit of memory usage per process, in megabytes
	int time_limit_ms;	 ///< limit of time assigned to the process, in miliseconds
	int stack_size_mb;	///< limit of allowed stack, in megabytes.
	
};


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
	//clone into new namespaces.
	//redirecting stuff IDontKnow
	//chroot/fs_ns into the jug_rootfs
	//parameter the cgroup/setrlimit
	//watch for signals/(execution timeout)
	//kill it if it's possible/freeze if cgroups
	//control writing operations to /tmp.
	//fetch data from the std_out of the binary.


int jug_sandbox_run(struct run_params* run_params_struct,
	struct sandbox* sandbox_struct,
	char* binary_path,
	char*argv[]);


 int jug_sandbox_child(void* arg);

//5 seconds timelimit handler
void timeout_handler(int sig);
//cpu consumed handler
static void rlimit_cpu_handler(int sig);
#endif