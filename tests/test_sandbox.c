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
	
	struct run_params runp;
	runp.mem_limit_mb=256;
	runp.time_limit_ms=2000;
	runp.stack_size_mb=4;
	char* arg_vector[]={"/bin/sh",NULL};
	jug_sandbox_run(&runp,&sb,argv[1],arg_vector);

	return 0;
}
