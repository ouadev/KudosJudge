
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

/**
 * globals
 */
int launcher_pid;

/**
 * code of launched process.
 */
int process_func(void* feed){
	char * str=(char*)feed;
	printf("process launched, feed : %s\n",str);
	//fflush(stderr);
	sleep(1);
	return atoi(str);
}
/*
 * launcher_sig_handler
 */
void launcher_sig_handler(int sig){
	if(sig==SIGINT) return;
	printf("launcher_handler: received %d\n",sig);
	fflush(stdout);
	//should use clone specifying to attach the child to the grandparent
	char* stack;
	char *stacktop;
	int STACK_SIZE=10*1000000; //10MB
	stack=(char*)malloc(STACK_SIZE);
	if(!stack){
		debugt("jug_sandbox_run","cannot malloc the stack for this submission");
		return ;
	}
	stacktop=stack;
	stacktop+=STACK_SIZE;
	char *feed=(char*)malloc(50);
	sprintf(feed,"%d",rand()%25);
	pid_t process_pid=clone(process_func,stacktop,CLONE_PARENT|SIGCHLD,feed);
	if(process_pid==-1){
		printf("cannot clone a new process\n");
	}
	wait(NULL);
	free(feed);
	free(stack);


}
/**
 * launcher_func
 */

int launcher_func(void*arg){
	debugt("launcher","launcher process started..");
	signal(SIGUSR1,launcher_sig_handler);
	signal(SIGINT,launcher_sig_handler);
	while(1){
		sleep(1);
		//printf("launcher_func is up : %d \n", getpid());
	}
	return 0;
}

/**
 *  main_sig_handler
 */
void main_sig_handler(int sig){
	printf("main: CTRL+C launching a process ...\n");
	kill(launcher_pid,SIGUSR1);
	int status;
	wait(&status);
	if(WIFEXITED(status)){
		printf("process attached, and exited with code : %d\n",WEXITSTATUS(status));
	}else printf("process attached, status %d\n",status);

}
/**
 * main
 */
int main(int argc, char* argv[]){

	char* stack;
	char *stacktop;
	int STACK_SIZE=10*1000000; //10MB
	stack=(char*)malloc(STACK_SIZE);
	if(!stack){
		debugt("jug_sandbox_run","cannot malloc the stack for this submission");
		return -1;
	}
	stacktop=stack;
	stacktop+=STACK_SIZE;

	launcher_pid=clone(launcher_func,stacktop,SIGCHLD,NULL);
	if(launcher_pid==-1){
		debugt("jug_sandbox_run","cannot clone() the submission executer, linux error: %s",strerror(errno));
		return -2;
	}
	sleep(1);
	signal(SIGINT,main_sig_handler);

	while(1);
//
//	int i=3;
//	for(i=0;i<3;i++){
//		kill(launcher_pid,SIGUSR1);
//		sleep(0.6);
//	}

	return 0;
}
