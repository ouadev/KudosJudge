
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

/*
 *
 * Work()
 *
 */

void *work(void* arg)
{

	char** args=(char**)malloc(3*sizeof(char**));
	args[0]=(char*)malloc(99);
	strcpy(args[0],arg);
	args[1]=NULL;

	int ret;
	struct sandbox sb;
	ret=jug_sandbox_init(&sb);
	if(ret){
		printf("[test] cannot init sandbox : %d\n",ret);
		return NULL;
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

	jug_sandbox_run(&runp,&sb,args[0],args);

	result=runp.result;
	debugt("[[Thread]]","Executer result is : %s",jug_sandbox_result_str(result));


	close(infd);
	close(rightfd);

	return NULL;
}

/*
 *
 * main()
 *
 */
int main(void)
{
	printf("Start Thread Experiment\n");
	pthread_t threads[5];
	int i,nt=2;
	char sr0[500],sr1[500];
	char* prog1=(char*)malloc(80);
	char* prog2=(char*)malloc(80);
	strcpy(prog1,"/opt/twins");
	strcpy(prog2,"/opt/twins");



	pthread_create(&threads[0],NULL,work,prog1);
//	pthread_join(threads[0],NULL);
	printf("\n#####\n");
	pthread_create(&threads[1],NULL,work,prog2);
//	pthread_join(threads[1],NULL);
	printf("\n#####\n");
	pthread_create(&threads[2],NULL,work,prog2);
	printf("\n#####\n");
	pthread_create(&threads[3],NULL,work,prog2);

	//pthread_join(threads[0],NULL);


	printf("End Thread Experiment\n");

	while(1);
	exit(EXIT_SUCCESS);
}






