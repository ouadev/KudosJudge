#include "ramfs.h"



/**
*
*init the ramfs parameters
* max_size < size_alloc
*/
ramfs_info* init_ramfs(char* ramfs_root,char*ramfs_dirname,int size_alloc_mb,int max_size_mb){

	global_ramfs=NULL;
	//
	if(size_alloc_mb<0) size_alloc_mb=0;
	if(max_size_mb>size_alloc_mb)
		max_size_mb=size_alloc_mb;
	if(max_size_mb<0)
		max_size_mb=0;
	//meminfo
	ramfs_info* meminfo=(ramfs_info*)malloc(sizeof(ramfs_info));
	//copy ramfsdir
	meminfo->dirname=(char*)malloc(strlen(ramfs_dirname)+1);
	strcpy(meminfo->dirname,ramfs_dirname);
	//copy root
	meminfo->root_dir=(char*)malloc(strlen(ramfs_root)+1);
	strcpy(meminfo->root_dir,ramfs_root);
	//construct full path
	meminfo->path=(char*)malloc(strlen(ramfs_root)+strlen(ramfs_dirname)+5);
	strcpy(meminfo->path,meminfo->root_dir);
	strcat(meminfo->path,"/");
	strcat(meminfo->path,meminfo->dirname);
	//other params
	meminfo->size_alloc=size_alloc_mb*1000000;
	meminfo->used=0;
	meminfo->mem_max=max_size_mb*1000000;

	return meminfo;
}

//

void set_global_ramfs(ramfs_info* info){
	global_ramfs=info;
}

ramfs_info* get_global_ramfs(){
	return global_ramfs;
}


//is mounted, guessing w safi (boolean return)
//check the device in which our ramfs directory resides, if it's ram we're mounted.
// this functioin is not exact. there is no linux solution to know if a dir is mounted;
//but in ou case we can check if the dir is not in hardisk, and it's enough.
int ramfs_ismounted(ramfs_info*info){
	struct stat st;
	if(stat(info->path,&st)!=0){
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
	struct stat st={0};
	if(stat(info->path,&st)!=0){
		debug("ramfs_dir dir doesn't exist");
		return 0;
	}
	if(umount(info->path)!=0){
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

	char cmd[200];
	sprintf(cmd,"rm -rf %s",info->path);
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

	struct stat st={0};
	if( stat(info->path,&st)==-1){
		if(mkdir(info->path,0700)!=0){
			debugll("cannot mkdir the ramfs dir");
			return -1;
		}
	}else{
		return -2;//jug ramfs dir already exists
	}
	//success
	//mount a ram fs here
	char mount_opts[100];
	sprintf(mount_opts,"size=%dm",size_m);
	if(mount("none",info->path,"ramfs",MS_MGC_VAL|MS_NOSUID|MS_NODEV|MS_NOATIME,mount_opts) !=0){
		debugl();
		return -3;
	}
	debug("ramfs mounted successfully");
	return 0;
		
}

/** complete cping
* ramfs_cp()
* copy regular files into ramfs directory.
* if you want to copy directories, implement another function
* @param src the path to the file to be copied
* @param target relative path of the new file to the ramfs directory, /path/to/a/dir/newfilename, it's mandatory
* +
*/
int ramfs_cp(ramfs_info*info,char* src,char* target){
	

	struct stat st={0};
	struct stat st_target;
	char* full_target_dir;
	//does source exist ?
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
	
	//check target
	if(!target){
		debugt("ramfs_cp","the target path is null");
		return -3;
	}
	full_target_dir=(char*)malloc(strlen(info->path)+strlen(target)+5);
	sprintf(full_target_dir,"%s/%s",info->path,target);
	if(stat(full_target_dir,&st_target)==0){
		debugt("ramfs_cp"," a file exists with the same target name");
		return -11;
	}
	//do we have enough space ?
	int src_size=st.st_size;
	int memfree=(info->mem_max-info->used);
	if( memfree < src_size) {
		debugt("ramfs_cp","ramfs full,cannot copy");
		return -4;
	}

	//open src & target
	int src_fd,target_fd;
	target_fd=open(full_target_dir,O_CREAT|O_WRONLY,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
	if(target_fd<0){
		debugll("ramfs_cp cannot open target file");
		return -5;
	}
	src_fd=open(src,O_RDONLY);
	if(src_fd<0){
		debugll("ramfs_cp cannot open src file");
		return -6;
	}

	//do the cping
	int numrd;
	char buffer[RAMFS_CP_BUFFER_SIZE];
	while( (numrd=read(src_fd,buffer,RAMFS_CP_BUFFER_SIZE)) > 0 ){
		if(write(target_fd,buffer,numrd)!=numrd){
			debugt("ramfs_cp","error writing data to target file");
			return -7;
		}
	}
	if(numrd==-1){
		debugt("ramfs_cp","error reading data from source file");
		return -8;
	}
	//fix permissions
	//make sure owner has read & write , and group has read permissions
	mode_t src_mode=st.st_mode;
	fchmod(target_fd, src_mode|S_IRUSR|S_IWUSR|S_IRGRP );

	//update ramfs_info
	info->used+=src_size;
	//cleanup and exit
	if(close(src_fd)==-1){
		debugt("ramfs_cp","cannot close src file descriptor");
		return -9;
	}
	if(close(target_fd)==-1){
		debugt("ramfs_cp","cannot close target file descriptor");
		return -10;
	}
	//success
	return 0;

	//permissions transfer
	
	//


}

/**
*print
*/
void print_ramfs_info(ramfs_info*info){
	printf("\n");
	printf("directory:%s\n", info->path);
	printf("mem allocated : \t %d\n",info->size_alloc);
	printf("mem max allowed:\t %d\n",info->mem_max);
	printf("mem used:\t\t %d\n",info->used);
}
