
#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>


#include <fcntl.h>

 #include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>

int pipe_inout[2];


static int child(void*arg){
	printf("child:  (%d,%d) \n",getpid(),getppid() );
	//close stdout, and rediect it to the pipe
	while( ( dup2( pipe_inout[1],STDOUT_FILENO)==-1) && (errno==EINTR)){}
	close(pipe_inout[1]);
	close(pipe_inout[0]);
	sleep(0.5);
	
	fprintf(stderr,"stderr message\n");
	printf("child: exiting ...");
	fflush(stdout);
	execvp("/bin/sh",NULL);
	return 0;
}

/**
* main()
*
*
*/
int main(int argc, char*argv[]){


	printf("father: (%d,%d) \n",getpid(),getppid());
	//printf("%d\n",SIGXCPU);


	/* create a pipe */
	if(pipe(pipe_inout)==-1){
		printf("father: cannot create a pipe\n");
		exit(77);
	}

	//while( (dup2(STDIN_FILENO,pipe_inout[0])==-1) && (errno==EINTR) ){}
	
	/* clone */
	char* stack;
	char *stacktop;
	int STACK_SIZE=1024*1024;
	stack=(char*)malloc(STACK_SIZE);
	if(!stack){
		printf("cannot malloc() stack\n");
		return -10;
	}
	stacktop=stack;
	stacktop+=STACK_SIZE;
	pid_t pid=clone(child,stacktop,SIGCHLD,argv[1]);
	if(pid==-1){
		printf("clone() failed \n");
		return -5;
	}


	
	int status;
	wait(&status);
	printf("father: child exited with satus : %d \n",status);
	printf("father: exiting ....\n");

	/*read output*/
	while(1){
		char buffer[120];
		ssize_t c=read(pipe_inout[0],buffer,110);
		if(c==-1){
			exit(47);
		}
		if(c==0){
			break;
		}else{
			buffer[c]='\0';
			printf("#%s#\n",buffer);
		}

	}

	return 0;
	
}