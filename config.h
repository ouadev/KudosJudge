#ifndef JUG_CONFIG
#define JUG_CONFIG
#include <stdlib.h>
#include "iniparser/iniparser.h"
#include "assert.h"
/**
* config.h 
* a set of functions to manage ini config files.
* Theses functions are just an API, and the implementation can be changed.
* right now a small library found in github is used to actually parse and read the config files.
* this library is iniparser, if a bug is found there, it's recommended to bring a new lib and reimplement
* the following functions.
* the procedure of usage is :
*	call jug_init_load() to load configs from an ini file.
*	call jug_ini_get_string to get a value for a key inside a section. 
* the last functino always returns a string. so you must always change this to your true type (ie atoi())
*/


struct jug_ini{
	/**< a structure from the lib we use*/
	struct ini* iniparser;
	/**< a flag to tell whether a config file is loaded*/
	int loaded;
};

/**
* @desc must be called before doing any operation
* @return the ini structure to use for further operations 
*
*/
int jug_ini_load(char* filename,struct jug_ini* ini);
/**
* @desc called to get a value from the jug_ini object. 
* @return the value of the given key inside the given section
*/

char* jug_ini_get_string(struct jug_ini* ini,char* section,char* key);

/**
* @desc same as jug_ini_get_string(), it returns an integer representation of the value
*
*/

int jug_ini_get_int(struct jug_ini* ini,char* section,char* key);

#endif