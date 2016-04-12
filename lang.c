#include "lang.h"


//lang_init
int lang_init(){
	g_languages_count=0;
	jug_ini_keyval* ini_compilers=jug_ini_get_section("compilers");
	jug_ini_keyval* ini_interpreters=jug_ini_get_section("interpreters");
	jug_ini_keyval* ini_vms=jug_ini_get_section("virtual machines");

	char* p=NULL;
	char* q=NULL;
	jug_ini_keyval* tmp;
	//compilers
	tmp=ini_compilers;
	while(tmp!=NULL){
		g_languages[g_languages_count].type=LANG_COMPILED;
		strcpy(g_languages[g_languages_count].id,tmp->key);
		//version
		p=q=tmp->value;
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].version, p, q-p);
		//label
		p=q+1;
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].label, p, q-p);
		//cmd_compile
		p=q+1;
		strcpy(g_languages[g_languages_count].cmd_compile, p);
		tmp=tmp->next;
		//
		g_languages[g_languages_count].cmd_interpr[0]='\0';
		g_languages[g_languages_count].cmd_vm[0]='\0';
		g_languages_count++;
	}
	//	//interpreters
	tmp=ini_interpreters;
	while(tmp!=NULL){
		g_languages[g_languages_count].type=LANG_INTERPRETED;
		strcpy(g_languages[g_languages_count].id,tmp->key);
		//version
		p=q=tmp->value;
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].version, p, q-p);
		//label
		p=q+1;
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].label, p, q-p);
		//cmd_interpr
		p=q+1;
		strcpy(g_languages[g_languages_count].cmd_interpr, p);
		tmp=tmp->next;
		//
		g_languages[g_languages_count].cmd_compile[0]='\0';
		g_languages[g_languages_count].cmd_vm[0]='\0';
		g_languages_count++;
	}
	//	//Virtual machines
	tmp=ini_vms;
	while(tmp!=NULL){
		g_languages[g_languages_count].type=LANG_VM;
		strcpy(g_languages[g_languages_count].id,tmp->key);
		//version
		p=q=tmp->value;
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].version, p, q-p);
		//label
		p=q+1;
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].label, p, q-p);
		//cmd_compiler
		p=q+1;
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].cmd_compile, p, q-p);
		//cmd_vm
		p=q+1;
		strcpy(g_languages[g_languages_count].cmd_vm, p);
		tmp=tmp->next;
		//
		g_languages[g_languages_count].cmd_interpr[0]='\0';
		g_languages_count++;
	}

	return 0;
}


//lang_print
void lang_print(){
	int i;
	const char* stype[3]={"COMPILED","INTERPRETED","VIRTUAL MACHINE"};
	debugt("lang"," print supported languages (%d)", g_languages_count);
	for(i=0;i<g_languages_count;i++){
		debug("-------------------------");
		debug("%s (%s) : %s - [%s]", g_languages[i].id,g_languages[i].version, g_languages[i].label, stype[g_languages[i].type]);
		if(strlen(g_languages[i].cmd_compile)>0 )
			debug("compile: %s", g_languages[i].cmd_compile );
		if(strlen( g_languages[i].cmd_interpr)>0)
			debug("interpret: %s", g_languages[i].cmd_interpr );
		if(strlen(g_languages[i].cmd_vm)>0)
			debug("run in vm: %s", g_languages[i].cmd_vm );
		if(i==g_languages_count-1)
		debug("-------------------------");
	}
}
