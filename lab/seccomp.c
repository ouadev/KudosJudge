#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/prctl.h>
#include <seccomp.h>


int seccomp_filter(){
	struct soc_filter filter[]={
		VALIDATE_ARCHITECTURE,
		EXAMINE_SYSCALL,
		ALLOW_SYSCALL(rt_sigreturn),
		ALLOW_SYSCALL(sigreturn),
		ALLOW_SYSCALL(exit_group),
		ALLOW_SYSCALL(exit),
		ALLOW_SYSCALL(read),
		ALLOW_SYSCALL(write),
		KILL_PROCESS,
	};
	struct sock_fprog prog= {
		.len=(unsigned short)(sizeof(filter)/sizeof(filter[0])),
		.filter=filter,
	};

	if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)){
		perror("prctl(NO new privs)");
		goto failed;
	}
	if(prctl(PR_SET_SECCOMP,SECCOMP_MODE_FILTER,&prog)){
		perror("prctl(SECCOMP)");
		goto failed;
	}
	return 0;
	failed:
	if(errno==EINVAL)
		fprintf(stderr,"SECCOMP_FILTER is not available");
	return 1;
}


int main(int argc, int*argv[])
{
	char buff[1025];
	if(seccomp_filter()){
		printf("error installing the filter");
		return -1;
	}
	fflush(NULL);
	buff[0]='\0';
	fgets(buff,500,stdin);
	printf("r: %s\n",buff);
	fflush(NULL);
	sleep(1);
	fork();
	printf("after fork message");

	return 0;

}