#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "log.h"





void debug(char* message){
	printf("%s\n",message?message:"unknown debug message");
}

void debugl(){
	printf("(%d:%s)\n",errno,strerror(errno));
}

void debugll(char*message){
	printf("%s (%d,%s)\n",message?message:"unknown msg",errno,strerror(errno));
}

void debugt(char*tag,char*message){
	printf("%s: %s\n",tag,message);
}

