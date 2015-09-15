#ifndef H_COMPILER
#define H_COMPILER

/***************************************************
* struct compiler
* holds information about a compiler/interpreter
****************************************************/

typedef struct compiler{
	/** label to define the compiler */
	char* name;
	/** version of the compiler */
	char* version;
	/** path of the compiler */
	char* path;
	/** path of all shared libraries the compiler needs, optional attribute */
	char** so_paths;
	/** the arguments */
	char *cmd_args;
	
} compiler;


	compiler compiler_new(char*name,char* version,char*path);

#endif