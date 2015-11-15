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
	
	char** args=(char**)malloc(4*sizeof(char*));
	args[0]=(char*)malloc(50*sizeof(char));
	args[1]=(char*)malloc(50*sizeof(char));
	args[2]=(char*)malloc(50*sizeof(char));
	args[3]=(char*)malloc(50*sizeof(char));
	strcpy(args[0],"gcc");
	strcpy(args[1],"code.c");
	strcpy(args[2],"-o");
	strcpy(args[3],"/tmp/code.out");
	//second compiler
	char** args2=(char**)malloc(4*sizeof(char*));
	args2[0]=(char*)malloc(50*sizeof(char));
	args2[1]=(char*)malloc(50*sizeof(char));
	args2[2]=(char*)malloc(50*sizeof(char));
	args2[3]=(char*)malloc(50*sizeof(char));
	strcpy(args2[0],"gcc");
	strcpy(args2[1],"code.c");
	strcpy(args2[2],"-o");
	strcpy(args2[3],"/tmp/code.out");
	//fork the first compiler 
	get_wall_time();
	pid_t child_pid=fork();
	if(child_pid==0){
		//into child
		remove("/tmp/code.out");
		int res=execv("/media/jug/jug-gcc",args);
		if(res==-1){
			printf("error execv : %s \n",strerror(errno));
		}
		return 0;
	}
	//fork second compiler
	get_wall_time();
	pid_t child2_pid=fork();
	if(child2_pid==0){
		//into child
		remove("/tmp/code2.out");
		int res2=execv("/usr/bin/gcc-4.6",args2);
		if(res2==-1){
			printf("error execv 2: %s \n",strerror(errno));
		}
		return 0;
	}
	//parent continues
	int status;
	pid_t exit_pid=waitpid(-1,&status,0);
	
	if(exit_pid>0 && WIFEXITED(status)){
		printf("winner is : %s \n",exit_pid==child_pid?"jug-gcc":"gcc");
		get_wall_time();
		pid_t exit2_pid=waitpid(-1,&status,0);
		if(exit2_pid>0 && WIFEXITED(status)){
			printf("second place : %s\n",exit_pid==child_pid?"gcc":"jug-gcc");
			get_wall_time();
		}else{
			puts("error waiting for second child to exit");
			return 0;
		}
	}
	else{
		puts("error compiling");
		return 0;
	}

	
	return 0;
}