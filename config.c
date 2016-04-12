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


//
jug_ini_keyval* jug_ini_get_section(char* section) {
	struct jug_ini* localini;
	if(!loaded){
		char* jug_path=getenv("JUG_ROOT");
		char config_path[250];
		config_path[0]='\0';
		//debugt("jug_path",jug_path);
		strcpy(config_path,jug_path);
		strcat(config_path,"/etc/config.ini");
		localini= jug_ini_load(config_path);

	}else localini=&singleton_ini;

	if(!localini){
		debugt("config","cannot obtain an ini strcutre");
		return NULL;
	}
	///init linked list
	jug_ini_keyval* kv=NULL;
	jug_ini_keyval* tmp_kv=NULL;
	////////
	int s, c;
	struct ini* iniparser=localini->iniparser;
	for(s=0; s<iniparser->num_sections; s++)
	{
		if( strcmp(section, iniparser->sections[s]->name) == 0 ){
			for(c=0; c<iniparser->sections[s]->num_configs; c++){
				jug_ini_keyval* k=(jug_ini_keyval*)malloc(sizeof(jug_ini_keyval));
				k->key		=iniparser->sections[s]->configs[c]->key;
				k->value	=iniparser->sections[s]->configs[c]->value;
				k->next		= NULL;
				if(tmp_kv==NULL) {
					kv=k;
					tmp_kv=k;
				}
				else{
					tmp_kv->next=k;
					tmp_kv=tmp_kv->next;
				}

			};
		}

	}

	return kv;

}





//jug_get_config()
char* jug_get_config(char* section,char* key){
	struct jug_ini* localini;
	if(!loaded){		
		char* jug_path=getenv("JUG_ROOT");
		char config_path[250];
		config_path[0]='\0';
		//debugt("jug_path",jug_path);
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
