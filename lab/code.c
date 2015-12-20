#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
void sig_hnd(int sig){
	printf("sig alrm received\n\n");

}
int main(int argc,char*argv[]){
	int i;
	for(i=0;i<4;i++)
	printf("Second Code.c ....\n");
	sleep(1);

//
//	pid_t pid=fork();
//	if(pid==0){
//		printf("Child Got Here ... \n");
//	}
//	sleep(2);

	//	char* cmd="ls";
//	char*args[3]={cmd,NULL};
//	execvp(cmd,args);


//	char*cmd="ls /";
//	system(cmd);

	return 0;
}
