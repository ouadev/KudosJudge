#include "config.h"


struct jug_ini singleton_ini;
int loaded=0;

//jug_ini_load()
 struct jug_ini* jug_ini_load(char* filename){
 	if(loaded){
 		return & singleton_ini;
 	}

	if(!filename){
		debug("jug_ini : filename is null ");
		return NULL;
	}
	loaded=1;
	struct read_ini *ini_r = NULL;
	
	struct ini *ini = read_ini(&ini_r, filename);
	
	singleton_ini.iniparser=ini;
	return &singleton_ini;
}





//jug_ini_get_string()
char* jug_ini_get_string(struct jug_ini* ini_struct,char* section,char* key){
	if(!loaded){
		debugt("jug_ini","jug_ini structure not loaded");
		return NULL;
	}
	return ini_get_value(ini_struct->iniparser,section,key);
}






//jug_get_config()
char* jug_get_config(char* section,char* key){
	struct jug_ini* localini;
	if(!loaded){		
		char* jug_path=getenv("JUG_ROOT");
		char config_path[250];
		config_path[0]='\0';
		debugt("jug_path",jug_path);
		strcpy(config_path,jug_path);
		strcat(config_path,"/etc/config.ini");
		localini= jug_ini_load(config_path);

	}else localini=&singleton_ini;

	if(!localini){
		debugt("config","cannot obtain an ini strcutre");
		return NULL;
	}
	return jug_ini_get_string(localini,section,key);
}
