#include <stdio.h>
#include <stdlib.h>
	#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

//
double get_wall_time(){
	static double t0=0;
	struct timeval time;
	if (gettimeofday(&time,NULL)){
		return 0;
	}
	double t=(double)time.tv_sec + (double)time.tv_usec * .000001;
	if(t0==0) t0=t;
	printf("#time : %f\n ",(t-t0)*1000);
	return t;
}

//main

int main(int argc,char*argv[]){
	if(argc==1){
		printf("args missing\n");
		return -1;
	}
	char* gcc=argv[1];
	printf("%s\n",gcc);
	char* outdir;
	if(argc==3)
		outdir=argv[2];
	else outdir="/tmp";
	char outfile[200];
	strcpy(outfile,outdir);
	strcat(outfile,"/code.out");
	printf("outfile:%s\n",outfile);
	char** args=(char**)malloc(4*sizeof(char*));
	args[0]=(char*)malloc(50*sizeof(char));
	args[1]=(char*)malloc(50*sizeof(char));
	args[2]=(char*)malloc(50*sizeof(char));
	args[3]=(char*)malloc(50*sizeof(char));
	strcpy(args[0],"gcc");
	strcpy(args[1],"code.c");
	strcpy(args[2],"-o");
	strcpy(args[3],outfile);
	//fork the compiler 
	get_wall_time();
	pid_t child_pid=fork();
	if(child_pid==0){
		//into child
		remove(outfile);
		int res=execv(gcc,args);
		if(res==-1){
			printf("error execv : %s \n",strerror(errno));
		}
		return 0;
	}
	//parent continues
	int status;
	
	if(waitpid(child_pid,&status,0)>0 && WIFEXITED(status)){
		printf("compiling finished\n");
		get_wall_time();
	}
	else{
		puts("error compiling");
		return 0;
	}

	//run the code

	char**runargs=(char**)malloc(1*sizeof(char*));
	runargs[0]=(char*)malloc(50*sizeof(char));
	strcpy(runargs[0],"code.out");
	puts("~~~~~~~~~~~~~~~~~~~~~~~~~~");
	//fork
	pid_t run_pid=fork();
	if(run_pid==0){
		int rret=execv(outfile,runargs);
		if(rret==-1) printf("cannot run compiled code:%s\n",strerror(errno));
		return 0;

	}
	if(waitpid(run_pid,&status,0)>0 && WIFEXITED(status)){
		puts("~~~~~~~~~~~~~~~~~~~~~~~~~~");
		printf("run finished\n");
	}else
		printf("error waiting for runner\n");
	
	get_wall_time();
	return 0;
}