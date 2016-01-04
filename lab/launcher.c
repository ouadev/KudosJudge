
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


#include "../sandbox.h"
/**
 * globals
 */
int launcher_pid;
int _pipe[2];

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
	pid_t pid,c_pid;
	int ret,status;
	struct sandbox sb;
	//init template
	ret=jug_sandbox_template_init();
	if(ret!=0){
		printf("error init template\n");
		return 5;
	}
	//run using template

	//init sandbox
	ret=jug_sandbox_init(&sb);
	if(ret){
		printf("[test] cannot init sandbox : %d\n",ret);
		return 56;
	}


	//	int outfd=open("/tmp/dup.data",O_CREAT| O_TRUNC | O_WRONLY,S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
	int infd=open("/home/odev/jug/tests/problems/twins/twins.in",O_RDWR);
	int rightfd=open("/home/odev/jug/tests/problems/twins/twins.out",O_RDWR);
	jug_sandbox_result result;
	//change the config.ini default parameters with your own stuff
	struct run_params runp;

	runp.mem_limit_mb=-1;//1220000;
	runp.time_limit_ms=-1;//1000;
	runp.fd_datasource=infd;
	runp.fd_datasource_dir=0;
	runp.compare_output=compare_output;
	runp.fd_output_ref=rightfd;


	//args
	char* args[3]={"/bin/ls","-la",NULL};

	jug_sandbox_run_tpl(&runp,&sb,args[0],args,0);


	close(infd);
	close(rightfd);



	return 5;

	////////////////////////////////


	int x=545;
	int rep=0;
	while(1){
		pid=jug_sandbox_template_clone(&x,sizeof(int));
		printf("request sent, pid=%d\n",pid);
		fflush(stdout);
		if(pid<0){
			if(rep<5){
				rep++;
				printf("template_pid=%d\n",template_pid);
				sleep(2);
				continue;
			}else{
				rep=0;
			}
			printf("error cloning template\n");
			jug_sandbox_template_term();
			return 1;
		}

		//printf("pid=%d\n",(int)pid);
		c_pid=waitpid(pid,&status,0);
		//printf("c_pid=%d\n",(int)c_pid);
		if(WIFEXITED(status)){
			printf("exited:%d\n",WEXITSTATUS(status));
		}else printf("status:%d\n",status);
	}

	//	char* stack;
	//	char *stacktop;
	//	int STACK_SIZE=10*1000000; //10MB
	//	stack=(char*)malloc(STACK_SIZE);
	//	if(!stack){
	//		debugt("jug_sandbox_run","cannot malloc the stack for this submission");
	//		return -1;
	//	}
	//	stacktop=stack;
	//	stacktop+=STACK_SIZE;
	//
	//	launcher_pid=clone(launcher_func,stacktop,SIGCHLD,NULL);
	//	if(launcher_pid==-1){
	//		debugt("jug_sandbox_run","cannot clone() the submission executer, linux error: %s",strerror(errno));
	//		return -2;
	//	}
	//	sleep(1);
	//	signal(SIGINT,main_sig_handler);
	//
	//	while(1);


	return 0;
}
