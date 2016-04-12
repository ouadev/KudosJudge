
#include "log.h"


/**
 * globals
 *
 */
//the tags to focus on
short _is_focus_tags=0;
char _focus_tags[20][10];//10 of 20-chars strings


void debug(char* message,...){
	char* dbg_msg=(char*)malloc(strlen(message)+120);
	va_list args;
	va_start(args,message);
	vsprintf(dbg_msg,message,args);
	va_end(args);
	fprintf(stderr,"+ %s\n",dbg_msg);
	fflush(stderr);
	free(dbg_msg);

}

void debugl(){
	fprintf(stderr,"+ (%d:%s)\n",errno,strerror(errno));
}

void debugll(char*message,...){
	char* linux_error=strerror(errno);
	char* dbg_msg=(char*)malloc(strlen(message)+strlen(linux_error)+240);

	va_list args;
	va_start(args,message);
	vsprintf(dbg_msg,message,args);
	va_end(args);
	sprintf(dbg_msg,"%s : (%d,%s)",dbg_msg,errno,linux_error);
	fprintf(stderr,"+ %s\n",dbg_msg);
	free(dbg_msg);
}

void debugt(char*tag,char*message,...){
//	//fixme: TESTUNIT
//	return;

	//check focus_tags
	if(_is_focus_tags){
		short inside=0,i=0;
		for(i=0;i<8;i++){
			//printf("ft %i : %s, cmp=%d\n",i,_focus_tags[i],strncmp(_focus_tags[i],tag,20));
			if(strncmp(_focus_tags[i],tag,20)==0){
				inside=1;
				break;
			}
		}
		if(!inside) return;
	}

	//process
	char* dbg_msg=(char*)malloc(strlen(message)+strlen(tag)+120);
	va_list args;
	va_start(args,message);
	vsprintf(dbg_msg,message,args);
	va_end(args);
	//

	fprintf(stderr, "+ %s: %s\n",tag,dbg_msg);
	fflush(stderr);
	free(dbg_msg);
}


//print bytes in hexa format, inernal use
void print_bytes(char*buffer,int len){
	int i=0;
	for(i=0;i<len;i++){
		if(buffer[i]=='\n')
			fprintf(stderr,"{%02x}",buffer[i]);
		fprintf(stderr,"%02x",buffer[i]);

	}
	fprintf(stderr,"\n");
}


void debug_fnprintf(FILE* where,char* buffer, int len){
	int i;
	for(i=0;i<len;i++){
		fprintf(where,"%c",buffer [i]);
	}
	fprintf(where,"\n");
}


//void debug_focus(char* tags[]);
void debug_focus(char* tags[]){

	int max_tags=8;
	int i=0;
	if(tags==NULL)
		_is_focus_tags=0;

	while(tags[i]!=NULL && i<=max_tags){
		strncpy(_focus_tags[i],tags[i],19);
		i++;
	}


	_focus_tags[i][0]='\0';
	_is_focus_tags=1;

}

void print_focus_tags(){
	int i=0;
	while(_focus_tags[i][0]!='\0' && i<10){

		fprintf(stderr,"Focus On: [%s]\n",_focus_tags[i]);
		i++;
	}
}



/**
 * log to syslog device. (useful for daemon stuff)
 *
 * void kjd_log(char* msg)
 */
void kjd_log( char* message,...){

	//
	char* dbg_msg=(char*)malloc(strlen(message)+120);
	va_list args;
	va_start(args,message);
	vsprintf(dbg_msg,message,args);
	va_end(args);
	//

	syslog(LOG_INFO, "%s\n",dbg_msg);
}



