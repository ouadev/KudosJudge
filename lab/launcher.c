
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
int launch(struct sandbox* psandbox,char*argv[]){
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

	ret=jug_sandbox_run_tpl(&runp,psandbox,argv[0],argv,0);

	close(infd);
	close(rightfd);

	return ret;
}


/**
 * main
 */
int main(int argc, char* argv[]){
	int ret,i;
	struct sandbox sb;
	//init logger
	char* _tags[8]={"run_tpl","[launcher]",NULL};
	//debug_focus(_tags);

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
	//run using template
	///////////////////////////////////////////////
	sleep(2);
	char* args[3]={"/opt/twins","-la",NULL};
	int success=0;
	for(i=0;i<4;i++){
		if(launch(&sb,args)){
			debugt("[launcher]","run failed, success=%d",success);
			break;
		}else
			success++;
	}
	printf("[launcher]: success=%d\n\n",success);
	///////////////////////////////////////////////
	jug_sandbox_template_term();



	return 5;
}
