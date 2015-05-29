/****************************/
/***** testing purposes ****/
/**************************/

#include "ramfs.h"

const char* ramfs_root="/mnt";
const char* gcc_path="/usr/bin/gcc-4.6";
const char* gcc_label="gcc-4.6";


int main(int argc,char*argv[]){
	debug("~~~~~~~~~~~ouadim jamal~~~~~~~~~~~~~~");

	char root[200];strcpy(root,ramfs_root);
	ramfs_info*info=init_ramfs(root,10,100);

	if(argc==2 ){
		if(!strcmp(argv[1],"-d")){
			umount_ramfs(info);
			rm_ramfs_dir(info);
			return 0;
		}
		if(!strcmp(argv[1],"-i")){
			printf("%s\n",ramfs_ismounted(info)?"mounted":"not mounted");
			return 0;
		}
	}
	//
	create_ramfs(info);
	print_ramfs_info(info);



	return 0;
}