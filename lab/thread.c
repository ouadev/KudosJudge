
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
	char** argument=(char**)arg;
	char* sresult=argument[0];
	argument+=1;
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

	jug_sandbox_run(&runp,&sb,argument[0],argument+1);

	result=runp.result;
	//debugt("test_sandbox","Executer result is : %s",jug_sandbox_result_str(result));
	strcpy(sresult,jug_sandbox_result_str(result));

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
	pthread_t threads[2];
	int i;

	for(i=0;i<2;i++){

		char sr[400];sr[0]='\0';
		char* args[3]={sr,"/opt/twins",NULL};
		pthread_create(&threads[i],NULL,work,args);
		pthread_join(threads[i],NULL);

		printf("[Launcher] result is: %s\n\n",sr);

	}

	exit(EXIT_SUCCESS);
}
