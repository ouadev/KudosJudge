#ifndef H_JUG_LANG
#define H_JUG_LANG
/*
 * lang.h
 * @date April 12 2016
 * @author ouadimjamal@gmail.com
 *
 * lang.h : definition of functions that manage the languages.
 *
 */
#include <stdlib.h>
#include <stdint.h>
#include "log.h"
#include "config.h"

typedef enum{LANG_COMPILED, LANG_INTERPRETED, LANG_VM} lang_type;

/**
 * Lang structure.
 * definition of a language
 */
typedef struct{
	lang_type type;
	char cmd_compile[100];
	char cmd_interpr[100];
	char cmd_vm[100];

	char id[20]; //C99
	char version[30];
	char label[80];

} Lang;


Lang g_languages[19]; //19 languages maximum
int g_languages_count;
/**
 * init the languages from config file.
 */
int lang_init();


void lang_print();


#endif
