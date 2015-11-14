
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

int pipe_inout[2];
//test function to generate load on cpu
int fact(int u){
	if(u==1) return 1;
	return fact(u-1);
}

static int child(void*arg){
	printf("child:  (%d,%d) \n",getpid(),getppid() );
	//
	struct rlimit cpu_rlimit;
	getrlimit(RLIMIT_CPU,&cpu_rlimit);
	cpu_rlimit.rlim_cur=2;
	cpu_rlimit.rlim_max=cpu_rlimit.rlim_cur+2;//2 seconds of margin
	int fail=setrlimit(RLIMIT_CPU,&cpu_rlimit);
	if(fail){
		printf("child:cannot set cpu rlimit");
		exit(100);
	}

	while(1) fact(10);

	printf("child: exiting ...");
	return 0;
}

/**
 * main()
 *
 *
 */
int main(int argc, char*argv[]){


	printf("father: (%d,%d) \n",getpid(),getppid());


	/* clone */
	char* stack;
	char *stacktop;
	int STACK_SIZE=1024*1024;
	stack=(char*)malloc(STACK_SIZE);
	if(!stack){
		printf("cannot malloc() stack\n");
		return -10;
	}
	stacktop=stack;
	stacktop+=STACK_SIZE;
	pid_t pid=clone(child,stacktop,SIGCHLD,argv[1]);
	if(pid==-1){
		printf("clone() failed \n");
		return -5;
	}



	int status;
	wait(&status);
	printf("father: child exited with satus : %d \n",status);
	printf("father: exiting ....\n");



	return 0;

}
