
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
#include <sys/ptrace.h>

#include "../log.h"

void sig_handler(int sign,siginfo_t* siginfo,void* context){
	printf("signal %d received\n",sign);
	printf("reason : %s\n",siginfo->si_code==SEGV_MAPERR?"not mapped":"invalid perm");
	printf("errno  : %d\n",siginfo->si_errno);
	exit(44);
}

/**
 * main()
 *
 *
 */
int main(int argc, char*argv[]){
	struct sigaction sa;
	sa.sa_flags=SA_SIGINFO;
	sa.sa_sigaction=sig_handler;
	sigaction(SIGSEGV,&sa,NULL);

	struct rlimit lim;
	lim.rlim_cur=10;
	lim.rlim_max=lim.rlim_cur;
	setrlimit(RLIMIT_AS,&lim);

	//
	int a=malloc(256000);
	printf("allocated: %d\n",a);
	strcpy(a,"ouadimjamalmustgotosleep");
	return 0;

}







