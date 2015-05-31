/****************************/
/***** testing purposes ****/
/**************************/

#include "ramfs.h"

const char* ramfs_root="/mnt";
const char* gcc_path="/usr/bin/gcc-4.6";
const char* gcc_label="gcc-4.6";


int main(int argc,char*argv[]){

	char root[200];strcpy(root,ramfs_root);
	ramfs_info*info=init_ramfs(root,20,100);

	if(argc>=2 ){
		//delete ramfs
		if(!strcmp(argv[1],"-d")){
			umount_ramfs(info);
			rm_ramfs_dir(info);
			return 0;
		}
		//is ramfs mounted
		if(!strcmp(argv[1],"-i")){
			printf("%s\n",ramfs_ismounted(info)?"mounted":"not mounted");
			return 0;
		}
		//state 
		if(!strcmp(argv[1],"state")){
			struct stat st;
			stat(info->directory,&st);
			printf("size dir:%d",(int)st.st_size);
			return 0;
		}
		//copy files
		if(!strcmp(argv[1],"-c")){
			if(argc>=4){
				char* src=argv[2];
				char* dst=argv[3];
				ramfs_cp(info,src,dst);
				print_ramfs_info(info);
				return 0;
			}else{
				printf("provide a source and a target\n");
				return -1;
			}
		}
	}
	//
	create_ramfs(info);
	print_ramfs_info(info);



	return 0;
}