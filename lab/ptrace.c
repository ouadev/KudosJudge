#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>

#include <sys/ptrace.h>
#define bool short
#define true (short)1
#define false (short)0
pid_t pid;




/* wait for the pid to exit */
void wait_for_exit(void)
{
	int status;
	while (1) {
		/* wait for signal from child */
		waitpid(pid, &status, 0);
		if(WIFSTOPPED(status)){
			if ( (WSTOPSIG(status) == SIGTRAP) &&
					(status & (PTRACE_EVENT_FORK << 8))){
				printf("Child Stopped..., stop sig:%d\n",WSTOPSIG(status));
				break;

			}
		}
		/* if not, pass the original signal onto the child */
		if (ptrace(PTRACE_CONT, pid, 0, WSTOPSIG(status))) {
			perror("stopper: ptrace(PTRACE_CONT, ...)");
			exit(1);
		}
	}

	printf("[stopper] : stopped here\n");
	sleep(1);
	if (ptrace(PTRACE_CONT, pid, 0, 0)) {
		perror("stopper: last ptrace(PTRACE_CONT, ...) failed,");
		exit(1);
	}
	kill(pid,9);

	exit(0);

}

int main(int argc, char *argv[])
{

	if(argc<2){
		printf("no enough args\n");
		exit(55);
	}
	char*pathname=argv[1];

	switch (pid = fork()) {
	case -1:
		perror("stopper: fork");
		exit(1);
		break;
	case 0:
		if (ptrace(PTRACE_TRACEME, 0, (char *)1, 0) < 0) {
			perror("stopper: ptrace(PTRACE_TRACEME, ...)");
			return -1;
		}

		execv(pathname,argv+1);
		perror("stopper: execv");
		exit(1);

	default:
		/* wait for first signal, which is from child when it execs */
		wait(NULL);

		/* set option to tell us when the signal exits */
		if (ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACEFORK)) {
			perror("stopper: ptrace(PTRACE_SETOPTIONS, ...)");
			return -1;
		}

		/* allow child to continue */
		if (ptrace(PTRACE_CONT, pid, 0, (void*)0)) {
			perror("stopper: ptrace(PTRACE_CONT, ...)");
			return -1;
		}

		wait_for_exit();
	}
	/* shouldn't get here */
	exit(1);
}





