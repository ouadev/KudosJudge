#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include "log.h"





void debug(char* message,...){
	char dbg_msg[400];
	va_list args;
	va_start(args,message);
	vsprintf(dbg_msg,message,args);
	va_end(args);
	fprintf(stderr,"%s\n",dbg_msg);
	
}

void debugl(){
	fprintf(stderr,"(%d:%s)\n",errno,strerror(errno));
}

void debugll(char*message,...){
	char* linux_error=strerror(errno);
	char* dbg_msg=(char*)malloc(strlen(message)+strlen(linux_error)+240);

	va_list args;
	va_start(args,message);
	vsprintf(dbg_msg,message,args);
	va_end(args);
	sprintf(dbg_msg,"%s : (%d,%s)",dbg_msg,errno,linux_error);
	fprintf(stderr,"%s\n",dbg_msg);
}

void debugt(char*tag,char*message,...){
	char* dbg_msg=(char*)malloc(strlen(message)+strlen(tag)+120);
	va_list args;
	va_start(args,message);
	vsprintf(dbg_msg,message,args);
	va_end(args);
	//

	fprintf(stderr, "%s: %s\n",tag,dbg_msg);
}


//
void print_bytes(char*buffer,int len){
	int i=0;
	for(i=0;i<len;i++){
		if(buffer[i]=='\n')
			fprintf(stderr,"{%02x}",buffer[i]);
		fprintf(stderr,"%02x",buffer[i]);

	}
	fprintf(stderr,"\n");
}
