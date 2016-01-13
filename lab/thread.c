
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
 * defs
 */
struct worker_arg{
	char* prog;
	int thread_order;
};

/**
 * globals
 */

pthread_t threads[5];
struct sandbox sb;
/**
 *
 * child_atfork
 *
 */

void __atfork(){

}
/*
 * launch
 */

int launch(struct sandbox* psandbox,char*argv[],int thread_order){

	pid_t pid,c_pid;
	int ret,status;
	int infd,rightfd;
	jug_sandbox_result result;

	infd=open(		"/home/odev/jug/tests/problems/twins/twins.in",O_RDWR);
	rightfd=open(	"/home/odev/jug/tests/problems/twins/twins.out",O_RDWR);

	//change the config.ini default parameters with your own stuff
	struct run_params runp;
	runp.mem_limit_mb=-1;//1220000;
	runp.time_limit_ms=-1;//1000;
	runp.fd_datasource=infd;
	runp.compare_output=compare_output;
	runp.fd_output_ref=rightfd;

	//args
	ret=jug_sandbox_run_tpl(&runp,psandbox,argv[0],argv,thread_order);

	close(infd);
	close(rightfd);

	return ret;
}
/*
 *
 * Work()
 *
 */

void *work(void* arg)
{
	struct worker_arg* wa=(struct worker_arg*)arg;
	char** args=(char**)malloc(3*sizeof(char**));
	args[0]=(char*)malloc(99);
	strcpy(args[0],wa->prog);
	args[1]=NULL;


	launch(&sb,args,wa->thread_order);
	//pthread_detach(pthread_self());
	return NULL;
}

/*
 *
 * main()
 *
 */

int main(void)
{
	int ret;

	//init debugging params
	char* tags[6]={"Binary",NULL};
	debug_focus(tags);
	//	debug_focus(NULL);
	//init sandbox (should be first)

	ret=jug_sandbox_init(&sb);
	if(ret){
		printf("[test] cannot init sandbox : %d\n",ret);
		return 56;
	}
	//init template
	ret=jug_sandbox_template_init();
	if(ret!=0){
		printf("error init template\n");
		return 5;
	}
	sleep(2);

	//threading
	int i,nt=2,error;

	char* prog1=(char*)malloc(80);
	strcpy(prog1,"/opt/twins");
	struct worker_arg* wa0=(struct worker_arg*)malloc(sizeof(struct worker_arg));
	wa0->prog=prog1;
	wa0->thread_order=0;
	/////////////////////////////////////////
	error=pthread_create(&threads[0],NULL,work,wa0);
	if(error){
		perror("pthread_create()\n");
	}
	sleep(0.2);
	////////////////////////////////////////////
	struct worker_arg* wa1=(struct worker_arg*)malloc(sizeof(struct worker_arg));
	wa1->prog=prog1;
	wa1->thread_order=1;
	error=pthread_create(&threads[0],NULL,work,wa1);
	if(error){
		perror("pthread_create()\n");
	}
	////////////////////////////////////////////
	struct worker_arg* wa2=(struct worker_arg*)malloc(sizeof(struct worker_arg));
	wa2->prog=prog1;
	wa2->thread_order=2;
	error=pthread_create(&threads[0],NULL,work,wa2);
	if(error){
		perror("pthread_create()\n");
	}
	////////////////////////////////////////////
	struct worker_arg* wa3=(struct worker_arg*)malloc(sizeof(struct worker_arg));
	wa3->prog=prog1;
	wa3->thread_order=3;
	error=pthread_create(&threads[0],NULL,work,wa3);
	if(error){
		perror("pthread_create()\n");
	}

	pthread_join(threads[0],NULL);
	pthread_join(threads[1],NULL);
	pthread_join(threads[2],NULL);
	pthread_join(threads[3],NULL);

	jug_sandbox_template_term();

	sleep(2);
	exit(EXIT_SUCCESS);
}






