
#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>


#include <fcntl.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/ptrace.h>

#include "../log.h"


/**
 * main()
 *
 *
 */
int main(int argc, char*argv[]){
	int fail;
	if(argc<2){
		printf("no enough args\n");
		exit(4);
	}

	printf("father: (%d,%d) \n",getpid(),getppid());



	/* fork */
	pid_t pid =fork();
	if(pid==-1){
		printf("clone() failed \n");
		return -5;
	}else if (pid>0){
		//parent
		printf("the father\n");
		int exitstatus;
		while(1){
			wait(&exitstatus);
			if(WIFSTOPPED(exitstatus)){
				debugt("parent","Child stpopped with signal : %d",WSTOPSIG(exitstatus) );
				fail = ptrace(PTRACE_CONT,pid,NULL,NULL);
				debugt("child","Child continuing");
				sleep(4);
				if(fail==-1) debugt("child","ptrace_cont failed");
			}
			else if(WIFEXITED(exitstatus)){
				debugt("parent","child exited, with code : %d",WEXITSTATUS(exitstatus) );
				break;
			}
			else if(WIFSIGNALED(exitstatus)){
				debugt("parent","child signaled, signal : %d",WTERMSIG(exitstatus) );
				break;
			}
			else{
				debugt("child","GrandChild chanegd state, status : %d",exitstatus );
				break;
			}
		}


	}

	//child
	ptrace(PTRACE_TRACEME,0,NULL,NULL);
	char* arg_vector[]={"/bin/sh",NULL};
	execvp(argv[1],arg_vector);

	return 0;

}







