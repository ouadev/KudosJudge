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
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].cmd_compile, p, q-p);
		//ext compile
		p=q+1;
		strcpy(g_languages[g_languages_count].ext_compile, p);
		//
		g_languages[g_languages_count].cmd_interpr[0]='\0';
		g_languages[g_languages_count].cmd_vm[0]='\0';
		g_languages[g_languages_count].ext_interpr[0]='\0';
		g_languages[g_languages_count].ext_vm[0]='\0';
		//iteration end
		tmp=tmp->next;
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
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].cmd_interpr, p, q-p);
		//ext_inter
		p=q+1;
		strcpy(g_languages[g_languages_count].ext_interpr, p);

		//
		g_languages[g_languages_count].cmd_compile[0]='\0';
		g_languages[g_languages_count].cmd_vm[0]='\0';
		g_languages[g_languages_count].ext_compile[0]='\0';
		g_languages[g_languages_count].ext_vm[0]='\0';

		tmp=tmp->next;
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
		//ext_compile
		p=q+1;
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].ext_compile, p, q-p);
		//cmd_vm
		p=q+1;
		q=strchr(p,',');
		strncpy(g_languages[g_languages_count].cmd_vm, p, q-p);
		//ext_vm
		p=q+1;
		strcpy(g_languages[g_languages_count].ext_vm, p);
		//
		g_languages[g_languages_count].cmd_interpr[0]='\0';
		g_languages[g_languages_count].ext_interpr[0]='\0';

		tmp=tmp->next;
		g_languages_count++;
	}

	//create a directory in global_ramfs
	ramfs_info* ramfs=get_global_ramfs();
	char * lang_workspace=(char*) malloc(sizeof(char)*(strlen(ramfs->path)+100));
	sprintf(lang_workspace, "%s/%s",ramfs->path,"lang-workspace");
	mode_t oldWorkspaceMask=umask(022);
	if(mkdir(lang_workspace, 0777)!=0 && errno!=EEXIST){
		debugt("lang"," cannot create languages workspace in ramfs directory");
		umask(oldWorkspaceMask);
		return -5;
	}
	umask(oldWorkspaceMask);
	g_lang_workspace=lang_workspace;
	//
	return 0;
}

//get_lang
Lang* lang_get(char* langid){
	int i;
	for(i=0;i<g_languages_count; i++){
		if ( strcmp(g_languages[i].id, langid)==0)
			return g_languages+i;
	}
	return NULL;
}

//lang_process
//, char*bin_cmd, char* output_exec
int lang_process(jug_submission* submission, char* langid, int worker_id){
	//init the worker directory in the languages workspace
	
	if(lang_workspace_inited[worker_id]==0){
		char* worker_workspace=(char*)malloc(sizeof(char)*(strlen(g_lang_workspace)+20));
		sprintf(worker_workspace, "%s/%d",g_lang_workspace,worker_id );
		mode_t oldWorkerMask=umask(0022);
		if(mkdir(worker_workspace, 0777) && errno!=EEXIST){
			debugt("lang","cannot create the lang workspace for the worker : %d", worker_id);
			free(worker_workspace);
			umask(oldWorkerMask);
			return -2;
		}
		umask(oldWorkerMask);
		lang_workspace_inited[worker_id]=1;
		free(worker_workspace);
	}
	

	//get lang
	Lang* lang=lang_get(langid);
	if(lang==NULL){
		//unfounf langid
		return -1;
	}
	const char* stype[3]={"COMPILED","INTERPRETED","VIRTUAL MACHINE"};
	debugt("lang-process","submission language : %s (%s)", lang->label, stype[lang->type]);
	//get text
	ramfs_info* ramfs=get_global_ramfs();
	char* text_filename=(char*)malloc(sizeof(char)*(strlen(ramfs->path)+50));
	char* output_filename=(char*)malloc(sizeof(char)*(strlen(ramfs->path)+50));
	char* compile_cmdline=(char*)malloc(sizeof(char)*(strlen(ramfs->path)+200));
	FILE* compile_file=NULL;
	struct stat st;
	//COMPILED & VM languages need a compiling layer
	if(lang->type==LANG_COMPILED  || lang->type==LANG_VM){
		//in file
		sprintf(text_filename,"%s/%d/Submission.%s", g_lang_workspace,worker_id,lang->ext_compile);
		debugt("lang-process", "text_filename ; %s", text_filename);
		umask(S_IWGRP|S_IRGRP|S_IWOTH|S_IROTH);
		compile_file=fopen(text_filename,"w+");
		if(compile_file==NULL){
			debugt("lang","cannot create a tmp C file, error:%s\n",strerror(errno));
			free(text_filename);
			free(output_filename);
			free(compile_cmdline);
			return -2;
		}
		int pp=fputs(submission->source,compile_file);
		if(pp<=0){
			debugt("lang","cannot write sourcecode to text_filename");
			free(text_filename);
			free(output_filename);
			free(compile_cmdline);
			fclose(compile_file);
			return -5;
		}
		fclose(compile_file);
		//out file
		if(lang->type==LANG_VM){//bytecode
			sprintf(output_filename,"%s/%d/Submission.%s",g_lang_workspace,worker_id, lang->ext_vm);
		}else{//just compiling directly to a binary
			sprintf(output_filename,"%s/%d/Submission",g_lang_workspace, worker_id);

		}
		//launch complining
		sprintf(compile_cmdline,lang->cmd_compile, text_filename, output_filename);

//		debugt("lang-process", "compile final cmd: %s", compile_cmdline);
		unlink(output_filename);
		system(compile_cmdline);

		//check if the file is generated : it may not be always true
		if(stat(output_filename,&st) != 0){
			debugt("lang","cannot find the generated file : %s",output_filename);
			free(text_filename);
			free(output_filename);
			free(compile_cmdline);
			return -3;
		}
		
		//else delete
		unlink(text_filename);
	}


	//construct the cmd_binary.
	//INTERPRETED languages need no processing
	if(lang->type==LANG_INTERPRETED){
		sprintf(output_filename, "%s/%d/Submission.%s", g_lang_workspace , worker_id, lang->ext_interpr);

	}

	submission->bin_cmd=(char*)malloc(300);
	submission->bin_path=(char*)malloc(300);
	if(lang->type==LANG_COMPILED){
		sprintf(submission->bin_cmd,"%s", output_filename);
		submission->interpreted=0;
	}else if( lang->type==LANG_VM){
		sprintf(submission->bin_cmd, lang->cmd_vm, output_filename );
		submission->interpreted=1;
	}else if (lang->type==LANG_INTERPRETED){
		sprintf(submission->bin_cmd, lang->cmd_interpr, output_filename);
		submission->interpreted=1;
	}
	//set the output filename
	strcpy(submission->bin_path, output_filename);
	//cleanup
	free(text_filename);
	free(output_filename);
	free(compile_cmdline);
	//fclose(compile_file);
	//
	return 0;


}

//lang_remove_binary
void lang_remove_binary(jug_submission submission){
	unlink(submission.bin_path);
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
