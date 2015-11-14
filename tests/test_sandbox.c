#include "../sandbox.h"


//use : "sudo -E ./test-sandbox" to execute this test

int main(int argc, char*argv[]){

	if(argc<2){
		printf("no enough args\n");
		exit (475);
	}
	int ret;
	struct sandbox sb;


	ret=jug_sandbox_init(&sb);


	if(ret){
		printf("[test] cannot init sandbox : %d\n",ret);
		return 1;
	}
	
	int outfd=open("/tmp/dup.data",O_CREAT| O_TRUNC | O_WRONLY,S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
	int infd=open("/tmp/script",O_RDWR);
	int rightfd=open("/tmp/correct.data",O_RDWR);
	int stat=fcntl(infd,F_GETFL);

	struct run_params runp;
	runp.mem_limit_mb=1220000;
	runp.time_limit_ms=1000;
	runp.stack_size_mb=40;
	runp.fd_datasource=infd;
	runp.fd_datasource_dir=0;
	runp.compare_output=compare_output;
	runp.fd_output_ref=rightfd;
	char* arg_vector[]={"/bin/sh",NULL};
	jug_sandbox_run(&runp,&sb,argv[1],arg_vector);

	return 0;
}
