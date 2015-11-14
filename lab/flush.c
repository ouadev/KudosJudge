
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

int out_pipe[2];
//test function to generate load on cpu
int fact(int u){
	if(u==1) return 1;
	return fact(u-1);
}

void sig_handler(int sig);

/**
 * main()
 *
 *
 */
int main(int argc, char*argv[]){
	int fail;
	if(argc<2){
		printf("no enough args\n");
		exit(4);
	}

	printf("father: (%d,%d) \n",getpid(),getppid());

	/*pipe*/
	if(pipe(out_pipe) != 0) fprintf(stderr,"problem pipe creation\n");
	/* fork */
	pid_t pid =fork();
	if(pid==-1){
		printf("clone() failed \n");
		return -5;
	}
	int tmpfd=open("./out.data",O_CREAT| O_TRUNC | O_WRONLY,S_IWUSR|S_IRUSR);
	if(tmpfd<0) fprintf(stderr,"error openong out.data : %s\n",strerror(errno));
	if(pid>0){
		//father
		close(out_pipe[1]);
		int status;
		while(1){
			pid_t wpid=waitpid(pid,&status,WNOHANG|WUNTRACED);
			if(wpid<0){
				fprintf(stderr,"father: error waiting: %s \n",strerror(errno));
				continue;
			}else if(wpid>0){
				fflush(stdout);
				//something happened to the inner watcher.
				if (WIFEXITED(status)){
					fprintf(stderr,"father: son exited: %d\n",WEXITSTATUS(status));

					break;
				}
				else if(WIFSIGNALED(status)){
					fprintf(stderr,"father: son signaled : %d\n",WTERMSIG(status));

					break;
				}else fprintf(stderr,"father: son nor exited nor signaled, though event received\n");
			}
			//
			char buffer[256];
			int count=read(out_pipe[0],buffer,255);
			if(count<0 && errno==EAGAIN) continue;
			if(count<0){
				fprintf(stderr,"testing, error reading from the output pipe\n");
				//TODO: here, the judgement must be set to (server error);
				break;
			}

			buffer[count]='\0';
			write(tmpfd,buffer,count);
//			write(STDOUT_FILENO,buffer,count);



			//debugt("Captor", "%d written",ww);

		}

		close(tmpfd);
		exit(44);
	}

	//son
	close(out_pipe[0]);
	dup2(out_pipe[1],STDOUT_FILENO);
	close(out_pipe[1]);

	printf("child: trying to execute cmd\n");

	signal(SIGINT,sig_handler);
	signal(SIGALRM,sig_handler);
	alarm(3);

//	while(1) printf("%d\n",rand()%41);
	execvp(argv[1],argv+1);





	return 0;

}







void sig_handler(int sig){
	if(sig==SIGINT){
		printf("sig int received\n");
		fflush(stdout);
		exit(4);
	}
	if(sig==SIGALRM){
		printf("sig alarm received\n");
		fflush(stdout);
		exit(5);
	}
}
