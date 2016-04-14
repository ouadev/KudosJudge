/*
 * lang.h
 * @date April 12 2016
 * @author ouadimjamal@gmail.com
 *
 * lang.h : definition of functions that manage the languages.
 * + manage supported languages (loading & initializing)
 * + for languages that support compiling, establish a layer to transform submissions
 *
 */
#ifndef H_JUG_LANG
#define H_JUG_LANG
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"
#include "config.h"
#include "ramfs.h"

#define  LANG_WORKERS_MAX_NBR 40
typedef enum{LANG_COMPILED, LANG_INTERPRETED, LANG_VM} lang_type;

/**
 * Lang structure.
 * definition of a language
 */
typedef struct{
	//general information
	lang_type type;
	char id[20]; //C99
	char version[30];
	char label[80];
	//command-line
	char cmd_compile[100];
	char cmd_interpr[100];
	char cmd_vm[100];
	//file extensions
	char ext_compile[20];
	char ext_interpr[20];
	char ext_vm[20];



} Lang;


Lang g_languages[19]; //19 languages maximum
int g_languages_count;
char* g_lang_workspace;
char lang_workspace_inited[LANG_WORKERS_MAX_NBR]; //keep track of workers that are already inited for languages processing
/**
 * init the languages from config file.
 */
int lang_init();
/**
 * lang_process()
 * this function takes the text of the submission and process it depending on the language
 * @param text the text of the user code
 * @param langid the id of the language to use
 * @param worker_id the id of the worker
 * @param bin_cmd the command to be executed to run the resulting binary (ie: python out.py)
 */
int lang_process(char* text, char* langid, int worker_id, char*bin_cmd);
/**
 * returns the Lang structure from an id
 * @return Lang structure, NULL if not found
 */
Lang* lang_get(char* langid);

void lang_print();


#endif
