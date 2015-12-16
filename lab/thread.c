#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>


int x=0,y=0,go=0;

void *work(void *argument)
{
	while(1){
		if(go==1){
			printf("%d+%d=%d\n",x,y,x+y);
			go=0;
		}
		if(go<0) break;
	}
	printf("calculator exiting ....\n");
	return NULL;
}

int main(void)
{

	pthread_t thread;
	int result_code;
	int _arg=54;
	printf("spawner pid:%d\n",getppid());
	pthread_create(&thread,NULL,work,&_arg);


	while(1){
		scanf("%d %d",&x,&y);
		go=1;
		if(x<0 || y<0){
			go=-1;
			break;
		}
	}
	pthread_join(thread,NULL);


	exit(EXIT_SUCCESS);
}
