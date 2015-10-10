#include "config.h"

//jug_ini_load()
int jug_ini_load(char* filename,struct jug_ini* ini_struct){
	if(!filename){
		debug("jug_ini : filename is null ");
		return -1;
	}
	struct read_ini *ini_r = NULL;
	struct ini *ini = read_ini(&ini_r, filename);
	ini_struct->iniparser=ini;
	ini_struct->loaded=1;
	return 0;
}




//jug_ini_get_string()
char* jug_ini_get_string(struct jug_ini* ini_struct,char* section,char* key){
	if(!ini_struct->loaded){
		debugt("jug_ini","jug_ini structure not loaded");
		return NULL;
	}
	return ini_get_value(ini_struct->iniparser,section,key);
}


