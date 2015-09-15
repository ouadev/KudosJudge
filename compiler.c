#include "compiler.h"

compiler compiler_new(char*name,char* version,char*path){
	compiler nc;
	nc.name=name;
	nc.version=version;
	nc.path=path;
}