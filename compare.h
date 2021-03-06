#ifndef JUG_COMPARE
#define JUG_COMPARE

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
 #include <errno.h>
 #include <memory.h>
#include <signal.h>
#include <time.h>
 #include <fcntl.h>
 #include <sys/types.h>
 #include <sys/stat.h>


#include "config.h"

#define READ_SIZE 256
/**
 * default function to compare two streams of output via two file descriptors.
 *
 */

/**
 * global variables of the comparaison function
 */

int fd_out_ref_checked; 	//< wether we already checked the validity of fd_out_ref


/**
 * @desc receive
 * @param rx a chunk of data, supposedly received from the submission.
 * @param size the size of rx
 * @param stage boolean value indicates if the submission is done producing output
 * 		  -1:last call,0:first call, else: intermediate calls
 * @return returns 0 if output is equal,
 * @note in the last call to compare_output (end==1), there will be no comparing done. rx may be NULL
 */

int compare_output(int fd_out_ref,char* rx,int size,int stage);






#endif
