#include "ramfs.h"



/**
*
*init the ramfs parameters
* max_size < size_alloc
*/
ramfs_info* init_ramfs(char* rootfs,int size_alloc,int max_size){
	if(max_size>size_alloc)
		max_size=size_alloc;
	if(max_size<0)
		max_size=0;

	ramfs_info* meminfo=(ramfs_info*)malloc(sizeof(ramfs_info));
	meminfo->root_dir=(char*)malloc(strlen(rootfs));
	strcpy(meminfo->root_dir,rootfs);
	meminfo->directory=get_ramfs_dir(meminfo);
	meminfo->size_alloc=size_alloc;
	meminfo->used=0;
	meminfo->mem_max=max_size;

	return meminfo;
}


// generate ramfs jug directory from ramfs_root
char* get_ramfs_dir(ramfs_info* info){
	if(!info->directory){
		char* ramfs_root=info->root_dir;
		char* ramfs_dir=(char*)malloc(strlen(ramfs_root)+10);
		strcpy(ramfs_dir,ramfs_root);
		strcat(ramfs_dir,"/jug");
		info->directory=ramfs_dir;
	}
	return info->directory;
}

//is mounted, guessing w safi (boolean return)
//check the device in which our ramfs directory resides, if it's ram we're mounted.
// this functioin is not exact. there is no linux solution to know if a dir is mounted;
//but in ou case we can check if the dir is not in hardisk, and it's enough.
int ramfs_ismounted(ramfs_info*info){
	struct stat st;
	if(stat(info->directory,&st)!=0){
		debugt("ramfs_ismounted","ramfs dir does not exist");
		return 0;
	}
	//stat the ramfs root, which is not a ramfs dir
	struct stat strt;
	if(stat(info->root_dir,&strt)!=0){
		debugt("ramfs_ismounted","weired ! cannot find our ramfs root_dir");
		return 0;
	}

	//here we check of major is different from the major of harddisk, or if it equales 0 ( I don't know why)
	int hdmjr=major(strt.st_dev);
	int mjr=major(st.st_dev);
	if(mjr!=hdmjr){
		return 1;
	}else{
		debugt("ramfs_ismounted","ramfs dir is likely unmounted");
		return 0;
	}
	//confirm
	return 1;
}

//unmount the ramfs
int umount_ramfs(ramfs_info*info){
	char* ramfs_root=info->root_dir;
	char* ramfs_dir=get_ramfs_dir(info);
	struct stat st={0};
	if(stat(ramfs_dir,&st)!=0){
		debug("ramfs_dir dir doesn't exist");
		return 0;
	}
	if(umount(ramfs_dir)!=0){
		if(errno==22){
			debugll("ramfs_dir is probably not mounted");
			return 0;
		}
		debugll("error unmounting ramfs");
		return -1;
	}
	debug("ramfs unmounted");
	return 0;
}


//remove the ramfs
int rm_ramfs_dir( ramfs_info*info){
	char* ramfs_root=info->root_dir;
	char* ramfs_dir=get_ramfs_dir(info);
	char cmd[200];
	sprintf(cmd,"rm -rf %s/%s",ramfs_root,"jug");
	if(system(cmd)==-1){
		debugll("error rm-ing ramfs_dir directory");
		return -1;
	}
	//success
	return 0;
}


//create ramfs in /tmp
//returns 0 on success
int create_ramfs( ramfs_info*info){
	char* ramfs_root=info->root_dir;
	int size_m=info->size_alloc;
	//
	char* ramfs_dir=get_ramfs_dir(info);
	struct stat st={0};
	if( stat(ramfs_dir,&st)==-1){
		if(mkdir(ramfs_dir,0700)!=0){
			debugll("cannot mkdir the ramfs dir");
			return -1;
		}
	}else{
		debug("ramfs dir already exists :/");
		return -2;//jug ramfs dir already exists
	}
	//success
	//mount a ram fs here 
	char mount_opts[100];
	sprintf(mount_opts,"size=%dm",size_m);
	if(mount("none",ramfs_dir,"ramfs",MS_MGC_VAL|MS_NOSUID|MS_NODEV|MS_NOATIME,mount_opts) !=0){
		debugl();
		return -3;
	}
	debug("ramfs mounted successfully");
	return 0;
		
}

/** complete cping
*ramfs_cp()
* copy regular files into ramfs directory.
* if you want to copy directories, implement another function
* +
*/
int ramfs_cp(ramfs_info*info,char* src,char* target){
	char* ramfs_dir=get_ramfs_dir(info);
	
	//check targer
	if(!target){
		target=(char*)malloc(5);
		strcpy(target,"");
	}

	//does source exist ?
	struct stat st={0};
	if(stat(src,&st)!=0){
		debugt("ramfs_cp","the source file doesn't exist");
		return -1;
	}
	//is a regular file
	//if a symlink follow to the origin and copy
	if(S_ISLNK(st.st_mode)){
		debugt("ramfs_cp","the given source is a symbolic link");
	}
	if(!S_ISREG(st.st_mode)){
		debugt("ramfs_cp","the given source is not a regular file");
		return -2;
	}

	//do we have enough space
	int src_size=st.st_size;
	int memfree=(info->mem_max-info->used);
	if( memfree < src_size) {
		debugt("ramfs_cp","ramfs full,cannot copy");
		return -3;
	}

	// char msg[200];
	// sprintf(msg,"ramfs free space : %d\n",)

}

/**
*print
*/
void print_ramfs_info(ramfs_info*info){
	printf("\n\n");
	printf("directory:\t\t%s%s\n",info->root_dir,"/jug");
	printf("mem allocated : \t %d\n",info->size_alloc);
	printf("mem max allowed:\t %d\n",info->mem_max);
	printf("mem used:\t\t %d\n",info->used);
}
