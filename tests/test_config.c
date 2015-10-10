#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../config.h"
#include "../log.h"


int main(int argc, char*argv[]){
	struct jug_ini ini;
	int ret=jug_ini_load("./test.ini",&ini);
	if(ret){
		debugt("test config","error loading filename");
		return 0;
	}

	char* label=jug_ini_get_string(&ini,"production","label");
	char* strint=jug_ini_get_string(&ini,"development","max_submissions");

	printf("label value : %s\n",label);
	printf("max submissions : %d\n",atoi(strint));

	char * unfound=jug_ini_get_string(&ini,"production","inexistant");
	if(!unfound)
		printf("this is as unfound key\n");



	return 5;



}